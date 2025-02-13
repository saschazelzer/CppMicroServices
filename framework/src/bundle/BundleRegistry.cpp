/*=============================================================================

  Library: CppMicroServices

  Copyright (c) The CppMicroServices developers. See the COPYRIGHT
  file at the top-level directory of this distribution and at
  https://github.com/CppMicroServices/CppMicroServices/COPYRIGHT .

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=============================================================================*/

#include "BundleRegistry.h"

#include "cppmicroservices/Bundle.h"
#include "cppmicroservices/BundleActivator.h"
#include "cppmicroservices/BundleEvent.h"
#include "cppmicroservices/FrameworkEvent.h"
#include "cppmicroservices/GetBundleContext.h"

#include "cppmicroservices/util/Error.h"
#include "cppmicroservices/util/String.h"

#include "BundleContextPrivate.h"
#include "BundlePrivate.h"
#include "BundleResourceContainer.h"
#include "BundleStorage.h"
#include "CoreBundleContext.h"
#include "FrameworkPrivate.h"

#include <cassert>
#include <deque>
#include <functional>
#include <map>

namespace
{

    // Helper class to ensure RAII for InitialBundleMap in case where Install0 throws
    class InitialBundleMapCleanup
    {
      public:
        InitialBundleMapCleanup(std::function<void()> cleanupFcn) : _cleanupFcn(std::move(cleanupFcn)) {}
        ~InitialBundleMapCleanup() { _cleanupFcn(); }

      private:
        std::function<void()> _cleanupFcn;
    };

} // namespace

namespace cppmicroservices
{

    BundleRegistry::BundleRegistry(CoreBundleContext* coreCtx) : coreCtx(coreCtx) {}

    BundleRegistry::~BundleRegistry() = default;

    void
    BundleRegistry::Init()
    {
        bundles.v.insert(std::make_pair(coreCtx->systemBundle->location, coreCtx->systemBundle));
    }

    void
    BundleRegistry::Clear()
    {
        auto l = bundles.Lock();
        US_UNUSED(l);
        bundles.v.clear();
    }

    /*
      This function acquires a lock and decrements the reference count
      for the specified bundle. If this decrement causes the count to be
      0, it is erased from the initialBundleInstallMap map.
    */
    void
    BundleRegistry::DecrementInitialBundleMapRef(cppmicroservices::detail::MutexLockingStrategy<>::UniqueLock& l,
                                                 std::string const& location)
    {
        l.Lock();
        initialBundleInstallMap[location].first--;
        if (initialBundleInstallMap[location].first == 0)
        {
            initialBundleInstallMap.erase(location);
        }
        l.UnLock();
    }

    /*
      This function populates the res and alreadyInstalled vectors with the
      appropriate entries so that they can be used by the Install0 call. This was
      extracted from Install() for convenience. We lock the 'bundles' object to
      prevent any installs from writing to the map while this operation is occurring
      since the map can trigger a re-balancing of the tree nodes and cause some of
      the iterators to be incorrect.
    */
    std::shared_ptr<BundleResourceContainer>
    BundleRegistry::GetAlreadyInstalledBundlesAtLocation(
        std::pair<BundleMap::iterator, BundleMap::iterator> foundBundles,
        std::string const& location,
        cppmicroservices::AnyMap const& bundleManifest,
        std::vector<Bundle>& resultingBundles,
        std::vector<std::string>& alreadyInstalled)
    {
        auto l = bundles.Lock();
        US_UNUSED(l);
        // First, get a BundleResourceContainer to work with. Either create a new one (if one hasn't been
        // made yet for this location), or use one from another BundleArchive at this location.
        auto resourceContainer = (foundBundles.first == foundBundles.second
                                      ? std::make_shared<BundleResourceContainer>(location, bundleManifest)
                                      : foundBundles.first->second->GetBundleArchive()->GetResourceContainer());

        while (foundBundles.first != foundBundles.second)
        {
            auto installedBundlePrivate = foundBundles.first->second;
            alreadyInstalled.emplace_back(installedBundlePrivate->symbolicName);
            auto actualBundle
                = coreCtx->bundleHooks.FilterBundle(MakeBundleContext(installedBundlePrivate->bundleContext.Load()),
                                                    MakeBundle(installedBundlePrivate));
            if (actualBundle)
            {
                resultingBundles.push_back(actualBundle);
            }
            ++foundBundles.first;
        }
        return resourceContainer;
    }

