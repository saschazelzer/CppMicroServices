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

#include "cppmicroservices/Bundle.h"
#include "cppmicroservices/BundleInitialization.h"
#include "cppmicroservices/Constants.h"
#include "cppmicroservices/SharedLibrary.h"
#include "cppmicroservices/SharedLibraryException.h"
#include "cppmicroservices/util/BundleHandles.h"

#include "BundleLoader.hpp"
#include <regex>
#if defined(_WIN32)
#    include <Windows.h>
#else
#    include <cerrno>
#    include <dlfcn.h>
#endif

namespace cppmicroservices
{
    namespace scrimpl
    {
        namespace
        {
            /**
             * @brief Convert parameter to string
             *
             * @tparam T type of the value to convert to a string
             * @param val value to convert to a string
             * @return std::string the value converted to a string
             */
            template <typename T>
            std::string
            ToString(T val)
            {
#if defined(__ANDROID__)
                std::ostringstream os;
                os << val;
                return os.str();
#else
                return std::to_string(val);
#endif
            }
        } // namespace

#if defined(_WIN32)
        /**
         * Utility function to convert UTF8 string to UTF16 string. This method is copied from
         * the CppMicroServices framework codebase(util/src/String.cpp).
         */
        std::wstring
        UTF8StrToWStr(std::string const& inStr)
        {
            if (inStr.empty())
            {
                return std::wstring();
            }
            int wchar_count = MultiByteToWideChar(CP_UTF8, 0, inStr.c_str(), -1, NULL, 0);
            std::unique_ptr<wchar_t[]> wBuf(new wchar_t[wchar_count]);
            wchar_count = MultiByteToWideChar(CP_UTF8, 0, inStr.c_str(), -1, wBuf.get(), wchar_count);
            if (wchar_count == 0)
            {
                throw std::invalid_argument("Failed to convert " + inStr + " to UTF16.");
            }
            return wBuf.get();
        }
#endif

        std::tuple<std::function<ComponentInstance*(void)>, std::function<void(ComponentInstance*)>>
        GetComponentCreatorDeletors(std::string const& compName,
                                    cppmicroservices::Bundle const& fromBundle,
                                    std::shared_ptr<cppmicroservices::logservice::LogService> const& logger)
        {
            // cannot use bundle id as key because id is reused when the framework is restarted.
            // strings are not optimal but will work fine as long as a binary is not unloaded
            // from the process.
            static Guarded<std::map<std::string, void*>> bundleBinaries; ///< map of bundle location and handle pairs
            auto const bundleLoc = fromBundle.GetLocation();

            void* handle = nullptr;
            if (bundleBinaries.lock()->count(bundleLoc) != 0u)
            {
                handle = bundleBinaries.lock()->at(bundleLoc);
            }
            else
            {
                SharedLibrary sh(bundleLoc);
                try
                {
                    auto ctx = fromBundle.GetBundleContext();
                    auto opts = ctx.GetProperty(Constants::LIBRARY_LOAD_OPTIONS);
                    logger->Log(logservice::SeverityLevel::LOG_INFO,
                                "Loading shared library for Bundle #" + ToString(fromBundle.GetBundleId())
                                    + " (location=" + bundleLoc + ")");

                    sh.Load(any_cast<int>(opts));

                    // Message timestamps are used for approximately measuring the time it takes to load the library file
                    logger->Log(logservice::SeverityLevel::LOG_INFO,
                                "Finished loading shared library for Bundle #" + ToString(fromBundle.GetBundleId())
                                    + " (location=" + bundleLoc + ")");

                    std::string set_bundle_context_func = US_STR(US_SET_CTX_PREFIX) + fromBundle.GetSymbolicName();
                    SetBundleContextHook hook;
                    std::string set_bundle_context_err;
                    util::GetSymbol(hook, sh.GetHandle(), set_bundle_context_func, &set_bundle_context_err);
                    if (hook)
                    {
                        BundleContext* ctx_copy = new BundleContext(fromBundle.GetBundleContext());
                        hook(ctx_copy);
                    }
                    else
                    {
                        logger->Log(logservice::SeverityLevel::LOG_WARNING, set_bundle_context_err);
                    }

                }
                catch (std::system_error const& ex)
                {
                    logger->Log(logservice::SeverityLevel::LOG_INFO,
                                "Failed loading shared library for Bundle #" + ToString(fromBundle.GetBundleId())
                                    + " (location=" + bundleLoc + ")",
                                std::make_exception_ptr(ex));
                    // SharedLibrary::Load() will throw a std::system_error when a shared library
                    // fails to load. Creating a SharedLibraryException here to throw with fromBundle information.
                    throw cppmicroservices::SharedLibraryException(ex.code(), ex.what(), fromBundle);
                }
                handle = sh.GetHandle();
                bundleBinaries.lock()->emplace(bundleLoc, handle);
            }

            const std::string symbolName = std::regex_replace(compName, std::regex("::"), "_");
            const std::string newInstanceFuncName("NewInstance_" + symbolName);
            const std::string deleteInstanceFuncName("DeleteInstance_" + symbolName);

            void* newsym = fromBundle.GetSymbol(handle, newInstanceFuncName);
            void* delsym = fromBundle.GetSymbol(handle, deleteInstanceFuncName);

            if (newsym == nullptr || delsym == nullptr)
            {
                std::string errMsg("Unable to find entry-point functions in bundle ");
                errMsg += fromBundle.GetLocation();
#if defined(_WIN32)
                errMsg += ". Error code: ";
                errMsg += std::to_string(GetLastError());
#else
                errMsg += ". Error: ";
                char const* dlErrMsg = dlerror();
                errMsg += (dlErrMsg) ? dlErrMsg : "none";
#endif
                throw std::runtime_error(errMsg);
            }

            return std::make_tuple(reinterpret_cast<ComponentInstance* (*)(void)>(newsym),  // NOLINT
                                   reinterpret_cast<void (*)(ComponentInstance*)>(delsym)); // NOLINT
        }
    } // namespace scrimpl
} // namespace cppmicroservices
