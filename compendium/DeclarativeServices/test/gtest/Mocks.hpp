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

#ifndef __MOCKS_HPP__
#define __MOCKS_HPP__
#include "../../src/ComponentRegistry.hpp"
#include "../../src/SCRAsyncWorkService.hpp"
#include "../../src/manager/ComponentConfiguration.hpp"
#include "../../src/manager/ComponentConfigurationImpl.hpp"
#include "../../src/manager/ComponentManager.hpp"
#include "../../src/manager/ComponentManagerImpl.hpp"
#include "../../src/manager/ReferenceManager.hpp"
#include "../../src/manager/ReferenceManagerImpl.hpp"
#include "../../src/manager/states/ComponentConfigurationState.hpp"
#include "../../src/manager/states/ComponentManagerState.hpp"
#include "gmock/gmock.h"
#include <cppmicroservices/ServiceFactory.h>
#include <cppmicroservices/asyncworkservice/AsyncWorkService.hpp>

namespace cppmicroservices
{
    namespace scrimpl
    {
        namespace metadata
        {
            inline std::ostream&
            operator<<(std::ostream& os, ComponentMetadata const& metadata)
            {
                os << "ComponentMetadata[name = " << metadata.name << "]" << std::endl
                   << "\tenabled: " << metadata.enabled << std::endl
                   << "\timmediate: " << metadata.immediate << std::endl
                   << "\timplClassName: " << metadata.implClassName << std::endl
                   << "\tactivateMethodName: " << metadata.activateMethodName << std::endl
                   << "\tdeactivateMethodName: " << metadata.deactivateMethodName << std::endl
                   << "\tmodifiedMethodName: " << metadata.modifiedMethodName << std::endl;

                os << "\trefsMetaData[size = " << metadata.refsMetadata.size() << "]: [" << std::endl;
                for (auto const& rMeta : metadata.refsMetadata)
                {
                    os << "\t\tReferenceMetadata[name = " << rMeta.name << "]" << std::endl
                       << "\t\t\ttarget: " << rMeta.target << std::endl
                       << "\t\t\tinterfaceName: " << rMeta.interfaceName << std::endl
                       << "\t\t\tcardinality: " << rMeta.cardinality << std::endl
                       << "\t\t\tpolicy: " << rMeta.policy << std::endl
                       << "\t\t\tpolicyOption: " << rMeta.policyOption << std::endl
                       << "\t\t\tscope: " << rMeta.scope << std::endl
                       << "\t\t\tminCardinality: " << rMeta.minCardinality << std::endl
                       << "\t\t\tmaxCardinality: " << rMeta.maxCardinality << std::endl;
                }
                os << "\t]" << std::endl;

                os << "\tserviceMetadata:" << std::endl
                   << "\t\tServiceMetadata: [" << std::endl
                   << "\t\t\tinterfaces[size = " << metadata.serviceMetadata.interfaces.size() << "]: [" << std::endl;
                for (auto const& interface : metadata.serviceMetadata.interfaces)
                {
                    bool isLast = interface != metadata.serviceMetadata.interfaces.back();
                    os << "\t\t\t\t" << interface << (isLast ? "" : ",") << std::endl;
                }
                os << "\t\t\t]," << std::endl
                   << "\t\t\tscope: " << metadata.serviceMetadata.scope << std::endl
                   << "\t\t]" << std::endl;

                os << "\tproperties: [size = " << metadata.properties.size() << "] not shown" << std::endl;
                os << "\tconfigurationPolicy: " << metadata.configurationPolicy << std::endl;
                os << "\tconfigurationPids[size = " << metadata.configurationPids.size() << "]: [" << std::endl;
                for (auto const& pid : metadata.configurationPids)
                {
                    bool isLast = pid != metadata.configurationPids.back();
                    os << "\t\t" << pid << (isLast ? "" : ",") << std::endl;
                }
                os << "\t"
                   << "]" << std::endl
                   << "\t"
                   << "factoryComponentID: " << metadata.factoryComponentID << std::endl;
                os << "\tfactoryComponentProperties: [size = " << metadata.factoryComponentProperties.size()
                   << "] not shown" << std::endl;

                return os;
            }
        } // namespace metadata

