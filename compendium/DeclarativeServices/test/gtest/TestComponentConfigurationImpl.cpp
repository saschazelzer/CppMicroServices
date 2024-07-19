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

#include <future>
#include <random>
#include <thread>
#include <tuple>

#include "../../src/ConfigurationListenerImpl.hpp"
#include "../../src/SCRAsyncWorkService.hpp"
#include "../../src/SCRExtensionRegistry.hpp"
#include "../../src/SCRLogger.hpp"
#include "../../src/manager/BundleLoader.hpp"
#include "../../src/manager/BundleOrPrototypeComponentConfiguration.hpp"
#include "../../src/manager/ComponentConfigurationImpl.hpp"
#include "../../src/manager/ReferenceManager.hpp"
#include "../../src/manager/SingletonComponentConfiguration.hpp"
#include "../../src/manager/states/CCActiveState.hpp"
#include "../../src/manager/states/CCRegisteredState.hpp"
#include "../../src/manager/states/CCUnsatisfiedReferenceState.hpp"
#include "ConcurrencyTestUtil.hpp"
#include "Mocks.hpp"
#include "cppmicroservices/Framework.h"
#include "cppmicroservices/FrameworkEvent.h"
#include "cppmicroservices/FrameworkFactory.h"
#include "cppmicroservices/ServiceInterface.h"

#include "../TestUtils.hpp"
#include <TestInterfaces/Interfaces.hpp>

using cppmicroservices::service::component::ComponentContext;

namespace cppmicroservices
{
    namespace scrimpl
    {

        class ComponentConfigurationImplTest : public ::testing::Test
        {
          protected:
            ComponentConfigurationImplTest() : framework(cppmicroservices::FrameworkFactory().NewFramework()) {}

            virtual ~ComponentConfigurationImplTest() = default;

            virtual void
            SetUp()
            {
                framework.Start();
            }

            virtual void
            TearDown()
            {
                framework.Stop();
                framework.WaitForStop(std::chrono::milliseconds::zero());
            }

            cppmicroservices::Framework&
            GetFramework()
            {
                return framework;
            }

          private:
            cppmicroservices::Framework framework;
        };

