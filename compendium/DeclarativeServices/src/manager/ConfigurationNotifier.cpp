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

#include "ConfigurationNotifier.hpp"
#include "../ComponentRegistry.hpp"
#include "../metadata/ComponentMetadata.hpp"
#include "ComponentConfigurationImpl.hpp"
#include "ComponentManagerImpl.hpp"
#include "cppmicroservices/SecurityException.h"
#include "cppmicroservices/SharedLibraryException.h"
#include "cppmicroservices/asyncworkservice/AsyncWorkService.hpp"
#include "cppmicroservices/cm/ConfigurationAdmin.hpp"
#include "../SCRExtensionRegistry.hpp"

namespace cppmicroservices
{
    namespace scrimpl
    {
        using cppmicroservices::scrimpl::metadata::ComponentMetadata;

        ConfigurationNotifier::ConfigurationNotifier(
            cppmicroservices::BundleContext const& context,
            std::shared_ptr<cppmicroservices::logservice::LogService> logger,
            std::shared_ptr<cppmicroservices::async::AsyncWorkService> asyncWorkSvc,
            std::shared_ptr<SCRExtensionRegistry> extensionReg)
            : tokenCounter(0)
            , logger(logger)
            , componentFactory(std::make_shared<ComponentFactoryImpl>(context, logger, asyncWorkSvc, extensionReg))
 
          {
            if (!context || !(this->logger) || !asyncWorkSvc || !extensionReg || !componentFactory)
            {
                throw std::invalid_argument("ConfigurationNotifier Constructor "
                                            "provided with invalid arguments");
            }
         }

        cppmicroservices::ListenerTokenId
        ConfigurationNotifier::RegisterListener(std::string const& pid,
                                                std::function<void(ConfigChangeNotification const&)> notify,
                                                std::shared_ptr<ComponentConfigurationImpl> mgr)
        {
            cppmicroservices::ListenerTokenId retToken = ++tokenCounter;

            {
                auto listener = Listener(notify, mgr);

                auto listenersMapHandle = listenersMap.lock();
                auto iter = listenersMapHandle->find(pid);
                if (iter != listenersMapHandle->end())
                {
                    (*(iter->second)).emplace(retToken, listener);
                }
                else
                {
                    auto tokenMapPtr = std::make_shared<TokenMap>();
                    tokenMapPtr->emplace(retToken, listener);
                    listenersMapHandle->emplace(pid, tokenMapPtr);
                }
            }

            return retToken;
        }

        void
        ConfigurationNotifier::UnregisterListener(std::string const& pid,
                                                  const cppmicroservices::ListenerTokenId token) noexcept
        {
            auto listenersMapHandle = listenersMap.lock();
            if (listenersMapHandle->empty() || pid.empty())
            {
                return;
            }

            auto iter = listenersMapHandle->find(pid);
            if (iter == listenersMapHandle->end())
            {
                return;
            }

            auto tokenMapPtr = iter->second;
            for (auto const& tokenMap : (*tokenMapPtr))
            {
                if (tokenMap.first == token)
                {
                    tokenMapPtr->erase(tokenMap.first);
                    if (tokenMapPtr->size() == 0)
                    {
                        listenersMapHandle->erase(iter);
                    }
                    break;
                }
            }
        }
        bool
        ConfigurationNotifier::AnyListenersForPid(std::string const& pid, cppmicroservices::AnyMap const& properties) noexcept
        {
            std::string factoryName;
            std::vector<std::shared_ptr<ComponentConfigurationImpl>> mgrs;
            {
                auto listenersMapHandle = listenersMap.lock();
                if (listenersMapHandle->empty() || pid.empty())
                {
                    return false;
                }

                auto iter = listenersMapHandle->find(pid);
                if (iter != listenersMapHandle->end())
                {
                    return true;
                }

                // The exact pid isn't present. See if this is a factory pid.
                auto position = pid.find('~');
                if (position == std::string::npos)
                {
                    return false;
                }
                // This is a factoryPid with format factoryComponentName~instanceName.
                // See if we're listening for changes to this factoryComponentName
                factoryName = pid.substr(0, position);
                if (factoryName.empty())
                {
                    return false;
                }
                iter = listenersMapHandle->find(factoryName);
                if (iter == listenersMapHandle->end())
                {
                    return false;
                }
                auto tokenMapPtr = iter->second;
                for (auto const& tokenEntry : (*tokenMapPtr))
                {
                    auto listener = tokenEntry.second;

                    if (!listener.mgr->GetMetadata()->factoryComponentID.empty())
                    {
                        // The component in our listeners map is a factory component.
                        mgrs.emplace_back(listener.mgr);
                    }
                }
                if (mgrs.empty())
                {
                    // None of the components in our listeners map is a factory component.
                    return false;
                }
            } // release listenersMapHandle lock

            for (auto & mgr : mgrs)
            {
            componentFactory->CreateFactoryComponent(pid, mgr, properties);
            }
            return true;
        }
        

        void
        ConfigurationNotifier::NotifyAllListeners(std::string const& pid,
                                                  cppmicroservices::service::cm::ConfigurationEventType type,
                                                  std::shared_ptr<cppmicroservices::AnyMap> properties)
        {
            ConfigChangeNotification notification
                = ConfigChangeNotification(pid, std::move(properties), std::move(type));

            auto listenersMapHandle = listenersMap.lock();
            auto iter = listenersMapHandle->find(pid);
            if (iter != listenersMapHandle->end())
            {
                for (auto const& configListenerPtr : *(iter->second))
                {
                    configListenerPtr.second.notify(notification);
                }
            }
            else
            {
                return;
            }
        }
        std::shared_ptr<ComponentFactoryImpl>  ConfigurationNotifier::GetComponentFactory() {
            return componentFactory;
        }

    } // namespace scrimpl
} // namespace cppmicroservices
