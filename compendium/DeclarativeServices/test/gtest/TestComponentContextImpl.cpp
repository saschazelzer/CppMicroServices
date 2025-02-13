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

#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <string>
#include <vector>

#include "../../src/ComponentContextImpl.hpp"
#include "Mocks.hpp"
#include "cppmicroservices/servicecomponent/ComponentConstants.hpp"
#include "cppmicroservices/servicecomponent/ComponentException.hpp"
#include "gmock/gmock.h"
#include <cppmicroservices/Constants.h>
#include <cppmicroservices/Framework.h>
#include <cppmicroservices/FrameworkEvent.h>
#include <cppmicroservices/FrameworkFactory.h>

#include "../TestUtils.hpp"

using cppmicroservices::Constants::SCOPE_BUNDLE;
using cppmicroservices::Constants::SCOPE_PROTOTYPE;
using cppmicroservices::Constants::SCOPE_SINGLETON;
using cppmicroservices::scrimpl::metadata::ComponentMetadata;
using cppmicroservices::service::component::ComponentContext;
using cppmicroservices::service::component::ComponentException;
using cppmicroservices::service::component::ComponentConstants::REFERENCE_SCOPE_PROTOTYPE_REQUIRED;

namespace cppmicroservices
{
    namespace scrimpl
    {

        // The fixture for testing class ComponentContextImpl.
        class ComponentContextImplTest : public ::testing::Test
        {
          protected:
            ComponentContextImplTest() : framework(cppmicroservices::FrameworkFactory().NewFramework()) {}
            virtual ~ComponentContextImplTest() = default;

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

        namespace test
        {
            struct Foo
            {
            };
            struct Bar
            {
            };
        } // namespace test

        TEST_F(ComponentContextImplTest, VerifyInvalidCtor)
        {
            EXPECT_THROW(
                {
                    std::shared_ptr<ComponentContext> ctxt
                        = std::make_shared<ComponentContextImpl>(std::weak_ptr<ComponentConfiguration>());
                },
                ComponentException);
            EXPECT_THROW(
                {
                    std::shared_ptr<ComponentContext> ctxt
                        = std::make_shared<ComponentContextImpl>(std::weak_ptr<ComponentConfiguration>(),
                                                                 GetFramework());
                },
                ComponentException);
        }

        auto anyMapComparator = [](std::pair<std::string, cppmicroservices::Any> const& x,
                                   std::pair<std::string, cppmicroservices::Any> const& y) -> bool
        {
            auto ret = ((x.first == y.first) && (x.second.ToStringNoExcept() == y.second.ToStringNoExcept()));
            return ret;
        };

        TEST_F(ComponentContextImplTest, VerifyComponentProperties)
        {
            auto mockConfig = std::make_shared<testing::NiceMock<MockComponentConfiguration>>();
            std::shared_ptr<ComponentContext> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            std::unordered_map<std::string, cppmicroservices::Any> fakeProps;
            fakeProps["foo"] = std::string("bar");
            EXPECT_CALL(*mockConfig, GetProperties()).Times(1).WillRepeatedly(testing::Return(fakeProps));
            EXPECT_NO_THROW({
                auto props = ctxt->GetProperties();
                bool compresult
                    = std::equal(props.begin(), props.end(), fakeProps.begin(), fakeProps.end(), anyMapComparator);
                EXPECT_EQ(compresult, true);
            });
        }