        namespace dummy
        {

            struct ServiceImpl
            {
            };
            struct Reference1
            {
            };
            struct Reference2
            {
            };
            struct Reference3
            {
            };

        } // namespace dummy

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4373)
#endif

        /**
         * This class is used in tests where the logger is required and the test
         * needs to verify what is sent to the logger
         */
        class MockLogger : public cppmicroservices::logservice::LogService
        {
          public:
            MOCK_METHOD2(Log, void(cppmicroservices::logservice::SeverityLevel, std::string const&));
            MOCK_METHOD3(Log,
                         void(cppmicroservices::logservice::SeverityLevel,
                              std::string const&,
                              std::exception_ptr const));
            MOCK_METHOD3(Log,
                         void(cppmicroservices::ServiceReferenceBase const&,
                              cppmicroservices::logservice::SeverityLevel,
                              std::string const&));
            MOCK_METHOD4(Log,
                         void(cppmicroservices::ServiceReferenceBase const&,
                              cppmicroservices::logservice::SeverityLevel,
                              std::string const&,
                              std::exception_ptr const));
        };

#ifdef _MSC_VER
#    pragma warning(pop)
#endif

        /**
         * This class is used in tests where the logger is required but the test
         * does not care if anything is actually sent to the logger
         */
        class FakeLogger : public cppmicroservices::logservice::LogService
        {
          public:
            void
            Log(cppmicroservices::logservice::SeverityLevel, std::string const&) override
            {
            }
            void
            Log(cppmicroservices::logservice::SeverityLevel, std::string const&, std::exception_ptr const) override
            {
            }
            void
            Log(ServiceReferenceBase const&, cppmicroservices::logservice::SeverityLevel, std::string const&) override
            {
            }
            void
            Log(ServiceReferenceBase const&,
                cppmicroservices::logservice::SeverityLevel,
                std::string const&,
                std::exception_ptr const) override
            {
            }
        };

        class MockComponentManager : public ComponentManager
        {
          public:
            MOCK_CONST_METHOD0(GetName, std::string(void));
            MOCK_CONST_METHOD0(GetBundle, cppmicroservices::Bundle(void));
            MOCK_CONST_METHOD0(GetBundleId, unsigned long(void));
            MOCK_METHOD2(WaitForFuture, void(std::shared_future<void>&, std::shared_ptr<std::atomic<bool>>));
            MOCK_METHOD0(Initialize, void(void));
            MOCK_CONST_METHOD0(IsEnabled, bool(void));
            MOCK_METHOD1(Enable, std::shared_future<void>(std::shared_ptr<std::atomic<bool>>));
            MOCK_METHOD1(Disable, std::shared_future<void>(std::shared_ptr<std::atomic<bool>>));
            MOCK_CONST_METHOD0(GetComponentConfigurations, std::vector<std::shared_ptr<ComponentConfiguration>>());
            MOCK_CONST_METHOD0(GetMetadata, std::shared_ptr<metadata::ComponentMetadata const>());
        };

        class MockComponentRegistry : public ComponentRegistry
        {
          public:
            MOCK_CONST_METHOD0(GetComponentManagers, std::vector<std::shared_ptr<ComponentManager>>());
            MOCK_CONST_METHOD1(GetComponentManagers, std::vector<std::shared_ptr<ComponentManager>>(unsigned long));
            MOCK_CONST_METHOD2(GetComponentManager,
                               std::shared_ptr<ComponentManager>(unsigned long, std::string const&));
            MOCK_METHOD1(AddComponentManager, bool(std::shared_ptr<ComponentManager> const&));
            MOCK_METHOD1(RemoveComponentManager, void(std::shared_ptr<ComponentManager> const&));
        };