    std::vector<Bundle>
    BundleRegistry::Install(std::string const& location, BundlePrivate*, cppmicroservices::AnyMap const& bundleManifest)
    {
        using namespace std::chrono_literals;

        CheckIllegalState();

        // Grab the lock for the BundleRegistry object so that we can
        // read into the map without any data races
        auto l = this->Lock();
        US_UNUSED(l);

        // Search the multimap for the current bundle location
        auto bundlesAtLocationRange = (bundles.Lock(), bundles.v.equal_range(location));

        /*
          If the bundle is already installed, then execute the regular
          install process. In this case, there are no data races to worry about.

          If the bundle isn't installed, then one of two things can happen: 1) either
          the current thread is the first thread trying to install this bundle or 2) the
          current thread is trying to install a bundle that is not installed but another
          thread is currently installing that bundle.

          If 1): Create an entry in the initialBundleInstallMap which keeps track of whether
            or not a given bundle is being installed for the first time. After creating this
            entry, the thread performs the install and notifies all threads waiting on that
            install to finish so that they too can perform an install (but in the context of it
            being already installed).

          If 2): Increment the reference counter for the bundle in the initialBundleInstallMap.
            This ensures that when we decrement the count after the installing thread is done,
            the map entry isn't deleted since the current thread needs access to the
            condition_variable and boolean flag. Once the installing thread notifies the thread
            that it is able to continue with it's install, it goes ahead and performs the regular
            install procedure.
        */
        if (bundlesAtLocationRange.first != bundlesAtLocationRange.second)
        {
            l.UnLock();
            std::vector<Bundle> resultingBundles;
            std::vector<std::string> alreadyInstalled;
            // Populate the resultingBundles and alreadyInstalled vectors with the appropriate data
            // based on what bundles are already installed
            auto resCont = GetAlreadyInstalledBundlesAtLocation(bundlesAtLocationRange,
                                                                location,
                                                                bundleManifest,
                                                                resultingBundles,
                                                                alreadyInstalled);

            // Perform the install
            auto newBundles = Install0(location, resCont, alreadyInstalled, bundleManifest);
            resultingBundles.insert(resultingBundles.end(), newBundles.begin(), newBundles.end());
            if (resultingBundles.empty())
            {
                throw std::runtime_error("All bundles rejected by a bundle hook");
            }
            return resultingBundles;
        }
        else
        {
            // Check to see if another thread is currently in the process of installing this
            // bundle for the first time.
            auto alreadyInstallingIterator = initialBundleInstallMap.find(location);

            /*
              If no other thread is installing the desired bundle, create a map entry,
              install the bundle, and notify all other threads waiting to install this bundle
              that it is safe to do so. If the current thread is not the installing thread, then
              it will increment the reference count in the initialBundleInstallMap so that the map
              entry isn't prematurely deleted, wait to be notified that the install thread is done,
              and then perform the regular install procedure.
            */
            if (alreadyInstallingIterator == initialBundleInstallMap.end())
            {
                // Insert entry into the initialBundleInstallMap to prevent other threads from
                // trying to install this bundle simultaneously
                auto pairToInsert = std::make_pair(uint32_t(1), WaitCondition {});
                initialBundleInstallMap.insert(std::make_pair(location, std::move(pairToInsert)));
                l.UnLock();

                std::vector<Bundle> installedBundles;
                {
                    // create instance of clean-up object to ensure RAII
                    InitialBundleMapCleanup cleanup(
                        [this, &l, &location]()
                        {
                            {
                                l.Lock();
                                auto& p = initialBundleInstallMap[location];
                                // Notify all waiting threads that it is safe to install the bundle
                                std::lock_guard<std::mutex> lock(*(p.second.m));
                                p.second.waitFlag = false;
                                p.second.cv->notify_all();
                                l.UnLock();
                            }
                            DecrementInitialBundleMapRef(l, location);
                        });

                    // Perform the install
                    auto resCont = std::make_shared<BundleResourceContainer>(location, bundleManifest);
                    installedBundles = Install0(location, resCont, {}, bundleManifest);
                }
                return installedBundles;
            }
            else
            {
                initialBundleInstallMap[location].first++;
                {
                    // Wait for the install thread to notify this thread that it is safe
                    // to install the current bundle
                    auto& p = initialBundleInstallMap[location];
                    l.UnLock();
                    std::unique_lock<std::mutex> lock(*(p.second.m));

                    // This while loop exists to prevent a known race condition. If the installing thread notifies
                    // before another thread trying to install the same bundle reaches this wait, it will wait
                    // indefinitely. To fix this, a wait_for is used instead (which utilizes a timeout to avoid this
                    // race) and the wait statement as a whole acts as the while statement's predicate; once the timeout
                    // is reached, wait_for exits and the statement is re-evaluated since it would have returned false.
                    while (!p.second.cv->wait_for(lock, 0.1ms, [&p] { return !p.second.waitFlag; }))
                        ;
                }

                // Re-acquire the range because while this thread was waiting, the installing
                // thread made a modification to bundles.v
                l.Lock();
                bundlesAtLocationRange = (bundles.Lock(), bundles.v.equal_range(location));
                l.UnLock();

                std::vector<Bundle> resultingBundles;
                std::vector<std::string> alreadyInstalled;
                auto resCont = GetAlreadyInstalledBundlesAtLocation(bundlesAtLocationRange,
                                                                    location,
                                                                    bundleManifest,
                                                                    resultingBundles,
                                                                    alreadyInstalled);

                std::vector<Bundle> newBundles;
                {
                    // create instance of clean-up object to ensure RAII
                    InitialBundleMapCleanup cleanup([this, &l, &location]()
                                                    { DecrementInitialBundleMapRef(l, location); });

                    // Perform the install
                    newBundles = Install0(location, resCont, alreadyInstalled, bundleManifest);
                }

                resultingBundles.insert(resultingBundles.end(), newBundles.begin(), newBundles.end());
                if (resultingBundles.empty())
                {
                    throw std::runtime_error("All bundles rejected by a bundle hook");
                }
                return resultingBundles;
            }
        }
    }