        TEST_F(ComponentContextImplTest, VerifyLocateService)
        {
            auto mockConfig = std::make_shared<MockComponentConfiguration>();
            auto fooServ = std::make_shared<test::Foo>();
            auto mockRefMgrFoo = std::make_shared<MockReferenceManager>();
            auto reg = GetFramework().GetBundleContext().RegisterService<test::Foo>(fooServ);
            std::set<cppmicroservices::ServiceReferenceBase> refsSet = { reg.GetReference() };
            EXPECT_CALL(*mockRefMgrFoo, GetBoundReferences()).Times(1).WillRepeatedly(testing::Return(refsSet));
            EXPECT_CALL(*mockRefMgrFoo, GetReferenceName()).Times(1).WillRepeatedly(testing::Return("foo"));
            EXPECT_CALL(*mockConfig, GetBundle()).WillRepeatedly(testing::Return(GetFramework()));
            std::vector<std::shared_ptr<ReferenceManager>> depMgrs { mockRefMgrFoo };
            EXPECT_CALL(*mockConfig, GetAllDependencyManagers()).Times(1).WillRepeatedly(testing::Return(depMgrs));
            std::shared_ptr<ComponentContext> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            EXPECT_NO_THROW({
                auto serv = ctxt->LocateService<test::Foo>("foo");
                EXPECT_EQ(serv, fooServ); // test with correct type and name
                auto serv1 = ctxt->LocateService<test::Foo>("bar");
                EXPECT_EQ(serv1, nullptr); // test with correct type and wrong name
                auto serv2 = ctxt->LocateService<test::Bar>("foo");
                EXPECT_EQ(serv2, nullptr); // test with wrong type and correct name
            });
        }

