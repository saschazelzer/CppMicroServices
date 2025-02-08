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

#ifndef CPPMICROSERVICES_SCRIMPL_CONFIGURATIONNOTIFIER_HPP
#define CPPMICROSERVICES_SCRIMPL_CONFIGURATIONNOTIFIER_HPP

#include "../SCRLogger.hpp"
#include "ComponentFactoryImpl.hpp"
#include "ConcurrencyUtil.hpp"
#include "cppmicroservices/cm/ConfigurationListener.hpp"

namespace cppmicroservices
{
    namespace scrimpl
    {
        class ComponentConfigurationImpl;
        class SCRExtensionRegistry;

        /** ConfigChangeNotification
         * This class is used by ConfigurationListener to notify ComponentConfigurationImpl
         * about changes to Configuration Objects.
         */
        struct ConfigChangeNotification final
        {
            ConfigChangeNotification(std::string pid,
                                     std::shared_ptr<cppmicroservices::AnyMap> properties,
                                     cppmicroservices::service::cm::ConfigurationEventType evt,
                                     unsigned long changeCount)
                : pid(std::move(pid))
                , event(std::move(evt))
                , newProperties(std::move(properties))
                , newChangeCount(changeCount)
            {
            }

            std::string pid;
            cppmicroservices::service::cm::ConfigurationEventType event;
            std::shared_ptr<cppmicroservices::AnyMap> newProperties;
            unsigned long newChangeCount;
        };

        struct Listener final
        {
            Listener(std::function<void(ConfigChangeNotification const&)> notify,
                     std::shared_ptr<ComponentConfigurationImpl> mgr)
                : notify(std::move(notify))
                , mgr(std::move(mgr))
            {
            }

            std::function<void(ConfigChangeNotification const&)> notify;
            std::shared_ptr<ComponentConfigurationImpl> mgr;
        };

        class ConfigurationNotifier final
        {

          public:
            /**
             * @throws std::invalid_argument exception if any of the params is a nullptr or
             * if componentFactory object cannot be constructed.
             */
            ConfigurationNotifier(cppmicroservices::BundleContext const& context,
                                  std::shared_ptr<cppmicroservices::logservice::LogService> logger,
                                  std::shared_ptr<cppmicroservices::async::AsyncWorkService> asyncWorkSvc,
                                  std::shared_ptr<SCRExtensionRegistry> extensionReg);

            ConfigurationNotifier(ConfigurationNotifier const&) = delete;
            ConfigurationNotifier(ConfigurationNotifier&&) = delete;
            ConfigurationNotifier& operator=(ConfigurationNotifier const&) = delete;
            ConfigurationNotifier& operator=(ConfigurationNotifier&&) = delete;
            ~ConfigurationNotifier() = default;

            /**
             * @throws std::bad_alloc exception if memory cannot be allocated
             */
            cppmicroservices::ListenerTokenId RegisterListener(
                std::string const& pid,
                std::function<void(ConfigChangeNotification const&)> notify,
                std::shared_ptr<ComponentConfigurationImpl> mgr);

            void UnregisterListener(std::string const& pid, cppmicroservices::ListenerTokenId const token) noexcept;

            bool AnyListenersForPid(std::string const& pid, cppmicroservices::AnyMap const& properties) noexcept;

            void NotifyAllListeners(std::string const& pid,
                                    cppmicroservices::service::cm::ConfigurationEventType type,
                                    std::shared_ptr<cppmicroservices::AnyMap> properties,
                                    unsigned long const& changeCount);

            std::shared_ptr<ComponentFactoryImpl> GetComponentFactory();
            void LogInvalidDynamicTargetInProperties(cppmicroservices::AnyMap const& properties,
                                                     std::shared_ptr<ComponentConfigurationImpl> mgr) const noexcept;

          private:
            using TokenMap = std::unordered_map<ListenerTokenId, Listener>;

            cppmicroservices::scrimpl::Guarded<std::unordered_map<std::string, std::shared_ptr<TokenMap>>> listenersMap;

            std::atomic<cppmicroservices::ListenerTokenId> tokenCounter; ///< used to
                                                                         /// generate unique
                                                                         /// tokens for
                                                                         /// listeners

            std::shared_ptr<cppmicroservices::logservice::LogService> logger;
            std::shared_ptr<ComponentFactoryImpl> componentFactory;
        };

    } // namespace scrimpl
} // namespace cppmicroservices
#endif // CPPMICROSERVICES_SCRIMPL_CONFIGURATIONNOTIFIER_HPP