        TEST_F(ComponentConfigurationImplTest, VerifyCtor)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            EXPECT_THROW(
                {
                    auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(nullptr,
                                                                                           GetFramework(),
                                                                                           mockRegistry,
                                                                                           fakeLogger,
                                                                                           notifier);
                },
                std::invalid_argument);
            EXPECT_THROW(
                {
                    auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                           GetFramework(),
                                                                                           nullptr,
                                                                                           fakeLogger,
                                                                                           notifier);
                },
                std::invalid_argument);
            EXPECT_THROW(
                {
                    auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                           GetFramework(),
                                                                                           mockRegistry,
                                                                                           nullptr,
                                                                                           notifier);
                },
                std::invalid_argument);

            EXPECT_NO_THROW({
                auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                       GetFramework(),
                                                                                       mockRegistry,
                                                                                       fakeLogger,
                                                                                       notifier);
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
                EXPECT_EQ(fakeCompConfig->regManager, nullptr);
                EXPECT_EQ(fakeCompConfig->referenceManagers.size(), static_cast<size_t>(0));
            });
        }

        TEST_F(ComponentConfigurationImplTest, VerifyUniqueId)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            std::set<unsigned long> idSet;
            size_t const iterCount = 10;
            for (size_t i = 0; i < iterCount; ++i)
            {
                EXPECT_NO_THROW({
                    auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                           GetFramework(),
                                                                                           mockRegistry,
                                                                                           fakeLogger,
                                                                                           notifier);
                    idSet.insert(fakeCompConfig->GetId());
                });
            }
            EXPECT_EQ(idSet.size(), iterCount);
        }

        TEST_F(ComponentConfigurationImplTest, VerifyRefSatisfied)
        {
            // test case: A component has three dependencies. Dependency1 is already
            // available, Dependency 2 becomes available, then Dependency3 becomes available.
            // When Dependency2 becomes available, Dependency3 is still unavailable so
            // the component must not trigger a state change. When Dependency3 becomes
            // available, all the dependencies are now available which results in a state change.
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();

            mockMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
            auto refMgr1 = std::make_shared<MockReferenceManager>();
            auto refMgr2 = std::make_shared<MockReferenceManager>();
            auto refMgr3 = std::make_shared<MockReferenceManager>();
            EXPECT_CALL(*refMgr1, IsSatisfied())
                .WillRepeatedly(testing::Return(true)); // simulate pre-existing reference
            EXPECT_CALL(*refMgr2, IsSatisfied())
                .Times(1)
                .WillOnce(testing::Return(true)); // simulate reference that becomes available
            EXPECT_CALL(*refMgr3, IsSatisfied())
                .Times(1)
                .WillOnce(testing::Return(false)); // simulate reference that is unavailable when ref2 is satisfied
            auto mockFactory = std::make_shared<MockFactory>();
            auto mockCompInstance = std::make_shared<MockComponentInstance>();
            auto bc = GetFramework().GetBundleContext();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(bc, fakeLogger, asyncWorkService, extRegistry);
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);
            EXPECT_CALL(*fakeCompConfig, GetFactory()).Times(1).WillOnce(testing::Return(mockFactory));
            // add the mock reference managers to the config object
            fakeCompConfig->referenceManagers.insert(std::make_pair("ref1", refMgr1));
            fakeCompConfig->referenceManagers.insert(std::make_pair("ref2", refMgr2));
            fakeCompConfig->referenceManagers.insert(std::make_pair("ref3", refMgr3));
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
            // callback from refMgr2
            fakeCompConfig->RefSatisfied("ref2");
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
            // callback from refMgr3
            fakeCompConfig->RefSatisfied("ref3");
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::SATISFIED);
            EXPECT_EQ(fakeCompConfig->GetServiceReference().operator bool(), true);
            EXPECT_EQ(
                fakeCompConfig->GetServiceReference().IsConvertibleTo(mockMetadata->serviceMetadata.interfaces.at(0)),
                true);
            fakeCompConfig->referenceManagers.clear(); // remove the mock reference managers
        }

        TEST_F(ComponentConfigurationImplTest, VerifyRefUnsatisfied)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();

            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);
            auto mockStatisfiedState = std::make_shared<MockComponentConfigurationState>();
            auto mockUnsatisfiedState = std::make_shared<MockComponentConfigurationState>();
            EXPECT_CALL(*mockStatisfiedState, GetValue())
                .Times(2)
                .WillRepeatedly(testing::Return(service::component::runtime::dto::ComponentState::SATISFIED));
            EXPECT_CALL(*mockStatisfiedState, Deactivate(testing::_))
                .Times(1)
                .WillRepeatedly(
                    testing::Invoke([&](ComponentConfigurationImpl& config) { config.state = mockUnsatisfiedState; }));
            EXPECT_CALL(*mockUnsatisfiedState, GetValue())
                .Times(1)
                .WillRepeatedly(
                    testing::Return(service::component::runtime::dto::ComponentState::UNSATISFIED_REFERENCE));
            auto refMgr1 = std::make_shared<MockReferenceManager>();
            fakeCompConfig->referenceManagers.insert(std::make_pair("ref1", refMgr1));
            fakeCompConfig->state = mockStatisfiedState;
            EXPECT_EQ(fakeCompConfig->GetConfigState(), service::component::runtime::dto::ComponentState::SATISFIED);
            fakeCompConfig->RefUnsatisfied("invalid_refname");
            EXPECT_EQ(fakeCompConfig->GetConfigState(), service::component::runtime::dto::ComponentState::SATISFIED);
            fakeCompConfig->RefUnsatisfied("ref1");
            EXPECT_EQ(fakeCompConfig->GetConfigState(),
                      service::component::runtime::dto::ComponentState::UNSATISFIED_REFERENCE);
            fakeCompConfig->referenceManagers.clear(); // remove the mock reference managers
        }

        TEST_F(ComponentConfigurationImplTest, VerifyRefChangedState)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();

            // Test that a call to Register with a component containing both a service
            // and a reference to the same service interface will not cause a state change.
            scrimpl::metadata::ReferenceMetadata refMetadata {};
            refMetadata.interfaceName = "dummy::ServiceImpl";
            mockMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
            mockMetadata->refsMetadata.push_back(refMetadata);
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);

            auto reg = GetFramework().GetBundleContext().RegisterService<dummy::ServiceImpl>(
                std::make_shared<dummy::ServiceImpl>());

            EXPECT_EQ(fakeCompConfig->GetConfigState(),
                      service::component::runtime::dto::ComponentState::UNSATISFIED_REFERENCE);
            reg.Unregister();
        }

        TEST_F(ComponentConfigurationImplTest, VerifyRegister)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            // Test if a call to Register will change the state when the component
            // does not provide a service.
            {
                auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                       GetFramework(),
                                                                                       mockRegistry,
                                                                                       fakeLogger,
                                                                                       notifier);
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
                EXPECT_EQ(fakeCompConfig->regManager, nullptr);
                EXPECT_EQ(fakeCompConfig->referenceManagers.size(), static_cast<size_t>(0));
                EXPECT_NO_THROW(fakeCompConfig->Register());
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::SATISFIED);
                EXPECT_EQ(fakeCompConfig->GetServiceReference().operator bool(), false);
            }
            // Test if a call to Register will change the state when the component
            // provides a service.
            {
                mockMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
                auto mockFactory = std::make_shared<MockFactory>();
                auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
                auto asyncWorkService = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(
                    GetFramework().GetBundleContext(),
                    logger);
                auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
                auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                        fakeLogger,
                                                                        asyncWorkService,
                                                                        extRegistry);
                auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                       GetFramework(),
                                                                                       mockRegistry,
                                                                                       fakeLogger,
                                                                                       notifier);
                EXPECT_CALL(*fakeCompConfig, GetFactory()).Times(1).WillRepeatedly(testing::Return(mockFactory));
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
                EXPECT_NE(fakeCompConfig->regManager, nullptr);
                EXPECT_EQ(fakeCompConfig->referenceManagers.size(), static_cast<size_t>(0));
                EXPECT_NO_THROW(fakeCompConfig->Register());
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::SATISFIED);
                EXPECT_EQ(fakeCompConfig->GetServiceReference().operator bool(), true);
                EXPECT_EQ(fakeCompConfig->GetServiceReference().IsConvertibleTo(
                              us_service_interface_iid<dummy::ServiceImpl>()),
                          true);
            }
            // Test if a call to Register will change the state when the component
            // provides a service and component is immediate type.
            // For immediate component, a call to Register will result in immediate activation
            {
                mockMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
                mockMetadata->immediate = true;
                auto mockFactory = std::make_shared<MockFactory>();
                auto mockCompInstance = std::make_shared<MockComponentInstance>();
                auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
                auto asyncWorkService = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(
                    GetFramework().GetBundleContext(),
                    logger);
                auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
                auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                        fakeLogger,
                                                                        asyncWorkService,
                                                                        extRegistry);
                auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                       GetFramework(),
                                                                                       mockRegistry,
                                                                                       fakeLogger,
                                                                                       notifier);
                EXPECT_CALL(*fakeCompConfig, GetFactory()).Times(1).WillRepeatedly(testing::Return(mockFactory));
                EXPECT_CALL(*fakeCompConfig, CreateAndActivateComponentInstance(testing::_))
                    .Times(1)
                    .WillRepeatedly(testing::Return(mockCompInstance));
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
                EXPECT_NE(fakeCompConfig->regManager, nullptr);
                EXPECT_EQ(fakeCompConfig->referenceManagers.size(), static_cast<size_t>(0));
                EXPECT_NO_THROW(fakeCompConfig->Register());
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::ACTIVE);
                EXPECT_EQ(fakeCompConfig->GetServiceReference().operator bool(), true);
                EXPECT_EQ(fakeCompConfig->GetServiceReference().IsConvertibleTo(
                              us_service_interface_iid<dummy::ServiceImpl>()),
                          true);
            }
        }

        TEST_F(ComponentConfigurationImplTest, VerifyStateChangeDelegation)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto mockState = std::make_shared<MockComponentConfigurationState>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
            fakeCompConfig->state = mockState;
            ComponentConfigurationImpl& fakeCompConfigBase
                = *(std::dynamic_pointer_cast<ComponentConfigurationImpl>(fakeCompConfig));
            EXPECT_CALL(*mockState, Register(testing::Ref(fakeCompConfigBase))).Times(1);
            EXPECT_CALL(*mockState, Activate(testing::Ref(fakeCompConfigBase), GetFramework())).Times(1);
            EXPECT_CALL(*mockState, Deactivate(testing::Ref(fakeCompConfigBase))).Times(1);
            EXPECT_CALL(*mockState, GetValue()).Times(1);
            fakeCompConfig->Register();
            fakeCompConfig->Activate(GetFramework());
            fakeCompConfig->Deactivate();
            (void)fakeCompConfig->GetConfigState();
        }

        TEST_F(ComponentConfigurationImplTest, VerifyActivate_Success)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);
            fakeCompConfig->state = std::make_shared<CCRegisteredState>();
            auto mockCompInstance = std::make_shared<MockComponentInstance>();
            EXPECT_CALL(*fakeCompConfig, CreateAndActivateComponentInstance(testing::_))
                .Times(1)
                .WillRepeatedly(testing::Return(mockCompInstance));
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::SATISFIED);
            EXPECT_NO_THROW(fakeCompConfig->Activate(GetFramework()));
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::ACTIVE);
        }

        TEST_F(ComponentConfigurationImplTest, VerifyActivate_Failure)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            // Test for exception from user code
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);
            fakeCompConfig->state = std::make_shared<CCRegisteredState>();
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::SATISFIED);
            EXPECT_CALL(*fakeCompConfig, CreateAndActivateComponentInstance(testing::_))
                .Times(1)
                .WillRepeatedly(testing::Return(nullptr));
            EXPECT_NO_THROW(fakeCompConfig->Activate(GetFramework()));
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::SATISFIED);
        }

        TEST_F(ComponentConfigurationImplTest, VerifyDeactivate)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);
            auto activeState = std::make_shared<CCActiveState>();
            fakeCompConfig->state = activeState;
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::ACTIVE);
            EXPECT_CALL(*fakeCompConfig, DestroyComponentInstances()).Times(1);
            EXPECT_NO_THROW(fakeCompConfig->Deactivate());
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
        }

        bool
        ValidateStateSequence(std::vector<std::pair<ComponentState, ComponentState>> const& stateArr)
        {
            bool foundInvalidTransition = false;
            auto vecSize = stateArr.size();
            for (size_t i = 0; i < vecSize && !foundInvalidTransition; ++i)
            {
                auto currState = stateArr[i].first;
                auto nextState = stateArr[i].second;
                switch (currState)
                {
                    case service::component::runtime::dto::ComponentState::UNSATISFIED_REFERENCE:
                        if (nextState == service::component::runtime::dto::ComponentState::ACTIVE)
                        {
                            foundInvalidTransition = true;
                        }
                        break;
                    case service::component::runtime::dto::ComponentState::SATISFIED:
                        break;
                    case service::component::runtime::dto::ComponentState::ACTIVE:
                        if (nextState == service::component::runtime::dto::ComponentState::SATISFIED)
                        {
                            foundInvalidTransition = true;
                        }
                        break;
                }
            }
            return !foundInvalidTransition;
        }

        TEST_F(ComponentConfigurationImplTest, VerifyConcurrentRegisterDeactivate)
        {
            // call register and deactivate from multiple threads simultaneously
            // ensure there is
            // - only one registration if the current state is SATISFIED
            // - zero registrations if the current state is UNSATISFIED_REFERENCE
            // ensure there are zero objects of ComponentInstance
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            mockMetadata->serviceMetadata.interfaces = { "ServiceInterface", "interface" };
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);
            EXPECT_CALL(*fakeCompConfig, GetFactory()).WillRepeatedly(testing::Return(std::make_shared<MockFactory>()));
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
            EXPECT_NE(fakeCompConfig->regManager, nullptr);
            std::function<std::pair<ComponentState, ComponentState>()> func = [&fakeCompConfig]()
            {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<unsigned int> dis;
                int randVal = dis(gen);
                auto prevState = fakeCompConfig->GetConfigState();
                if (randVal & 0x1)
                {
                    fakeCompConfig->Register();
                }
                else
                {
                    fakeCompConfig->Deactivate();
                }
                auto currentState = fakeCompConfig->GetConfigState();
                return std::make_pair(prevState, currentState);
            };

            auto results = ConcurrentInvoke(func);
            EXPECT_TRUE(ValidateStateSequence(results));
            if (fakeCompConfig->GetConfigState() == service::component::runtime::dto::ComponentState::SATISFIED)
            {
                EXPECT_EQ(GetFramework().GetBundleContext().GetServiceReferences("interface").size(), 1u);
            }
            else if (fakeCompConfig->GetConfigState()
                     == service::component::runtime::dto::ComponentState::UNSATISFIED_REFERENCE)
            {
                EXPECT_EQ(GetFramework().GetBundleContext().GetServiceReferences("interface").size(), 0u);
            }
        }

        TEST_F(ComponentConfigurationImplTest, VerifyConcurrentActivateDeactivate)
        {
            // call activate and deactivate from multiple threads simultaneously
            // ensure there the current state is UNSATISFIED_REFERENCE
            // ensure there are zero objects of ComponentInstance
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            mockMetadata->serviceMetadata.interfaces = { "ServiceInterface", "interface" };
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto mockCompInstance = std::make_shared<MockComponentInstance>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);
            EXPECT_CALL(*fakeCompConfig, CreateAndActivateComponentInstance(testing::_))
                .WillRepeatedly(testing::Return(mockCompInstance));
            EXPECT_CALL(*fakeCompConfig, GetFactory()).WillRepeatedly(testing::Return(std::make_shared<MockFactory>()));
            fakeCompConfig->Register();
            EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::SATISFIED);
            auto clientBundle = GetFramework();
            std::function<std::pair<ComponentState, ComponentState>()> func = [&fakeCompConfig, &clientBundle]()
            {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<unsigned int> dis;
                int randVal = dis(gen);
                auto prevState = fakeCompConfig->GetConfigState();
                if (randVal & 0x1)
                {
                    fakeCompConfig->Activate(clientBundle);
                }
                else
                {
                    fakeCompConfig->Deactivate();
                }
                auto currentState = fakeCompConfig->GetConfigState();
                return std::make_pair(prevState, currentState);
            };
            auto results = ConcurrentInvoke(func);
            EXPECT_TRUE(ValidateStateSequence(results));
        }

        TEST_F(ComponentConfigurationImplTest, VerifyImmediateComponent)
        {
            EXPECT_NO_THROW({
                auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
                mockMetadata->immediate = true;
                auto mockRegistry = std::make_shared<MockComponentRegistry>();
                auto fakeLogger = std::make_shared<FakeLogger>();
                auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
                auto asyncWorkService = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(
                    GetFramework().GetBundleContext(),
                    logger);
                auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
                auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                        fakeLogger,
                                                                        asyncWorkService,
                                                                        extRegistry);
                auto mockCompInstance = std::make_shared<MockComponentInstance>();
                auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                       GetFramework(),
                                                                                       mockRegistry,
                                                                                       fakeLogger,
                                                                                       notifier);
                EXPECT_CALL(*fakeCompConfig, CreateAndActivateComponentInstance(testing::_))
                    .Times(2)
                    .WillRepeatedly(testing::Return(mockCompInstance));
                fakeCompConfig->Initialize();
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::ACTIVE);
                EXPECT_CALL(*fakeCompConfig, DestroyComponentInstances()).Times(1);
                fakeCompConfig->Deactivate();
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
                fakeCompConfig->Register();
                // since its an immediate component, it gets activated on call to Register.
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::ACTIVE);
            });
        }

        TEST_F(ComponentConfigurationImplTest, VerifyDelayedComponent)
        {
            EXPECT_NO_THROW({
                auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
                auto mockRegistry = std::make_shared<MockComponentRegistry>();
                auto fakeLogger = std::make_shared<FakeLogger>();
                auto mockCompInstance = std::make_shared<MockComponentInstance>();
                auto mockFactory = std::make_shared<MockFactory>();
                auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
                auto asyncWorkService = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(
                    GetFramework().GetBundleContext(),
                    logger);
                auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
                auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                        fakeLogger,
                                                                        asyncWorkService,
                                                                        extRegistry);
                mockMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
                mockMetadata->immediate = false;
                auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                       GetFramework(),
                                                                                       mockRegistry,
                                                                                       fakeLogger,
                                                                                       notifier);
                EXPECT_CALL(*fakeCompConfig, GetFactory())
                    .Times(testing::AtLeast(1)) // 2
                    .WillRepeatedly(testing::Return(mockFactory));
                fakeCompConfig->Initialize();
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::SATISFIED);
                EXPECT_CALL(*fakeCompConfig, CreateAndActivateComponentInstance(testing::_))
                    .Times(testing::AtLeast(1)) // 2
                    .WillRepeatedly(testing::Return(mockCompInstance));
                fakeCompConfig->Activate(GetFramework());
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::ACTIVE);
                EXPECT_CALL(*fakeCompConfig, DestroyComponentInstances()).Times(1);
                fakeCompConfig->Deactivate();
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::UNSATISFIED_REFERENCE);
                fakeCompConfig->Register();
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::SATISFIED);
                auto bc = GetFramework().GetBundleContext();
                auto sRef = bc.GetServiceReference<dummy::ServiceImpl>();
                ASSERT_EQ(sRef.operator bool(), true);
                ASSERT_EQ(sRef, fakeCompConfig->GetServiceReference());
                auto mockServiceImpl = std::make_shared<dummy::ServiceImpl>();
                InterfaceMapPtr instanceMap = MakeInterfaceMap<dummy::ServiceImpl>(mockServiceImpl);
                EXPECT_CALL(*mockFactory, GetService(testing::_, testing::_))
                    .Times(1)
                    .WillRepeatedly(testing::Invoke(
                        [&](const cppmicroservices::Bundle& b, const cppmicroservices::ServiceRegistrationBase&)
                        {
                            fakeCompConfig->Activate(b);
                            return instanceMap;
                        }));
                EXPECT_CALL(*mockFactory, UngetService(testing::_, testing::_, testing::_));
                auto service = bc.GetService<dummy::ServiceImpl>(sRef);
                EXPECT_NE(service, nullptr);
                EXPECT_EQ(fakeCompConfig->GetConfigState(), ComponentState::ACTIVE);
            });
        }

        TEST_F(ComponentConfigurationImplTest, TestGetDependencyManagers)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto mockCompInstance = std::make_shared<MockComponentInstance>();
            auto mockFactory = std::make_shared<MockFactory>();
            auto logger = std::make_shared<SCRLogger>(GetFramework().GetBundleContext());
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   logger);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            mockMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
            mockMetadata->immediate = false;
            metadata::ReferenceMetadata rm1;
            rm1.name = "Foo";
            rm1.interfaceName = "sample::Foo";
            metadata::ReferenceMetadata rm2;
            rm2.name = "Bar";
            rm2.interfaceName = "sample::Bar";
            mockMetadata->refsMetadata.push_back(rm1);
            mockMetadata->refsMetadata.push_back(rm2);
            auto fakeCompConfig = std::make_shared<MockComponentConfigurationImpl>(mockMetadata,
                                                                                   GetFramework(),
                                                                                   mockRegistry,
                                                                                   fakeLogger,
                                                                                   notifier);
            EXPECT_EQ(fakeCompConfig->GetAllDependencyManagers().size(), mockMetadata->refsMetadata.size());
            EXPECT_NE(fakeCompConfig->GetDependencyManager("Foo"), nullptr);
            EXPECT_NE(fakeCompConfig->GetDependencyManager("Bar"), nullptr);
        }

        TEST_F(ComponentConfigurationImplTest, TestComponentWithUniqueName)
        {
#if defined(US_BUILD_SHARED_LIBS)
            auto dsPluginPath = test::GetDSRuntimePluginFilePath();
            auto dsbundles = GetFramework().GetBundleContext().InstallBundles(dsPluginPath);
            ASSERT_EQ(dsbundles.size(), 1);
            for (auto& bundle : dsbundles)
            {
                ASSERT_TRUE(bundle);
                bundle.Start();
            }
#endif

            auto testBundle = test::InstallAndStartBundle(GetFramework().GetBundleContext(), "TestBundleDSTOI8");
            ASSERT_TRUE(testBundle);
            ASSERT_EQ(testBundle.GetSymbolicName(), "TestBundleDSTOI8");

            auto svcRef = testBundle.GetBundleContext().GetServiceReference<test::Interface1>();
            ASSERT_TRUE(svcRef);
            auto svc = testBundle.GetBundleContext().GetService<test::Interface1>(svcRef);
            EXPECT_NE(svc, nullptr);
        }

        TEST_F(ComponentConfigurationImplTest, VerifyStateChangeWithSvcRefAndConfig)
        {
            auto mockMetadata = std::make_shared<metadata::ComponentMetadata>();
            // Test that a call to Register with a component containing a service,
            // a service reference and a config object dependency will trigger a state change
            // when the config object is satisfied before the config object change listener is
            // registered.
            mockMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::Reference1>() };
            scrimpl::metadata::ReferenceMetadata refMetadata {};
            refMetadata.interfaceName = "cppmicroservices::scrimpl::dummy::ServiceImpl";
            mockMetadata->refsMetadata.push_back(refMetadata);
            mockMetadata->configurationPolicy = "require";
            mockMetadata->configurationPids = { "foo" };

            auto fakeLogger = std::make_shared<FakeLogger>();
            auto asyncWorkService
                = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(GetFramework().GetBundleContext(),
                                                                                   fakeLogger);
            auto logger = std::make_shared<cppmicroservices::scrimpl::SCRLogger>(GetFramework().GetBundleContext());
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);
            auto notifier = std::make_shared<ConfigurationNotifier>(GetFramework().GetBundleContext(),
                                                                    fakeLogger,
                                                                    asyncWorkService,
                                                                    extRegistry);
            auto fakeCompConfig
                = std::make_shared<SingletonComponentConfigurationImpl>(mockMetadata,
                                                                        GetFramework(),
                                                                        std::make_shared<MockComponentRegistry>(),
                                                                        fakeLogger,
                                                                        notifier);

            auto fakeBundleProtoCompConfig = std::make_shared<BundleOrPrototypeComponentConfigurationImpl>(
                mockMetadata,
                GetFramework(),
                std::make_shared<MockComponentRegistry>(),
                fakeLogger,
                notifier);

            auto svcReg = GetFramework().GetBundleContext().RegisterService<dummy::ServiceImpl>(
                std::make_shared<dummy::ServiceImpl>());

            // update config object to satisfy component configuration
            test::InstallAndStartConfigAdmin(GetFramework().GetBundleContext());
            auto svcRef = GetFramework()
                              .GetBundleContext()
                              .GetServiceReference<cppmicroservices::service::cm::ConfigurationAdmin>();
            ASSERT_TRUE(svcRef);
            auto configAdminSvc = GetFramework().GetBundleContext().GetService(svcRef);
            ASSERT_TRUE(configAdminSvc);
            auto fooConfig = configAdminSvc->GetConfiguration("foo");
            cppmicroservices::AnyMap configData(cppmicroservices::AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
            configData["bar"] = std::string { "baz" };
            // update the config object before calling fakeCompConfig->Initialize(), which simulates the config object
            // being updated before the ComponentConfigurationImpl has a chance to setup config change listeners.
            ASSERT_NO_THROW(fooConfig->UpdateIfDifferent(configData).second.get());

            // Initialize should set the component state to Satisfied, even if the config object update was missed
            // by the config object change listener.
            fakeCompConfig->Initialize();
            EXPECT_EQ(cppmicroservices::service::component::runtime::dto::ComponentState::SATISFIED,
                      fakeCompConfig->GetConfigState());

            fakeBundleProtoCompConfig->Initialize();
            EXPECT_EQ(cppmicroservices::service::component::runtime::dto::ComponentState::SATISFIED,
                      fakeBundleProtoCompConfig->GetConfigState());

            fakeCompConfig->Deactivate();
            fakeCompConfig->Stop();
            fakeBundleProtoCompConfig->Deactivate();
            fakeBundleProtoCompConfig->Stop();
            svcReg.Unregister();
            svcReg = nullptr;
        }