        /**
         * This test point creates mutiple component configs per available bundle and
         * calls LocateService multiple times on each ComponentContext
         *
         * Test is DISABLED because the usage of reference scopes is not supported. Re-enable when
         * the feature is done.
         */
        TEST_F(ComponentContextImplTest, DISABLED_VerifyLocateServiceWithReferenceScopes)
        {
            size_t iterCount = 4ul;
            size_t componentInstanceCount = 3ul;
            auto bc = GetFramework().GetBundleContext();
            auto VerifyLocateServiceWithScopes
                = [&](std::string referenceScope, std::string publishedScope, size_t expectedServiceInstanceCount)
            {
                auto mockConfig = std::make_shared<MockComponentConfiguration>();

                auto fakeLogger = std::make_shared<FakeLogger>();
                metadata::ReferenceMetadata fakeRefMetadata;
                fakeRefMetadata.name = "foo";
                fakeRefMetadata.interfaceName = us_service_interface_iid<dummy::ServiceImpl>();
                fakeRefMetadata.scope = referenceScope;

                std::set<std::shared_ptr<dummy::ServiceImpl>> instances;

                // publish a service
                auto mockServiceFactory = std::make_shared<MockFactory>();
                InterfaceMapConstPtr singletonInstance
                    = MakeInterfaceMap<dummy::ServiceImpl>(std::make_shared<dummy::ServiceImpl>());
                EXPECT_CALL(*mockServiceFactory, GetService(testing::_, testing::_))
                    .WillRepeatedly(testing::Invoke(
                        [&](const cppmicroservices::Bundle&, const cppmicroservices::ServiceRegistrationBase&)
                        {
                            if (publishedScope == SCOPE_SINGLETON)
                            {
                                return singletonInstance;
                            }
                            else
                            {
                                InterfaceMapConstPtr iMap
                                    = MakeInterfaceMap<dummy::ServiceImpl>(std::make_shared<dummy::ServiceImpl>());
                                return iMap;
                            }
                        }));

                EXPECT_CALL(*mockServiceFactory, UngetService(testing::_, testing::_, testing::_))
                    .WillRepeatedly(testing::Invoke(
                        [&](const cppmicroservices::Bundle&,
                            const cppmicroservices::ServiceRegistrationBase&,
                            const cppmicroservices::InterfaceMapConstPtr& service)
                        {
                            if (publishedScope == SCOPE_SINGLETON)
                            {
                                ASSERT_EQ(service, singletonInstance);
                            }
                        }));

                auto sReg = bc.RegisterService<dummy::ServiceImpl>(
                    ToFactory(mockServiceFactory),
                    ServiceProperties({
                        {cppmicroservices::Constants::SERVICE_SCOPE, Any(publishedScope)}
                }));

                auto bundleContext = GetFramework().GetBundleContext();
                auto mockRefMgrFoo
                    = std::make_shared<ReferenceManagerImpl>(fakeRefMetadata, bundleContext, fakeLogger, "foobar");
                std::vector<std::shared_ptr<ReferenceManager>> depMgrs { mockRefMgrFoo };
                EXPECT_CALL(*mockConfig, GetAllDependencyManagers()).WillRepeatedly(testing::Return(depMgrs));
                EXPECT_CALL(*mockConfig, GetBundle()).WillRepeatedly(testing::Return(GetFramework()));
                EXPECT_CALL(*mockConfig, GetMetadata()).WillRepeatedly(::testing::Return(std::make_shared<metadata::ComponentMetadata>()));

                // component context has 1:1 relation with component instance, so we
                // use the context to validate the number of instances of a reference
                std::vector<std::shared_ptr<ComponentContext>> contexts;
                for (size_t i = 0; i < componentInstanceCount; i++)
                {
                    // ComponentContextImpl ctor can throw if there are mandatory service references that are
                    // not bound (i.e. unsatisfied). This test doesn't care about whether or not 
                    // ComponentContextimpl throws, so catch and ignore the exception.
                    try 
                    {
                        contexts.push_back(std::make_shared<ComponentContextImpl>(mockConfig));
                    } 
                    catch(...) 
                    {
                    }
                }

                for (size_t i = 0; i < iterCount; i++)
                {
                    for (auto ctxt : contexts)
                    {
                        auto serviceObj = ctxt->LocateService<dummy::ServiceImpl>("foo");
                        if (serviceObj)
                        {
                            instances.insert(serviceObj);
                        }
                    }
                }

                EXPECT_TRUE(std::none_of(instances.begin(),
                                         instances.end(),
                                         [](const std::shared_ptr<dummy::ServiceImpl>& sObj)
                                         { return (sObj == nullptr); }));
                EXPECT_EQ(instances.size(), expectedServiceInstanceCount);
                sReg.Unregister();
            };

            // See
            // https://osgi.org/specification/osgi.cmpn/7.0.0/service.component.html#service.component-reference.scope
            // If the reference scope is BUNDLE, and service is published with SINGLETON
            // scope all component instances must receive the same instance of the reference
            VerifyLocateServiceWithScopes(SCOPE_BUNDLE, SCOPE_SINGLETON, 1ul);
            // If the reference scope is BUNDLE, and service is published with BUNDLE
            // scope all component instances within the same bundle must receive the
            // same instance of the reference. Components from different bundles must
            // receive distinct instances
            VerifyLocateServiceWithScopes(SCOPE_BUNDLE, SCOPE_BUNDLE, 1ul);
            // If the reference scope is BUNDLE, and service is published with PROTOTYPE
            // this is no different from the case where the service is published with
            // BUNDLE scope
            VerifyLocateServiceWithScopes(SCOPE_BUNDLE, SCOPE_PROTOTYPE, 1ul);

            // If the reference scope is PROTOTYPE, and service is published with SINGLETON
            // scope all component instances must receive the same instance of the reference
            VerifyLocateServiceWithScopes(SCOPE_PROTOTYPE, SCOPE_SINGLETON, 1ul);
            // If the reference scope is PROTOTYPE, and service is published with BUNDLE
            // scope all component instances within the same bundle must receive the
            // same instance of the reference. Components from different bundles must
            // receive distinct instances
            VerifyLocateServiceWithScopes(SCOPE_PROTOTYPE, SCOPE_BUNDLE, 1ul);
            // If the reference scope is PROTOTYPE, and service is published with PROTOTYPE
            // scope each component instance must receive a unique instance of the reference
            // service
            VerifyLocateServiceWithScopes(SCOPE_PROTOTYPE, SCOPE_PROTOTYPE, componentInstanceCount);

            // If the reference scope is PROTOTYPE_REQUIRED, and service is published
            // with SINGLETON scope, the reference remains unsatisfied
            VerifyLocateServiceWithScopes(REFERENCE_SCOPE_PROTOTYPE_REQUIRED, SCOPE_SINGLETON, 0ul);
            // If the reference scope is PROTOTYPE_REQUIRED, and service is published
            // with BUNDLE scope, the reference remains unsatisfied
            VerifyLocateServiceWithScopes(REFERENCE_SCOPE_PROTOTYPE_REQUIRED, SCOPE_BUNDLE, 0ul);
            // If the reference scope is PROTOTYPE_REQUIRED, and service is published
            // with PROTOTYPE scope each component instance must receive a unique
            // instance of the reference service
            VerifyLocateServiceWithScopes(REFERENCE_SCOPE_PROTOTYPE_REQUIRED,
                                          SCOPE_PROTOTYPE,
                                          componentInstanceCount);
        }