    std::vector<Bundle>
    BundleRegistry::Install0(std::string const& location,
                             std::shared_ptr<BundleResourceContainer> const& resCont,
                             std::vector<std::string> const& alreadyInstalled,
                             cppmicroservices::AnyMap const& bundleManifest)
    {
        namespace cppms = cppmicroservices;
        using cppms::any_cast;
        using cppms::any_map;
        using cppms::AnyMap;

        std::vector<Bundle> installedBundles;
        std::vector<std::shared_ptr<BundleArchive>> barchives;
        std::unordered_set<std::string> exclude { alreadyInstalled.begin(), alreadyInstalled.end() };
        try
        {
            // Create a BundleArchive for each entry in the resource container... that is, top level entries
            // (symbolic names) in the zip file for the bundle at 'location'.
            //
            // Note: We MUST use the values from "GetTopLevelDirs()" because eventhough the keys in the
            // "bundleManifest" are also the "SymbolicNames", bundleManifest can be empty and will only
            // contain the symbolicName entries if it's not.
            auto entries = resCont->GetTopLevelDirs();
            for (auto const& symbolicName : entries)
            {
                // only install non-excluded entries
                if (0 == exclude.count(symbolicName))
                {
#ifndef US_BUILD_SHARED_LIBS
                    // The system bundle is already installed, so skip it.
                    if (Constants::SYSTEM_BUNDLE_SYMBOLICNAME == symbolicName)
                    {
                        continue;
                    }
#endif

                    // Either use the manifest found in the passed in bundleManifest list for the current entry,
                    // or construct an empty one
                    auto manifest = (0 != bundleManifest.count(symbolicName)
                                         ? any_cast<AnyMap>(bundleManifest.at(symbolicName))
                                         : AnyMap(any_map::UNORDERED_MAP_CASEINSENSITIVE_KEYS));

                    // Now, create a BundleArchive with the given manifest at 'entry' in the
                    // BundleResourceContainer, and remember the created BundleArchive here for later
                    // processing, including purging any items created if an exception is thrown.
                    barchives.push_back(coreCtx->storage->CreateAndInsertArchive(resCont, symbolicName, manifest));
                }
            }

            // Now, create a BundlePrivate for each BundleArchive, and then add a Bundle to the results
            // that are returned, one for each BundlePrivate that's created.
            for (auto const& ba : barchives)
            {
                try
                {
                    auto d = std::make_shared<BundlePrivate>(coreCtx, ba);
                    installedBundles.emplace_back(MakeBundle(d));
                }
                catch(const std::runtime_error& re)
                {
                    // speciffic handling for runtime_error
                    std::cerr << "Runtime error: " << re.what() << std::endl;
                }
                catch(const std::exception& ex)
                {
                    // speciffic handling for all exceptions extending std::exception, except
                    // std::runtime_error which is handled explicitly
                    std::cerr << "Error occurred: " << ex.what() << std::endl;
                }
            }

            // For each bundle that we created, add into the map of location->bundle, completing the
            // addition of the bundle into the registry with the given manifest.
            {
                auto l = bundles.Lock();
                US_UNUSED(l);
                for (auto& b : installedBundles)
                {
                    bundles.v.insert(std::make_pair(location, b.d));
                }
            }

            // Now fire off the bundle event listeners.
            for (auto& b : installedBundles)
            {
                coreCtx->listeners.BundleChanged(BundleEvent(BundleEvent::BUNDLE_INSTALLED, b));
            }
        }
        catch (...)
        {
            // catch any other errors (that we have no information about)
            std::cerr << "Failed to install bundle library at " + location + ": " + util::GetLastExceptionStr() << std::endl;

            for (auto& ba : barchives)
            {
                ba->Purge();
            }

            throw std::runtime_error("Failed to install bundle library at " + location + ": "
                                     + util::GetLastExceptionStr());
        }
        // And finally return the results.
        return installedBundles;
    }