        class MockComponentManagerState : public ComponentManagerState
        {
          public:
            MOCK_METHOD2(Enable, std::shared_future<void>(ComponentManagerImpl&, std::shared_ptr<std::atomic<bool>>));
            MOCK_METHOD2(Disable, std::shared_future<void>(ComponentManagerImpl&, std::shared_ptr<std::atomic<bool>>));
            MOCK_CONST_METHOD1(IsEnabled, bool(ComponentManagerImpl const&));
            MOCK_CONST_METHOD1(GetConfigurations,
                               std::vector<std::shared_ptr<ComponentConfiguration>>(ComponentManagerImpl const&));
            MOCK_CONST_METHOD0(GetFuture, std::shared_future<void>());
        };

        class MockReferenceManager : public ReferenceManager
        {
          public:
            MOCK_CONST_METHOD0(GetReferenceName, std::string(void));
            MOCK_CONST_METHOD0(GetReferenceScope, std::string(void));
            MOCK_CONST_METHOD0(GetLDAPString, std::string(void));
            MOCK_CONST_METHOD0(IsSatisfied, bool(void));
            MOCK_CONST_METHOD0(IsOptional, bool(void));
            MOCK_CONST_METHOD0(GetBoundReferences, std::set<cppmicroservices::ServiceReferenceBase>());
            MOCK_CONST_METHOD0(GetTargetReferences, std::set<cppmicroservices::ServiceReferenceBase>());
            MOCK_METHOD1(RegisterListener,
                         cppmicroservices::ListenerTokenId(std::function<void(RefChangeNotification const&)>));
            MOCK_METHOD1(UnregisterListener, void(cppmicroservices::ListenerTokenId));
            MOCK_METHOD0(StopTracking, void(void));
            MOCK_CONST_METHOD0(IsUnary, bool(void));
            MOCK_CONST_METHOD0(IsMultiple, bool(void));
        };

        class MockReferenceManagerBaseImpl : public ReferenceManagerBaseImpl
        {
          public:
            MockReferenceManagerBaseImpl(metadata::ReferenceMetadata const& metadata,
                                         cppmicroservices::BundleContext const& bc,
                                         std::shared_ptr<cppmicroservices::logservice::LogService> logger,
                                         std::string const& configName)
                : ReferenceManagerBaseImpl(metadata, bc, logger, configName)
            {
            }

            MOCK_CONST_METHOD0(GetReferenceName, std::string(void));
            MOCK_CONST_METHOD0(GetReferenceScope, std::string(void));
            MOCK_CONST_METHOD0(GetLDAPString, std::string(void));
            MOCK_CONST_METHOD0(IsSatisfied, bool(void));
            MOCK_CONST_METHOD0(IsOptional, bool(void));
            MOCK_CONST_METHOD0(GetBoundReferences, std::set<cppmicroservices::ServiceReferenceBase>());
            MOCK_CONST_METHOD0(GetTargetReferences, std::set<cppmicroservices::ServiceReferenceBase>());
            MOCK_METHOD1(RegisterListener,
                         cppmicroservices::ListenerTokenId(std::function<void(RefChangeNotification const&)>));
            MOCK_METHOD1(UnregisterListener, void(cppmicroservices::ListenerTokenId));
            MOCK_METHOD0(StopTracking, void(void));
        };

        class MockComponentConfiguration : public ComponentConfiguration
        {
          public:
            MOCK_CONST_METHOD0(GetProperties, std::unordered_map<std::string, cppmicroservices::Any>(void));
            MOCK_CONST_METHOD0(GetAllDependencyManagers, std::vector<std::shared_ptr<ReferenceManager>>(void));
            MOCK_CONST_METHOD1(GetDependencyManager, std::shared_ptr<ReferenceManager>(std::string const&));
            MOCK_CONST_METHOD0(GetServiceReference, cppmicroservices::ServiceReferenceBase(void));
            MOCK_CONST_METHOD0(GetRegistry, std::shared_ptr<ComponentRegistry>(void));
            MOCK_CONST_METHOD0(GetBundle, cppmicroservices::Bundle(void));
            MOCK_CONST_METHOD0(GetId, unsigned long(void));
            MOCK_CONST_METHOD0(GetConfigState, ComponentState(void));
            MOCK_CONST_METHOD0(GetMetadata, std::shared_ptr<metadata::ComponentMetadata const>(void));
        };