        TEST_F(ComponentContextImplTest, VerifyLocateServiceWithHighestRank)
        {
            auto mockConfig = std::make_shared<MockComponentConfiguration>();
            auto fooServ = std::make_shared<test::Foo>();
            auto mockRefMgrFoo = std::make_shared<MockReferenceManager>();
            auto reg = GetFramework().GetBundleContext().RegisterService<test::Foo>(fooServ);
            auto fooServ1 = std::make_shared<test::Foo>();
            auto reg1 = GetFramework().GetBundleContext().RegisterService<test::Foo>(
                fooServ1,
                ServiceProperties({
                    {cppmicroservices::Constants::SERVICE_RANKING, 20}
            }));
            auto refs = GetFramework().GetBundleContext().GetServiceReferences(us_service_interface_iid<test::Foo>());
            std::set<cppmicroservices::ServiceReferenceBase> refsSet;
            refsSet.insert(refs.begin(), refs.end());
            EXPECT_CALL(*mockRefMgrFoo, GetBoundReferences()).Times(1).WillRepeatedly(testing::Return(refsSet));
            EXPECT_CALL(*mockRefMgrFoo, GetReferenceName()).Times(1).WillRepeatedly(testing::Return("foo"));
            EXPECT_CALL(*mockConfig, GetBundle()).WillRepeatedly(testing::Return(GetFramework()));
            std::vector<std::shared_ptr<ReferenceManager>> depMgrs { mockRefMgrFoo };
            EXPECT_CALL(*mockConfig, GetAllDependencyManagers()).Times(1).WillRepeatedly(testing::Return(depMgrs));
            std::shared_ptr<ComponentContext> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            EXPECT_NO_THROW({
                auto service = ctxt->LocateService<test::Foo>("foo");
                EXPECT_NE(service.get(), fooServ.get());
                EXPECT_EQ(service.get(), fooServ1.get()); // test with correct type and name
            });
        }

        TEST_F(ComponentContextImplTest, VerifyLocateServiceWithLowestId)
        {
            auto mockConfig = std::make_shared<MockComponentConfiguration>();
            auto mockRefMgrFoo = std::make_shared<MockReferenceManager>();
            auto fooServ = std::make_shared<test::Foo>();
            auto reg = GetFramework().GetBundleContext().RegisterService<test::Foo>(fooServ);
            auto fooServ1 = std::make_shared<test::Foo>();
            auto reg1 = GetFramework().GetBundleContext().RegisterService<test::Foo>(fooServ1);

            std::vector<cppmicroservices::ServiceReferenceBase> refArr;
            auto refs = GetFramework().GetBundleContext().GetServiceReferences(us_service_interface_iid<test::Foo>());
            std::set<cppmicroservices::ServiceReferenceBase> refsSet;
            refsSet.insert(refs.begin(), refs.end());
            EXPECT_CALL(*mockRefMgrFoo, GetBoundReferences()).Times(1).WillRepeatedly(testing::Return(refsSet));
            EXPECT_CALL(*mockRefMgrFoo, GetReferenceName()).Times(1).WillRepeatedly(testing::Return("foo"));
            EXPECT_CALL(*mockConfig, GetBundle()).WillRepeatedly(testing::Return(GetFramework()));
            std::vector<std::shared_ptr<ReferenceManager>> depMgrs { mockRefMgrFoo };
            EXPECT_CALL(*mockConfig, GetAllDependencyManagers()).Times(1).WillRepeatedly(testing::Return(depMgrs));

            std::shared_ptr<ComponentContext> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            EXPECT_NO_THROW({
                auto service = ctxt->LocateService<test::Foo>("foo");
                EXPECT_EQ(service, fooServ); // test with correct type and name
            });
        }