#if !defined(__MINGW32__)
        // Note: This is different than the other tests in this suite as Declarative Services is actually
        // installed and started rather than using mocks.
        TEST(ComponentConfigurationImplLogTest, LoadLibraryLogsMessagesImmediateTest)
        {
            auto framework = cppmicroservices::FrameworkFactory().NewFramework();
            framework.Start();
            ASSERT_TRUE(framework);

            auto context = framework.GetBundleContext();
            ASSERT_TRUE(context);

            test::InstallAndStartDS(context);

            // The logger should receive 2 Log() calls from creating the SCRBundleExtension and 2 from the
            // code which actually calls SharedLibrary::Load()
            //
            // The logger is created and registered before InstallAndStartBundle since it done after, it would
            // miss the log messages.
            auto logger = std::make_shared<MockLogger>();

            // Because we are actually installing DS and creating the logger before installing and starting
            // the bundle (since it is immediate), there are 2 other log messages that are sent. They pertain
            // to creating and having created the SCRBundleExtension. If that expectation is not set, the test
            // fails.
            EXPECT_CALL(*logger, Log(logservice::SeverityLevel::LOG_DEBUG, ::testing::_)).Times(2);
            // This expectation is for the actual info log messages pertaining to loading of the shared
            // library.
            EXPECT_CALL(*logger, Log(logservice::SeverityLevel::LOG_INFO, ::testing::_)).Times(2);

            auto loggerReg = context.RegisterService<logservice::LogService>(logger);

            // TestBundleDSTOI1 is immediate=true so the call to InstallAndStart should cause the shared
            // library for the bundle to be loaded. This should in turn log 4 (2 regarding shared library
            // loading) messages with the log service.
            //
            // NOTE: TestBundleDSTOI1 cannot be used in the test since a previously ran test already installed
            // it. The DS runtime service is a singleton so even though a new framework is used, DS remembers
            // which bundles were already installed. This means that when this test tried to load
            // TestBundleDSTOI1, it did not actually call SharedLibrary::Load(), hence the test failed.
            // TestBundleDSTOI3 is now used as it has not been previously installed.
            test::InstallAndStartBundle(context, "TestBundleDSTOI3");

            loggerReg.Unregister();

            framework.Stop();
            framework.WaitForStop(std::chrono::milliseconds::zero());
        }

        TEST(ComponentConfigurationTests, TestConcurrentStop)
        {
            auto framework = cppmicroservices::FrameworkFactory().NewFramework();
            framework.Start();
            ASSERT_TRUE(framework);

            auto context = framework.GetBundleContext();
            ASSERT_TRUE(context);

            test::InstallAndStartDS(context);

            std::vector<cppmicroservices::Bundle> installedBundles
                = { ::test::InstallAndStartBundle(context, "DSGraph01"),
                    ::test::InstallAndStartBundle(context, "DSGraph02"),
                    ::test::InstallAndStartBundle(context, "DSGraph03"),
                    ::test::InstallAndStartBundle(context, "DSGraph04"),
                    ::test::InstallAndStartBundle(context, "DSGraph05"),
                    ::test::InstallAndStartBundle(context, "DSGraph06"),
                    ::test::InstallAndStartBundle(context, "DSGraph07") };

            std::vector<cppmicroservices::ServiceReferenceU> const interfaces {
                context.GetServiceReference<test::DSGraph01>(), context.GetServiceReference<test::DSGraph02>(),
                context.GetServiceReference<test::DSGraph03>(), context.GetServiceReference<test::DSGraph04>(),
                context.GetServiceReference<test::DSGraph05>(), context.GetServiceReference<test::DSGraph06>(),
                context.GetServiceReference<test::DSGraph07>()
            };

            for (auto const& sref : interfaces)
            {
                ASSERT_TRUE(static_cast<bool>(sref));
                auto service = context.GetService(sref);
                ASSERT_NE(service, nullptr);
            }

            EXPECT_NO_THROW({
                std::thread bundleT = std::thread(
                    [&installedBundles]()
                    {
                        for (auto& bundle : installedBundles)
                        {
                            bundle.Stop();
                        }
                    });
                std::thread frameworkT = std::thread(
                    [&framework]()
                    {
                        framework.Stop();
                        framework.WaitForStop(std::chrono::milliseconds::zero());
                    });

                bundleT.join();
                frameworkT.join();
            });
        }

        // Note: This is different than the other tests in this suite as Declarative Services is actually
        // installed and started rather than using mocks.
        TEST(ComponentConfigurationImplLogTest, LoadLibraryLogsMessagesNotImmediateTest)
        {
            auto framework = cppmicroservices::FrameworkFactory().NewFramework();
            framework.Start();
            ASSERT_TRUE(framework);

            auto context = framework.GetBundleContext();
            ASSERT_TRUE(context);

            test::InstallAndStartDS(context);

            // TestBundleDSTOI14 is immediate=false so this InstallAndStart should not
            // load the library until GetService is called. The logger is created after
            // this InstallAndStart so in the event that the log messages for loading the
            // shared library are sent when the library is loaded, the test will fail.
            test::InstallAndStartBundle(context, "TestBundleDSTOI14");

            // The logger should receive 2 Log() calls from the code which actually invokes
            // SharedLibrary::Load()
            //
            // The logger is registered after InstallAndStartBundle in this case to prove that
            // for non-immediate DS bundles, the log messages are sent when the library is actually
            // loaded (i.e., when a call to GetService is made for an interface implemented by
            // the bundle).
            auto logger = std::make_shared<MockLogger>();

            // This expectation is for the actual info log messages pertaining to loading of
            // the shared library.
            EXPECT_CALL(*logger, Log(logservice::SeverityLevel::LOG_INFO, ::testing::_)).Times(2);

            auto loggerReg = context.RegisterService<logservice::LogService>(logger);

            // The call to GetService should cause the library to be loaded, thus sending
            // the 2 expected log messages.
            auto sRef = context.GetServiceReference<test::Interface1>();
            ASSERT_TRUE(sRef);
            (void)context.GetService<test::Interface1>(sRef);

            loggerReg.Unregister();

            framework.Stop();
            framework.WaitForStop(std::chrono::milliseconds::zero());
        }

        std::tuple<cppmicroservices::Framework,
                   std::shared_ptr<metadata::ComponentMetadata>,
                   std::shared_ptr<MockComponentRegistry>,
                   std::shared_ptr<FakeLogger>,
                   std::shared_ptr<ConfigurationNotifier>,
                   std::shared_ptr<cppmicroservices::service::cm::ConfigurationAdmin>>
        mySetUp()
        {
            /**
             * LSAN is incorrectly flagging a lock inversion while using the
             * TestComponentConfigurationImpl fixture. We have therefore
             * intentionally not used that fixture
             */
            auto framework = cppmicroservices::FrameworkFactory().NewFramework();
            framework.Start();

            auto frameworkContext = framework.GetBundleContext();
            auto compMetadata = std::make_shared<metadata::ComponentMetadata>();
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto fakeLogger = std::make_shared<FakeLogger>();
            auto logger = std::make_shared<SCRLogger>(frameworkContext);
            auto extRegistry = std::make_shared<SCRExtensionRegistry>(logger);

            auto asyncWorkService = std::make_shared<SCRAsyncWorkService>(frameworkContext, logger);
            auto notifier
                = std::make_shared<ConfigurationNotifier>(frameworkContext, fakeLogger, asyncWorkService, extRegistry);

            // Publish ConfigurationListener
            auto configListener
                = std::make_shared<cppmicroservices::service::cm::ConfigurationListenerImpl>(frameworkContext,
                                                                                             logger,
                                                                                             notifier);
            auto configListenerReg
                = frameworkContext.RegisterService<cppmicroservices::service::cm::ConfigurationListener>(
                    std::move(configListener));

#    if defined(US_BUILD_SHARED_LIBS)
            auto cabundles = frameworkContext.InstallBundles(test::GetConfigAdminRuntimePluginFilePath());
            for (auto& bundle : cabundles)
            {
                bundle.Start();
            }
#    endif

            // Get a service reference to ConfigAdmin to create the configuration object.
            auto configAdminServiceRef
                = frameworkContext.GetServiceReference<cppmicroservices::service::cm::ConfigurationAdmin>();
            auto configAdminService = frameworkContext.GetService(configAdminServiceRef);

            return { framework, compMetadata, mockRegistry, fakeLogger, notifier, configAdminService };
        }
        /**
         * Test that the Modified method on a component configuration that is
         * lazily loaded and requires a configuration object to activate is never
         * called if the configuration is only created once and updated once.
         * Expected to be run repeatedly to verify race condition does not occur
         */
        TEST(ConfigAdminComponentCreationRace, TestModifiedIsNeverCalled)
        {
            auto [framework, compMetadata, mockRegistry, fakeLogger, notifier, configAdminService] = mySetUp();
            compMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
            compMetadata->immediate = true;
            compMetadata->configurationPolicy = "require";
            compMetadata->configurationPids = { "sample::config" };

            auto compConfig = std::make_shared<SingletonComponentConfigurationImpl>(compMetadata,
                                                                                    framework,
                                                                                    mockRegistry,
                                                                                    fakeLogger,
                                                                                    notifier);
            auto mockCompContext = std::make_shared<MockComponentContextImpl>(compConfig);
            auto mockCompInstance = std::make_shared<MockComponentInstance>();
            compConfig->SetComponentInstancePair(InstanceContextPair(mockCompInstance, mockCompContext));

            /**
             * Rather than testing a component with and without a modified method, we can just verify that
             * DoesModifiedMethodExist is never called. This is only, and always, called if we deem it acceptable to
             * either a) modify or b) deactivate the ComponentInstance. Therefore if we just block from calling this
             * check, it verifies behavior for ComponentInstance's with and without the modified method.
             */
            EXPECT_CALL(*mockCompInstance, DoesModifiedMethodExist()).Times(0);

            auto bundleT = std::thread([&compConfig]() { compConfig->Initialize(); });
            auto frameworkT = std::thread(
                [configAdminService = configAdminService]()
                {
                    auto configuration = configAdminService->GetConfiguration("sample::config");
                    auto fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                        {"foo", true}
                    });
                    fut.second.wait();
                });

            bundleT.join();
            frameworkT.join();

            compConfig->Deactivate();
            compConfig->Stop();

            framework.Stop();
            framework.WaitForStop(std::chrono::milliseconds::zero());
        }

        /**
         * Test that the Modified method on a component configuration that is
         * lazily loaded and requires two configuration objects to activate is never
         * called if one config is updated repeatedly prior to initialization and
         * the other is updated concurrently with initialization
         */
        TEST(ConfigAdminComponentCreationRace, TestMultipleConfigsNoModifiedCall)
        {
            auto [framework, compMetadata, mockRegistry, fakeLogger, notifier, configAdminService] = mySetUp();

            compMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
            compMetadata->immediate = true;
            compMetadata->configurationPolicy = "require";
            compMetadata->configurationPids = { "sample::config", "sample::config1" };

            auto compConfig = std::make_shared<SingletonComponentConfigurationImpl>(compMetadata,
                                                                                    framework,
                                                                                    mockRegistry,
                                                                                    fakeLogger,
                                                                                    notifier);
            auto mockCompContext = std::make_shared<MockComponentContextImpl>(compConfig);
            auto mockCompInstance = std::make_shared<MockComponentInstance>();
            compConfig->SetComponentInstancePair(InstanceContextPair(mockCompInstance, mockCompContext));

            /**
             * Rather than testing a component with and without a modified method, we can just verify that
             * DoesModifiedMethodExist is never called. This is only, and always, called if we deem it acceptable to
             * either a) modify or b) deactivate the ComponentInstance. Therefore if we just block from calling this
             * check, it verifies behavior for ComponentInstance's with and without the modified method.
             */
            EXPECT_CALL(*mockCompInstance, DoesModifiedMethodExist()).Times(0);
            auto configuration = configAdminService->GetConfiguration("sample::config1");
            auto fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                {"foo", true}
            });
            fut.second.wait();
            fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                {"foo", false}
            });
            fut.second.wait();
            fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                {"foo", true}
            });
            fut.second.wait();

            auto bundleT = std::thread([&compConfig]() { compConfig->Initialize(); });
            auto frameworkT = std::thread(
                [configAdminService = configAdminService]()
                {
                    auto configuration = configAdminService->GetConfiguration("sample::config");
                    auto fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                        {"foo1", true}
                    });
                    fut.second.wait();
                });

            bundleT.join();
            frameworkT.join();

            compConfig->Deactivate();
            compConfig->Stop();

            framework.Stop();
            framework.WaitForStop(std::chrono::milliseconds::zero());
        }

        /**
         * Test that the Modified method on a component configuration that is
         * lazily loaded and requires two configuration objects where one is
         * twice and the other once, the the component is initialized and then
         * the second config object is updated for the second time. This should trigger a modified() call.
         */
        TEST(ConfigAdminComponentCreationRace, TestMultipleConfigsModifiedCalled)
        {
            auto [framework, compMetadata, mockRegistry, fakeLogger, notifier, configAdminService] = mySetUp();

            compMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
            compMetadata->immediate = true;
            compMetadata->configurationPolicy = "require";
            compMetadata->configurationPids = { "sample::config", "sample::config1" };

            auto compConfig = std::make_shared<SingletonComponentConfigurationImpl>(compMetadata,
                                                                                    framework,
                                                                                    mockRegistry,
                                                                                    fakeLogger,
                                                                                    notifier);
            auto mockCompContext = std::make_shared<MockComponentContextImpl>(compConfig);
            auto mockCompInstance = std::make_shared<MockComponentInstance>();
            compConfig->SetComponentInstancePair(InstanceContextPair(mockCompInstance, mockCompContext));

            /**
             * Rather than testing a component with and without a modified method, we can just verify that
             * DoesModifiedMethodExist is never called. This is only, and always, called if we deem it acceptable to
             * either a) modify or b) deactivate the ComponentInstance. Therefore if we just block from calling this
             * check, it verifies behavior for ComponentInstance's with and without the modified method.
             */
            EXPECT_CALL(*mockCompInstance, DoesModifiedMethodExist()).Times(1);
            auto configuration = configAdminService->GetConfiguration("sample::config1");
            auto fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                {"foo", true}
            });
            fut.second.wait();
            fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                {"foo", false}
            });
            fut.second.wait();

            configuration = configAdminService->GetConfiguration("sample::config");
            fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                {"foo1", true}
            });
            fut.second.wait();
            // config change counts: {1, 2}
            compConfig->Initialize();
            fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                {"foo1", false}
            });
            fut.second.wait();
            // config change counts: {2, 2}

            compConfig->Deactivate();
            compConfig->Stop();

            framework.Stop();
            framework.WaitForStop(std::chrono::milliseconds::zero());
        }

        TEST(ConfigAdminComponentCreationRace, TestConfigNotifierSafeWithNoListenersForPid)
        {
            auto [framework, compMetadata, mockRegistry, fakeLogger, notifier, configAdminService] = mySetUp();

            compMetadata->serviceMetadata.interfaces = { us_service_interface_iid<dummy::ServiceImpl>() };
            compMetadata->immediate = true;
            compMetadata->configurationPolicy = "require";
            compMetadata->configurationPids = { "sample::config" };

            auto compConfig = std::make_shared<SingletonComponentConfigurationImpl>(compMetadata,
                                                                                    framework,
                                                                                    mockRegistry,
                                                                                    fakeLogger,
                                                                                    notifier);
            auto mockCompContext = std::make_shared<MockComponentContextImpl>(compConfig);
            auto mockCompInstance = std::make_shared<MockComponentInstance>();
            compConfig->SetComponentInstancePair(InstanceContextPair(mockCompInstance, mockCompContext));
            compConfig->Initialize();

            auto compConfig1 = std::make_shared<SingletonComponentConfigurationImpl>(compMetadata,
                                                                                     framework,
                                                                                     mockRegistry,
                                                                                     fakeLogger,
                                                                                     notifier);
            auto mockCompContext1 = std::make_shared<MockComponentContextImpl>(compConfig);
            auto mockCompInstance1 = std::make_shared<MockComponentInstance>();
            compConfig1->SetComponentInstancePair(InstanceContextPair(mockCompInstance, mockCompContext));
            compConfig1->Initialize();

            auto compConfig2 = std::make_shared<SingletonComponentConfigurationImpl>(compMetadata,
                                                                                     framework,
                                                                                     mockRegistry,
                                                                                     fakeLogger,
                                                                                     notifier);
            auto mockCompContext2 = std::make_shared<MockComponentContextImpl>(compConfig);
            auto mockCompInstance2 = std::make_shared<MockComponentInstance>();
            compConfig2->SetComponentInstancePair(InstanceContextPair(mockCompInstance, mockCompContext));
            compConfig2->Initialize();

            auto compConfig3 = std::make_shared<SingletonComponentConfigurationImpl>(compMetadata,
                                                                                     framework,
                                                                                     mockRegistry,
                                                                                     fakeLogger,
                                                                                     notifier);
            auto mockCompContext3 = std::make_shared<MockComponentContextImpl>(compConfig);
            auto mockCompInstance3 = std::make_shared<MockComponentInstance>();
            compConfig3->SetComponentInstancePair(InstanceContextPair(mockCompInstance, mockCompContext));
            compConfig3->Initialize();
            test::Barrier sync_point(5); // 5 threads to synchronize

            auto frameworkT = std::async(
                std::launch::async,
                [configAdminService = configAdminService, &sync_point]()
                {
                    sync_point.Wait(); // Wait for all threads to reach this point
                    auto configuration = configAdminService->GetConfiguration("sample::config");
                    auto fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                        {"foo", true}
                    });
                    fut.second.wait();
                    fut = configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                        {"foo", false}
                    });
                    fut.second.wait();
                    configuration->UpdateIfDifferent(std::unordered_map<std::string, cppmicroservices::Any> {
                        {"foo", true}
                    });
                });

            auto bundleT = std::async(std::launch::async,
                                      [&sync_point, &compConfig]()
                                      {
                                          sync_point.Wait(); // Wait for all threads to reach this point
                                          compConfig->Stop();
                                      });

            auto bundleT1 = std::async(std::launch::async,
                                       [&sync_point, &compConfig1]()
                                       {
                                           sync_point.Wait(); // Wait for all threads to reach this point
                                           compConfig1->Stop();
                                       });

            auto bundleT2 = std::async(std::launch::async,
                                       [&sync_point, &compConfig2]()
                                       {
                                           sync_point.Wait(); // Wait for all threads to reach this point
                                           compConfig2->Stop();
                                       });

            auto bundleT3 = std::async(std::launch::async,
                                       [&sync_point, &compConfig3]()
                                       {
                                           sync_point.Wait(); // Wait for all threads to reach this point
                                           compConfig3->Stop();
                                       });

            frameworkT.wait();
            bundleT.wait();
            bundleT1.wait();
            bundleT2.wait();
            bundleT3.wait();

            compConfig->Deactivate();
            compConfig->Stop();

            compConfig1->Deactivate();
            compConfig1->Stop();

            compConfig2->Deactivate();
            compConfig2->Stop();

            compConfig3->Deactivate();
            compConfig3->Stop();

            framework.Stop();
            framework.WaitForStop(std::chrono::milliseconds::zero());
        }
#endif
    } // namespace scrimpl
} // namespace cppmicroservices