        class MockFactory : public cppmicroservices::ServiceFactory
        {
          public:
            MOCK_METHOD2(GetService,
                         InterfaceMapConstPtr(cppmicroservices::Bundle const&,
                                              cppmicroservices::ServiceRegistrationBase const&));
            MOCK_METHOD3(UngetService,
                         void(cppmicroservices::Bundle const&,
                              cppmicroservices::ServiceRegistrationBase const&,
                              InterfaceMapConstPtr const&));
        };

        class MockComponentConfigurationState : public ComponentConfigurationState
        {
          public:
            MOCK_METHOD1(Register, void(ComponentConfigurationImpl&));
            MOCK_METHOD2(Activate,
                         std::shared_ptr<ComponentInstance>(ComponentConfigurationImpl&,
                                                            cppmicroservices::Bundle const&));
            MOCK_METHOD1(Deactivate, void(ComponentConfigurationImpl&));
            MOCK_METHOD1(Modified, bool(ComponentConfigurationImpl&));
            MOCK_METHOD4(Rebind,
                         void(ComponentConfigurationImpl&,
                              std::string const&,
                              ServiceReference<void> const&,
                              ServiceReference<void> const&));
            MOCK_CONST_METHOD0(GetValue, ComponentState(void));
            MOCK_METHOD0(WaitForTransitionTask, void());
        };

        class MockComponentInstance : public service::component::detail::ComponentInstance
        {
          public:
            MOCK_METHOD1(CreateInstance, void(std::shared_ptr<service::component::ComponentContext> const&));
            MOCK_METHOD1(BindReferences, void(std::shared_ptr<service::component::ComponentContext> const&));
            MOCK_METHOD0(UnbindReferences, void(void));
            MOCK_METHOD0(Activate, void(void));
            MOCK_METHOD0(Deactivate, void(void));
            MOCK_METHOD0(Modified, void(void));
            MOCK_METHOD2(InvokeUnbindMethod, void(std::string const&, cppmicroservices::ServiceReferenceBase const&));
            MOCK_METHOD2(InvokeBindMethod, void(std::string const&, cppmicroservices::ServiceReferenceBase const&));
            MOCK_METHOD0(GetInterfaceMap, cppmicroservices::InterfaceMapPtr(void));
            MOCK_METHOD0(DoesModifiedMethodExist, bool(void));
        };

        class MockComponentContextImpl : public ComponentContextImpl
        {
          public:
            MockComponentContextImpl(std::weak_ptr<ComponentConfiguration> const& cm) : ComponentContextImpl(cm) {}
            MOCK_CONST_METHOD0(GetProperties, std::unordered_map<std::string, cppmicroservices::Any>(void));
            MOCK_CONST_METHOD0(GetBundleContext, cppmicroservices::BundleContext(void));
            MOCK_CONST_METHOD0(GetUsingBundle, cppmicroservices::Bundle(void));
            MOCK_METHOD1(EnableComponent, void(std::string const&));
            MOCK_METHOD1(DisableComponent, void(std::string const&));
            MOCK_CONST_METHOD0(GetServiceReference, cppmicroservices::ServiceReferenceBase(void));
            MOCK_CONST_METHOD2(LocateService, std::shared_ptr<void>(std::string const&, std::string const&));
            MOCK_CONST_METHOD2(LocateServices,
                               std::vector<std::shared_ptr<void>>(std::string const&, std::string const&));
        };