        // Test that calling LocateService with an empty type string returns the service associated
        // with the service reference name.
        TEST_F(ComponentContextImplTest, VerifyLocateServiceWithEmptyInterfaceTypeString)
        {
            auto mockCompConfig = std::make_shared<MockComponentConfiguration>();

            ON_CALL(*mockCompConfig, GetBundle).WillByDefault(::testing::Return(GetFramework()));
            auto registeredFooSvc = std::make_shared<test::Foo>();
            auto fooServiceReg = GetFramework().GetBundleContext().RegisterService<test::Foo>(registeredFooSvc);

            auto compCtx = std::make_shared<ComponentContextImpl>(mockCompConfig, GetFramework());
            ASSERT_TRUE(compCtx->AddToBoundServicesCache("foo", fooServiceReg.GetReference()));
            ASSERT_NO_THROW(
                {
                    auto fooSvc = compCtx->LocateService("foo", "");
                    ASSERT_TRUE(fooSvc == registeredFooSvc);
                });

            ASSERT_NO_THROW(fooServiceReg.Unregister());
        }

        // Test that if a nullptr service makes it into the bound services cache, for example
        // if the service is unregistered on another thread, that LocateService will throw an exception.
        TEST_F(ComponentContextImplTest, VerifyLocateServiceWithNullPtrService)
        {
            // publish a service
            auto mockServiceFactory = std::make_shared<MockFactory>();

            EXPECT_CALL(*mockServiceFactory, GetService(testing::_, testing::_))
                .WillRepeatedly(testing::Invoke(
                    [&](const cppmicroservices::Bundle&, const cppmicroservices::ServiceRegistrationBase&)
                    {
                        std::unordered_map<std::string, std::shared_ptr<void>> nullptrSvcMap;
                        nullptrSvcMap.insert({"cppmicroservices::scrimpl::dummy::ServiceImpl", nullptr});
                        return std::make_shared<InterfaceMap const>(std::move(nullptrSvcMap));
                    }));

            EXPECT_CALL(*mockServiceFactory, UngetService(testing::_, testing::_, testing::_))
                .WillRepeatedly(testing::Invoke(
                    [&](const cppmicroservices::Bundle&,
                        const cppmicroservices::ServiceRegistrationBase&,
                        const cppmicroservices::InterfaceMapConstPtr& )
                    {
                    }));

            auto sReg = GetFramework().GetBundleContext().RegisterService<dummy::ServiceImpl>(
                ToFactory(mockServiceFactory),
                ServiceProperties({
                    {cppmicroservices::Constants::SERVICE_SCOPE, cppmicroservices::Constants::SCOPE_BUNDLE}
            }));

            auto mockCompConfig = std::make_shared<MockComponentConfiguration>();
            ON_CALL(*mockCompConfig, GetBundle).WillByDefault(::testing::Return(GetFramework()));
            ON_CALL(*mockCompConfig, GetMetadata).WillByDefault(::testing::Return(std::make_shared<metadata::ComponentMetadata>()));

            auto compCtx = std::make_shared<ComponentContextImpl>(mockCompConfig, GetFramework());

            ASSERT_TRUE(compCtx->AddToBoundServicesCache("foo", sReg.GetReference()));
            ASSERT_THROW(compCtx->LocateService("foo", "cppmicroservices::scrimpl::dummy::ServiceImpl"), ComponentException);

            ASSERT_NO_THROW(sReg.Unregister());
        }

        // Test that LocateService returns nullptr if a DS service failed to be constructed and
        // activated and if that service was added to the cache of bound services for the service component.
        TEST_F(ComponentContextImplTest, VerifyLocateServiceOnFailedServiceObjectConstruction)
        {
            ::test::InstallAndStartDS(GetFramework().GetBundleContext());
            auto testBundle = ::test::InstallAndStartBundle(GetFramework().GetBundleContext(), "TestBundleDSUpstreamDependencyA");
            auto svcRef = testBundle.GetBundleContext().GetServiceReference("test::TestBundleDSUpstreamDependency");
            auto svc = testBundle.GetBundleContext().GetService(svcRef);

            ASSERT_TRUE(!svc);

            auto mockCompConfig = std::make_shared<MockComponentConfiguration>();
            ON_CALL(*mockCompConfig, GetBundle).WillByDefault(::testing::Return(GetFramework()));

            auto compCtx = std::make_shared<ComponentContextImpl>(mockCompConfig, GetFramework());

            ASSERT_FALSE(compCtx->AddToBoundServicesCache("foo", svcRef));
            ASSERT_TRUE(nullptr == compCtx->LocateService("foo", "test::TestBundleDSUpstreamDependency"));
        }