    void
    BundleRegistry::Remove(std::string const& location, long id)
    {
        auto l = bundles.Lock();
        US_UNUSED(l);
        auto range = bundles.v.equal_range(location);
        for (auto iter = range.first; iter != range.second; ++iter)
        {
            if (iter->second->id == id)
            {
                bundles.v.erase(iter);
                return;
            }
        }
    }

    std::shared_ptr<BundlePrivate>
    BundleRegistry::GetBundle(long id) const
    {
        CheckIllegalState();

        auto l = bundles.Lock();
        US_UNUSED(l);

        for (auto& m : bundles.v)
        {
            if (m.second->id == id)
            {
                return m.second;
            }
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<BundlePrivate>>
    BundleRegistry::GetBundles(std::string const& location) const
    {
        CheckIllegalState();

        auto l = bundles.Lock();
        US_UNUSED(l);

        auto range = bundles.v.equal_range(location);
        std::vector<std::shared_ptr<BundlePrivate>> result;
        std::transform(range.first,
                       range.second,
                       std::back_inserter(result),
                       [](BundleMap::value_type const& p) { return p.second; });
        return result;
    }

    std::vector<std::shared_ptr<BundlePrivate>>
    BundleRegistry::GetBundles(std::string const& name, BundleVersion const& version) const
    {
        CheckIllegalState();

        std::vector<std::shared_ptr<BundlePrivate>> res;

        auto l = bundles.Lock();
        US_UNUSED(l);

        for (auto& p : bundles.v)
        {
            auto& b = p.second;
            if (name == b->symbolicName && version == b->version)
            {
                res.push_back(b);
            }
        }

        return res;
    }

    std::vector<std::shared_ptr<BundlePrivate>>
    BundleRegistry::GetBundles() const
    {
        auto l = bundles.Lock();
        US_UNUSED(l);

        std::vector<std::shared_ptr<BundlePrivate>> result;
        std::transform(bundles.v.begin(),
                       bundles.v.end(),
                       std::back_inserter(result),
                       [](BundleMap::value_type const& p) { return p.second; });
        return result;
    }

    std::vector<std::shared_ptr<BundlePrivate>>
    BundleRegistry::GetActiveBundles() const
    {
        CheckIllegalState();
        std::vector<std::shared_ptr<BundlePrivate>> result;

        auto l = bundles.Lock();
        US_UNUSED(l);
        for (auto& b : bundles.v)
        {
            auto s = b.second->state.load();
            if (s == Bundle::STATE_ACTIVE || s == Bundle::STATE_STARTING)
            {
                result.push_back(b.second);
            }
        }
        return result;
    }

    void
    BundleRegistry::Load()
    {
        auto l = this->Lock();
        US_UNUSED(l);
        auto bas = coreCtx->storage->GetAllBundleArchives();
        for (auto const& ba : bas)
        {
            try
            {
                auto impl = std::make_shared<BundlePrivate>(coreCtx, ba);
                bundles.v.insert(std::make_pair(impl->location, impl));
            }
            catch (...)
            {
                ba->SetAutostartSetting(-1); // Do not start on launch
                std::cerr << "Failed to load bundle " << util::ToString(ba->GetBundleId())
                          << " (" + ba->GetBundleLocation() + ") uninstalled it!"
                          << " (exception: " << util::GetExceptionStr(std::current_exception()) << ")" << std::endl;
            }
        }
    }

    void
    BundleRegistry::CheckIllegalState() const
    {
        if (coreCtx == nullptr)
        {
            throw std::logic_error("This framework instance is not active.");
        }
    }

} // namespace cppmicroservices