        class MockComponentManagerImpl : public ComponentManagerImpl
        {
          public:
            MockComponentManagerImpl(std::shared_ptr<metadata::ComponentMetadata const> metadata,
                                     std::shared_ptr<ComponentRegistry> registry,
                                     BundleContext bundleContext,
                                     std::shared_ptr<cppmicroservices::logservice::LogService> logger,
                                     std::shared_ptr<cppmicroservices::async::AsyncWorkService> asyncWorkService,
                                     std::shared_ptr<ConfigurationNotifier> notifier)
                : ComponentManagerImpl(metadata, registry, bundleContext, logger, asyncWorkService, notifier)
                , statechangecount(0)
            {
            }
            virtual ~MockComponentManagerImpl() = default;
            void
            SetState(std::shared_ptr<ComponentManagerState> const& newState)
            {
                auto currentState = GetState();
                ComponentManagerImpl::CompareAndSetState(&currentState, newState);
            }

            MOCK_CONST_METHOD0(GetComponentConfigurations, std::vector<std::shared_ptr<ComponentConfiguration>>());

            bool
            CompareAndSetState(std::shared_ptr<ComponentManagerState>* expectedState,
                               std::shared_ptr<ComponentManagerState> desiredState) override
            {
                bool superresult = ComponentManagerImpl::CompareAndSetState(expectedState, desiredState);
                statechangecount += superresult ? 1 : 0;
                return superresult;
            }

            void
            ResetCounter()
            {
                statechangecount = 0;
            }
            // count the number of successful state swaps. One successful state transition = 2 atomic state swaps
            std::atomic<int> statechangecount;
        };

        class MockComponentConfigurationImpl : public ComponentConfigurationImpl
        {
          public:
            MockComponentConfigurationImpl(std::shared_ptr<metadata::ComponentMetadata const> metadata,
                                           Bundle const& bundle,
                                           std::shared_ptr<ComponentRegistry> registry,
                                           std::shared_ptr<cppmicroservices::logservice::LogService> logger,
                                           std::shared_ptr<ConfigurationNotifier> notifier)
                : ComponentConfigurationImpl(metadata, bundle, registry, logger, notifier)
                , statechangecount(0)
            {
            }
            virtual ~MockComponentConfigurationImpl() = default;
            MOCK_METHOD0(GetFactory, std::shared_ptr<ServiceFactory>(void));
            MOCK_METHOD1(CreateAndActivateComponentInstance,
                         std::shared_ptr<ComponentInstance>(cppmicroservices::Bundle const&));
            MOCK_METHOD1(UnbindAndDeactivateComponentInstance, void(std::shared_ptr<ComponentInstance>));
            MOCK_METHOD0(DestroyComponentInstances, void());
            MOCK_METHOD2(BindReference, void(std::string const&, ServiceReferenceBase const&));
            MOCK_METHOD2(UnbindReference, void(std::string const&, ServiceReferenceBase const&));
            MOCK_METHOD0(ModifyComponentInstanceProperties, bool());
            void
            SetState(std::shared_ptr<ComponentConfigurationState> const& newState)
            {
                ComponentConfigurationImpl::SetState(newState);
            }

            bool
            CompareAndSetState(std::shared_ptr<ComponentConfigurationState>* expectedState,
                               std::shared_ptr<ComponentConfigurationState> desiredState) override
            {
                bool superresult = ComponentConfigurationImpl::CompareAndSetState(expectedState, desiredState);
                statechangecount += superresult ? 1 : 0;
                return superresult;
            }
            void
            ResetCounter()
            {
                statechangecount = 0;
            }
            // count the number of successful state swaps. One successful state transition = 2 atomic state swaps
            std::atomic<int> statechangecount;
        };

    } // namespace scrimpl

    namespace async
    {
        class MockAsyncWorkService : public cppmicroservices::async::AsyncWorkService
        {
          public:
            MockAsyncWorkService() : cppmicroservices::async::AsyncWorkService() {}

            MOCK_METHOD1(post, void(std::packaged_task<void()>&&));
            MOCK_METHOD0(waitForAllPostedTasksToRun, void());
        };
    } // namespace async
} // namespace cppmicroservices

#endif /* __MOCKS_HPP__ */