        /// Simulate a service instance being constructed with a missing mandatory service dependency
        /// The construction of the ComponentContext object should throw for a service with a static
        /// mandatory service dependency. 
        TEST_F(ComponentContextImplTest, VerifyConstructComponentContextWithMissingMandatoryServiceDependency)
        {
            auto mockCompConfig = std::make_shared<MockComponentConfiguration>();
            auto mockRefManager = std::make_shared<MockReferenceManager>();

            // set the default actions of MockReferenceManager and MockComponentConfiguration to simulate
            // constructing a ComponentContext object with data that should trigger an error condition (i.e. an exception).
            auto fooSvcReg = GetFramework().GetBundleContext().RegisterService<test::Foo>(std::make_shared<test::Foo>());
            std::set<cppmicroservices::ServiceReferenceBase> svcRefsSet;
            svcRefsSet.insert(fooSvcReg.GetReference());
            ON_CALL(*mockRefManager, GetBoundReferences).WillByDefault(::testing::Return(svcRefsSet));
            ON_CALL(*mockRefManager, GetReferenceScope).WillByDefault(::testing::Return(cppmicroservices::Constants::SCOPE_BUNDLE));
            ON_CALL(*mockRefManager, IsOptional).WillByDefault(::testing::Return(false));

            std::vector<std::shared_ptr<ReferenceManager>> refManagers;
            refManagers.push_back(std::move(mockRefManager));
            ON_CALL(*mockCompConfig, GetAllDependencyManagers).WillByDefault(::testing::Return(refManagers));
            ON_CALL(*mockCompConfig, GetBundle).WillByDefault(::testing::Return(GetFramework()));
            ON_CALL(*mockCompConfig, GetMetadata).WillByDefault(::testing::Return(std::make_shared<metadata::ComponentMetadata>()));

            // cause the service reference to become invalid for the component context constructor
            fooSvcReg.Unregister();
            ASSERT_THROW(auto compCtx = std::make_shared<ComponentContextImpl>(mockCompConfig), ComponentException);
        }

        /// Simulate a service instance being constructed with a missing optional service dependency
        /// The construction of the ComponentContext object should succeed for a service with a missing
        /// static optional service dependency. 
        TEST_F(ComponentContextImplTest, VerifyConstructComponentContextWithMissingOptionalServiceDependency)
        {
            auto mockCompConfig = std::make_shared<MockComponentConfiguration>();
            auto mockRefManager = std::make_shared<MockReferenceManager>();

            // set the default actions of MockReferenceManager and MockComponentConfiguration to simulate
            // constructing a ComponentContext object with data that should not trigger an error condition (i.e. an exception).
            auto fooSvcReg = GetFramework().GetBundleContext().RegisterService<test::Foo>(std::make_shared<test::Foo>());
            std::set<cppmicroservices::ServiceReferenceBase> svcRefsSet;
            svcRefsSet.insert(fooSvcReg.GetReference());
            ON_CALL(*mockRefManager, GetBoundReferences).WillByDefault(::testing::Return(svcRefsSet));
            ON_CALL(*mockRefManager, GetReferenceScope).WillByDefault(::testing::Return(cppmicroservices::Constants::SCOPE_BUNDLE));
            ON_CALL(*mockRefManager, IsOptional).WillByDefault(::testing::Return(true));

            std::vector<std::shared_ptr<ReferenceManager>> refManagers;
            refManagers.push_back(std::move(mockRefManager));
            ON_CALL(*mockCompConfig, GetAllDependencyManagers).WillByDefault(::testing::Return(refManagers));
            ON_CALL(*mockCompConfig, GetBundle).WillByDefault(::testing::Return(GetFramework()));
            ON_CALL(*mockCompConfig, GetMetadata).WillByDefault(::testing::Return(std::make_shared<metadata::ComponentMetadata>()));

            // cause the service reference to become invalid for the component context constructor
            // since the service reference is optional an invalid service reference is acceptable
            // and the component context construction should succeed.
            fooSvcReg.Unregister();
            ASSERT_NO_THROW(auto compCtx = std::make_shared<ComponentContextImpl>(mockCompConfig));
        }

