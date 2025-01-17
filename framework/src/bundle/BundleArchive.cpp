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

#include "BundleArchive.h"

#include <utility>

#include "cppmicroservices/BundleResource.h"
#include "cppmicroservices/BundleResourceStream.h"
#include "cppmicroservices/util/Error.h"

#include "BundleManifest.h"
#include "BundleResourceContainer.h"
#include "BundleStorage.h"

#include "Utils.h"

namespace cppmicroservices
{

    namespace
    {

        int64_t
        now()
        {
            namespace sc = std::chrono;
            return sc::duration_cast<sc::milliseconds>(sc::steady_clock::now().time_since_epoch()).count();
        }

    } // namespace

    const std::string BundleArchive::AUTOSTART_SETTING_STOPPED = "stopped";
    const std::string BundleArchive::AUTOSTART_SETTING_EAGER = "eager";
    const std::string BundleArchive::AUTOSTART_SETTING_ACTIVATION_POLICY = "activation_policy";

    BundleArchive::BundleArchive()
        : storage(nullptr)
        , bundleId(0)
        , lastModified(now())
        , autostartSetting(-1)
        , manifest(any_map::UNORDERED_MAP_CASEINSENSITIVE_KEYS)
    {
    }

    BundleArchive::BundleArchive(BundleStorage* storage,
                                 std::shared_ptr<BundleResourceContainer> resourceContainer,
                                 std::string prefix,
                                 std::string location,
                                 long bundleId,
                                 AnyMap bundleManifest)
        : storage(storage)
        , resourceContainer(std::move(resourceContainer))
        , resourcePrefix(std::move(prefix))
        , location(std::move(location))
        , bundleId(bundleId)
        , lastModified(now())
        , autostartSetting(-1)
        , manifest(std::move(bundleManifest))
    {
    }

    bool
    BundleArchive::IsValid() const
    {
        return bool(bundleId >= 0);
    }

    void
    BundleArchive::Purge()
    {
        storage->RemoveArchive(this);
    }

    long
    BundleArchive::GetBundleId() const
    {
        return bundleId;
    }

    std::string
    BundleArchive::GetBundleLocation() const
    {
        return location;
    }

    std::string
    BundleArchive::GetResourcePrefix() const
    {
        return resourcePrefix;
    }

    BundleResource
    BundleArchive::GetResource(std::string const& path, bool bypassPrefix) const
    {
        if (!resourceContainer)
        {
            return BundleResource();
        }
        BundleResource result(path, this->shared_from_this(), bypassPrefix);
        if (result)
            return result;
        return BundleResource();
    }

    std::vector<BundleResource>
    BundleArchive::FindResources(std::string const& path, std::string const& filePattern, bool recurse) const
    {
        std::vector<BundleResource> result;
        if (!resourceContainer)
        {
            return result;
        }

        std::string normalizedPath = path;
        // add a leading and trailing slash
        if (normalizedPath.empty())
            normalizedPath.push_back('/');
        if (*normalizedPath.begin() != '/')
            normalizedPath = '/' + normalizedPath;
        if (*normalizedPath.rbegin() != '/')
            normalizedPath.push_back('/');
        resourceContainer->FindNodes(this->shared_from_this(),
                                     resourcePrefix + normalizedPath,
                                     filePattern.empty() ? "*" : filePattern,
                                     recurse,
                                     result);
        return result;
    }

    BundleArchive::TimeStamp
    BundleArchive::GetLastModified() const
    {
        return TimeStamp { std::chrono::milliseconds(lastModified) };
    }

    void
    BundleArchive::SetLastModified(TimeStamp const& ts)
    {
        namespace sc = std::chrono;

        lastModified = sc::duration_cast<sc::milliseconds>(ts.time_since_epoch()).count();
    }

    int32_t
    BundleArchive::GetAutostartSetting() const
    {
        return autostartSetting;
    }

    void
    BundleArchive::SetAutostartSetting(int32_t setting)
    {
        autostartSetting = setting;
    }

    std::shared_ptr<BundleResourceContainer>
    BundleArchive::GetResourceContainer() const
    {
        return resourceContainer;
    }

    AnyMap const&
    BundleArchive::GetInjectedManifest() const
    {
        return manifest;
    }

} // namespace cppmicroservices