        TEST_F(ComponentContextImplTest, VerifyLocateServices)
        {
            auto mockConfig = std::make_shared<MockComponentConfiguration>();
            auto fooServ = std::make_shared<test::Foo>();
            auto mockRefMgrFoo = std::make_shared<MockReferenceManager>();
            auto reg = GetFramework().GetBundleContext().RegisterService<test::Foo>(fooServ);
            auto fooServ1 = std::make_shared<test::Foo>();
            auto reg1 = GetFramework().GetBundleContext().RegisterService<test::Foo>(
                fooServ1,
                ServiceProperties({
                    {cppmicroservices::Constants::SERVICE_RANKING, 20}
            }));
            auto refs = GetFramework().GetBundleContext().GetServiceReferences(us_service_interface_iid<test::Foo>());
            std::set<cppmicroservices::ServiceReferenceBase> refsSet;
            refsSet.insert(refs.begin(), refs.end());
            EXPECT_CALL(*mockRefMgrFoo, GetBoundReferences()).Times(1).WillRepeatedly(testing::Return(refsSet));
            EXPECT_CALL(*mockRefMgrFoo, GetReferenceName()).Times(1).WillRepeatedly(testing::Return("foo"));
            EXPECT_CALL(*mockConfig, GetBundle()).WillRepeatedly(testing::Return(GetFramework()));
            std::vector<std::shared_ptr<ReferenceManager>> depMgrs { mockRefMgrFoo };
            EXPECT_CALL(*mockConfig, GetAllDependencyManagers()).Times(1).WillRepeatedly(testing::Return(depMgrs));
            std::shared_ptr<ComponentContext> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            EXPECT_NO_THROW({
                auto services = ctxt->LocateServices<test::Foo>("foo");
                EXPECT_EQ(services.size(), refs.size()); // test with correct type and name
            });
        }

        TEST_F(ComponentContextImplTest, VerifyUsingBundle)
        {
            auto mockConfig = std::make_shared<testing::NiceMock<MockComponentConfiguration>>();
            // The context created without a usingBundle will always return an invalid bundle.
            std::shared_ptr<ComponentContext> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            EXPECT_EQ(ctxt->GetUsingBundle().operator bool(), false);

            // The context created with a usingBundle must return the passed argument.
            std::shared_ptr<ComponentContext> ctxt1
                = std::make_shared<ComponentContextImpl>(mockConfig, GetFramework());
            EXPECT_EQ(ctxt1->GetUsingBundle().operator bool(), true);
            EXPECT_EQ(ctxt1->GetUsingBundle(), GetFramework());
        }

        TEST_F(ComponentContextImplTest, VerifyEnableComponent)
        {
            auto mockConfig = std::make_shared<testing::NiceMock<MockComponentConfiguration>>();
            std::shared_ptr<ComponentContext> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto mockCompMgr = std::make_shared<MockComponentManager>();
            EXPECT_CALL(*mockConfig, GetBundle()).Times(1).WillRepeatedly(testing::Return(GetFramework()));
            EXPECT_CALL(*mockConfig, GetRegistry()).Times(1).WillRepeatedly(testing::Return(mockRegistry));
            EXPECT_CALL(*mockRegistry, GetComponentManager(GetFramework().GetBundleId(), "comp::name"))
                .Times(1)
                .WillRepeatedly(testing::Return(mockCompMgr));
            EXPECT_CALL(*mockCompMgr, Enable(testing::_)).Times(1);
            ctxt->EnableComponent("comp::name");
        }

        TEST_F(ComponentContextImplTest, VerifyEnableComponentEmptyArg)
        {
            auto mockConfig = std::make_shared<testing::NiceMock<MockComponentConfiguration>>();
            std::shared_ptr<ComponentContext> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto mockCompMgr = std::make_shared<MockComponentManager>();
            auto mockCompMgr1 = std::make_shared<MockComponentManager>();
            auto mockCompMgr2 = std::make_shared<MockComponentManager>();
            std::vector<std::shared_ptr<ComponentManager>> compmgrs = { mockCompMgr, mockCompMgr1, mockCompMgr2 };
            EXPECT_CALL(*mockConfig, GetBundle()).Times(1).WillRepeatedly(testing::Return(GetFramework()));
            EXPECT_CALL(*mockConfig, GetRegistry()).Times(1).WillRepeatedly(testing::Return(mockRegistry));
            EXPECT_CALL(*mockRegistry, GetComponentManagers(GetFramework().GetBundleId()))
                .Times(1)
                .WillRepeatedly(testing::Return(compmgrs));
            EXPECT_CALL(*mockCompMgr, Enable(testing::_)).Times(1);
            EXPECT_CALL(*mockCompMgr1, Enable(testing::_)).Times(1);
            EXPECT_CALL(*mockCompMgr2, Enable(testing::_)).Times(1);
            ctxt->EnableComponent("");
        }

        TEST_F(ComponentContextImplTest, VerifyDisableComponent)
        {
            auto mockConfig = std::make_shared<testing::NiceMock<MockComponentConfiguration>>();
            std::shared_ptr<ComponentContext> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            auto mockRegistry = std::make_shared<MockComponentRegistry>();
            auto mockCompMgr = std::make_shared<MockComponentManager>();
            EXPECT_CALL(*mockConfig, GetBundle()).Times(1).WillRepeatedly(testing::Return(GetFramework()));
            EXPECT_CALL(*mockConfig, GetRegistry()).Times(1).WillRepeatedly(testing::Return(mockRegistry));
            EXPECT_CALL(*mockRegistry, GetComponentManager(GetFramework().GetBundleId(), "comp::name"))
                .Times(1)
                .WillRepeatedly(testing::Return(mockCompMgr));
            EXPECT_CALL(*mockCompMgr, Disable(testing::_)).Times(1);
            ctxt->DisableComponent("comp::name");
        }

        TEST_F(ComponentContextImplTest, VerifyInvalidate)
        {
            auto mockConfig = std::make_shared<testing::NiceMock<MockComponentConfiguration>>();
            // The context created without a usingBundle will always return an invalid bundle.
            std::shared_ptr<ComponentContextImpl> ctxt = std::make_shared<ComponentContextImpl>(mockConfig);
            EXPECT_CALL(*mockConfig, GetBundle()).WillRepeatedly(testing::Return(GetFramework()));
            EXPECT_EQ(ctxt->GetBundleContext().operator bool(), true);
            ctxt->Invalidate();
            EXPECT_THROW(ctxt->GetProperties(), ComponentException);
            EXPECT_THROW(ctxt->EnableComponent("foo"), ComponentException);
            EXPECT_THROW(ctxt->DisableComponent("foo"), ComponentException);
            EXPECT_EQ(ctxt->GetBundleContext().operator bool(), false);
            EXPECT_EQ(ctxt->GetServiceReference().operator bool(), false);
        }

        TEST(TestComponentContextImpl, TestInvalidComponentConfiguration)
        {
            auto mockConfig = std::make_shared<MockComponentConfiguration>();
            // perform some object ownership gymnastics so that ComponentContextImpl
            // can be constructed with a valid ComponentConfiguration and then destroy the
            // ComponentConfiguration object such that any subsequent ComponentContext 
            // calls fail because the ComponentConfiguration is no longer valid.
            std::weak_ptr<MockComponentConfiguration> weakMockCompConfig;
            weakMockCompConfig = mockConfig;
            auto compCtx = std::make_shared<ComponentContextImpl>(weakMockCompConfig);
            mockConfig.reset();
            
            ASSERT_THROW(compCtx->LocateService("", ""), ComponentException);
            ASSERT_THROW(compCtx->LocateServices("", ""), ComponentException);
        }
    } // namespace scrimpl
} // namespace cppmicroservices
