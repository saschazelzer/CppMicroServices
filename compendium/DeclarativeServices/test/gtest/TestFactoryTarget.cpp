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

#include "../../src/manager/ComponentFactoryImpl.hpp"
#include "../../src/manager/ConfigurationNotifier.hpp"
#include "../../src/manager/SingletonComponentConfiguration.hpp"
#include "../../src/metadata/ComponentMetadata.hpp"
#include "../../src/SCRExtensionRegistry.hpp"
#include "../../src/SCRAsyncWorkService.hpp"
#include "cppmicroservices/asyncworkservice/AsyncWorkService.hpp"
#include "ConcurrencyTestUtil.hpp"
#include "cppmicroservices/LDAPFilter.h"
#include "cppmicroservices/logservice/LogService.hpp"
#include "cppmicroservices/servicecomponent/ComponentConstants.hpp"
#include "cppmicroservices/ServiceTracker.h"
#include "gtest/gtest.h"
#include "Mocks.hpp"
#include "TestFixture.hpp"
#include <TestInterfaces/Interfaces.hpp>
namespace test
{
#define TIMEOUT_HALF_SECOND std::chrono::milliseconds(500)

    class tFactoryTarget : public tGenericDSAndCASuite
    {
      public:
        template <typename ServiceInterfaceT>
        std::shared_ptr<ServiceInterfaceT>
        getFactoryService(std::string factoryPid, std::string factoryInstance, cppmicroservices::AnyMap const& props)
        {

            // Get a service reference to ConfigAdmin to create the factory component instance.

            if (configAdmin)
            {
                std::shared_ptr<cppmicroservices::service::cm::Configuration> factoryConfig;
                // Create factory configuration object
                if (factoryInstance.empty())
                {
                    factoryConfig = configAdmin->CreateFactoryConfiguration(factoryPid);
                }
                else
                {
                    factoryConfig = configAdmin->GetFactoryConfiguration(factoryPid, factoryInstance);
                }

                if (factoryConfig)
                {
                    auto componentName = factoryConfig->GetPid();
                    auto fut = factoryConfig->Update(props);
                    fut.get();
                    std::string filter = "(" + cppmicroservices::service::component::ComponentConstants::COMPONENT_NAME
                                         + "=" + componentName + ")";
                    auto services = context.GetServiceReferences<ServiceInterfaceT>(filter);
                    if (services.size() >= 1)
                    {
                        return context.GetService<ServiceInterfaceT>(services.at(0));
                    }
                 }
            }
            return std::shared_ptr<ServiceInterfaceT>();
        }
        template <typename ServiceInterfaceT, typename ServiceImplT>
        cppmicroservices::ServiceRegistration<ServiceInterfaceT>
        registerSvc(std::string const name, std::string const property, std::string const propertyValue)
        {
            auto mockService = std::make_shared<ServiceImplT>();
            auto serviceProps = cppmicroservices::ServiceProperties({
                {std::string(cppmicroservices::service::component::ComponentConstants::COMPONENT_NAME),name},
                {property, propertyValue}
            });
            return context.RegisterService<ServiceInterfaceT>(mockService, serviceProps);
        } 
 

    };

    class MockServiceBImpl : public test::ServiceBInt
    {
      public:
        MockServiceBImpl() = default;
        MockServiceBImpl(MockServiceBImpl const&) = delete;
        MockServiceBImpl(MockServiceBImpl&&) = delete;
        MockServiceBImpl& operator=(MockServiceBImpl const&) = delete;
        MockServiceBImpl& operator=(MockServiceBImpl&&) = delete;

        cppmicroservices::AnyMap
        GetProperties()
        {
            return {};
        }
        ~MockServiceBImpl() = default;
    };

    class MockServiceCImpl : public test::ServiceCInt
    {
      public:
        MockServiceCImpl() = default;
        MockServiceCImpl(MockServiceCImpl const&) = delete;
        MockServiceCImpl(MockServiceCImpl&&) = delete;
        MockServiceCImpl& operator=(MockServiceCImpl const&) = delete;
        MockServiceCImpl& operator=(MockServiceCImpl&&) = delete;
        cppmicroservices::AnyMap
        GetProperties()
        {
            return {};
        }
        ~MockServiceCImpl() = default;
    }; 

    /* tFactoryTarget.dependencyExistsBefore.
     * ServiceA and ServiceB are both factory instances.
     * ServiceA~1 is dependent on ServiceB~123.
     * ServiceB~123 is registered before the bundle containing ServiceA is
     * started.
     */
    TEST_F(tFactoryTarget, dependencyExistsBefore)
    {
        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");
  
        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");

        // Create the ServiceA~1 configuration object. Specify a dynamic target
        // so that the test::ServiceBInt dependency for  ServiceA~1 will only be
        // satisfied by ServiceB~123 instance.
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        auto service = getFactoryService<test::ServiceAInt>("ServiceA", "1", props);
        ASSERT_TRUE(service);

        // Clean up
        serviceBReg.Unregister();
        testBundle.Stop();
    }

    /* tFactoryTarget.dependencyExistsAfter.
     * ServiceA and ServiceB are both factory instances.
     * ServiceA~1 is dependent on ServiceB~123.
     * ServiceB~123 is registered after the bundle containing ServiceA is
     * started.
     */
    TEST_F(tFactoryTarget, dependencyExistsAfter)
    {
        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");
 
        // Create the ServiceA~1 configuration object. Specify a dynamic target
        // so that the test::ServiceBInt dependency for  ServiceA~1 will only be
        // satisfied by ServiceB~123 instance.
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        auto service = getFactoryService<test::ServiceAInt>("ServiceA", "1", props);
        // Service cannot be instantiated because it's dependencies are not yet satisfied.
        ASSERT_TRUE(!service);

        // Create a service tracker for ServiceA. 
        std::unique_ptr<cppmicroservices::ServiceTracker<test::ServiceAInt>> tracker (new cppmicroservices::ServiceTracker<test::ServiceAInt>(context, nullptr));
        tracker->Open();

        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");
   
        auto serviceA = tracker->WaitForService(TIMEOUT_HALF_SECOND);
        ASSERT_TRUE(serviceA) << "ServiceA not registered";
  
        // Clean up
        tracker->Close();
        serviceBReg.Unregister();
        testBundle.Stop();
     }

    /* tFactoryTarget.multipleTargetsExistBefore.
     * ServiceA, ServiceB and ServiceC are all factory instances.
     * ServiceA~1 is dependent on ServiceB~123 and ServiceC~123
     *
     * ServiceB~123 and ServiceC~123 are registered before the bundle containing ServiceA is
     * started.
     */
    TEST_F(tFactoryTarget, multipleTargetsExistBefore)
    {
        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");
 
        // Register the ServiceC~123 factory service with a mock implementation
        auto serviceCReg = registerSvc<test::ServiceCInt, MockServiceCImpl>("ServiceC~123", "ServiceCId", "ServiceC~123");
 
        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC2");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC2");

        // Create the ServiceA~1 configuration object. Specify a two dynamic targets
        // so that the test::ServiceBInt dependency for  ServiceA~1 will only be
        // satisfied by ServiceB~123 instance and the test::ServiceCInt will only 
        // be satisfied by ServiceC~123
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        props["ServiceC.target"] = std::string("(ServiceCId=ServiceC~123)");
        auto service = getFactoryService<test::ServiceAInt>("ServiceA", "1", props);
        ASSERT_TRUE(service);

        // Clean up
        serviceBReg.Unregister();
        serviceCReg.Unregister();
        testBundle.Stop();
    }

    /* tFactoryTarget.multipleTargetsExistsAfter.
     * ServiceA, ServiceB and ServiceC are all factory instances.
     * ServiceA~1 is dependent on ServiceB~123 and ServiceC~123
     * ServiceB~123 and ServiceC~123 are registered after the bundle containing ServiceA is
     * started.
     */
    TEST_F(tFactoryTarget,multipleTargetsExistAfter)
    {
        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC2");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC2");

        // Create the ServiceA~1 configuration object. Specify a dynamic target
        // so that the test::ServiceBInt dependency for  ServiceA~1 will only be
        // satisfied by ServiceB~123 instance.
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        props["ServiceC.target"] = std::string("(ServiceCId=ServiceC~123)");
        auto service = getFactoryService<test::ServiceAInt>("ServiceA", "1", props);
        // Service cannot be instantiated because it's dependencies are not yet satisfied.
        ASSERT_TRUE(!service);

        // Create a service tracker for ServiceA.
        std::unique_ptr<cppmicroservices::ServiceTracker<test::ServiceAInt>> tracker = std::make_unique<cppmicroservices::ServiceTracker<test::ServiceAInt>>(context, nullptr);
        tracker->Open();

        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");

        // Register the ServiceC~123 factory service with a mock implementation
        auto serviceCReg = registerSvc<test::ServiceCInt, MockServiceCImpl>("ServiceC~123", "ServiceCId", "ServiceC~123");
  
        auto serviceA = tracker->WaitForService(TIMEOUT_HALF_SECOND);
        ASSERT_TRUE(serviceA) << "ServiceA not registered";

        // Clean up
        tracker->Close();
        serviceBReg.Unregister();
        testBundle.Stop();
    }

    /* tFactoryTarget.correctTarget.
     * ServiceA and ServiceB are both factory instances.
     * ServiceA~1 is dependent on ServiceB~123.
     * Several instances of ServiceB are registered. Verify that ServiceA~1 is 
     * satisfied by ServiceB~123 and not by any other instances of ServiceB.
     */
    TEST_F(tFactoryTarget, correctTarget)
    {
        // Register the ServiceB~1 factory service with a mock implementation
        auto serviceBReg = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~1", "ServiceBId", "ServiceB~1");

        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");

        // Create the ServiceA~1 configuration object. Specify a dynamic target
        // so that the test::ServiceBInt dependency for  ServiceA~1 will only be
        // satisfied by ServiceB~123 instance.
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        auto service = getFactoryService<test::ServiceAInt>("ServiceA", "1", props);
        ASSERT_TRUE(!service);  //ServiceA~1 is not satisfied so this should be nullptr
   
       // Create a service tracker for ServiceA.
        std::unique_ptr<cppmicroservices::ServiceTracker<test::ServiceAInt>> tracker
            = std::make_unique<cppmicroservices::ServiceTracker<test::ServiceAInt>>(context, nullptr);
        tracker->Open();
        
        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg2 = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");
 
        // ServiceA~1 should now be satisfied.
        auto serviceA = tracker->WaitForService(TIMEOUT_HALF_SECOND);
        ASSERT_TRUE(serviceA) << "ServiceA not registered";

 
        // Clean up
        tracker->Close();
        serviceBReg.Unregister();
        serviceBReg2.Unregister();
        testBundle.Stop();
    }

    /* tFactoryTarget.targetByProperty
     * ServiceA and ServiceB are both factory instances.
     * ServiceA~1 is dependent on the ServiceB with a target property user-ServiceB set 
     * equal to true.
     * Several instances of ServiceB are registered. Verify that ServiceA~1 is
     * satisfied by the ServiceB with the correct property and not by any other instances of ServiceB.
     */
    TEST_F(tFactoryTarget, targetByProperty)
    {
        // Register the ServiceB~1 factory service with a mock implementation
        auto serviceBReg = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~1", "ServiceBId", "ServiceB~1");

        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");

        // Create the ServiceA~1 configuration object. Specify a dynamic target
        // so that the test::ServiceBInt dependency for  ServiceA~1 will only be
        // satisfied by ServiceB~123 instance.
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(user-ServiceB=true)");
        auto service = getFactoryService<test::ServiceAInt>("ServiceA", "1", props);
        ASSERT_TRUE(!service); // ServiceA~1 is not satisfied so this should be nullptr

        // Create a service tracker for ServiceA.
        std::unique_ptr<cppmicroservices::ServiceTracker<test::ServiceAInt>> tracker
            = std::make_unique<cppmicroservices::ServiceTracker<test::ServiceAInt>>(context, nullptr);
           tracker->Open();

        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg2 = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "user-ServiceB", "true");

        // ServiceA~1 should now be satisfied.
        auto serviceA = tracker->WaitForService(std::chrono::milliseconds(500));
        ASSERT_TRUE(serviceA) << "ServiceA not registered";

        // Clean up
        tracker->Close();
        serviceBReg.Unregister();
        serviceBReg2.Unregister();
        testBundle.Stop();
    }

    /* tFactoryTarget.multipleToOneBefore.
     * ServiceA and ServiceB are both factory instances.
     * Five instances of ServiceA will be created which are all dependent on ServiceB~123.
     * ServiceB~123 is registered before the bundle containing ServiceA is
     * started.
     */
    TEST_F(tFactoryTarget, multipleToOneBefore)
    {
        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg
            = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");

        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");

        // Create ServiceA~1, ServiceA~2, ServiceA~3, ServiceA~4, ServiceA~5 configuration objects. 
        // Specify a dynamic target so that the test::ServiceBInt dependency for  
        // ServiceA~1 will only be satisfied by ServiceB~123 instance.
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        auto range = { "1", "2", "3", "4", "5" };
        for (auto const& i : range) {
            auto service = getFactoryService<test::ServiceAInt>("ServiceA", i, props);
            ASSERT_TRUE(service);
        }

        // Clean up
        serviceBReg.Unregister();
        testBundle.Stop();
    }

    /* tFactoryTarget.multipleToOneAfter
     * ServiceA and ServiceB are both factory instances.
     * Five instances of ServiceA will be created which are all dependent on ServiceB~123.
     * ServiceB~123 is registered after the bundle containing ServiceA is
     * started.
     */
    TEST_F(tFactoryTarget, multipleToOneAfter)
    {
        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");

        // Create ServiceA~1, ServiceA~2, ServiceA~3, ServiceA~4, ServiceA~5 configuration objects.
        // Specify a dynamic target so that the test::ServiceBInt dependency for
        // ServiceA~1 will only be satisfied by ServiceB~123 instance.
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        auto range = { "1", "2", "3", "4", "5" };
        std::vector<std::shared_ptr<cppmicroservices::ServiceTracker<test::ServiceAInt>>> trackers {};
 
        for (auto const& i : range)
        {
            auto service = getFactoryService<test::ServiceAInt>("ServiceA", i, props);
            ASSERT_TRUE(!service);

            // Create a service tracker for each instance of ServiceA.
            std::string filterString = "(" + cppmicroservices::service::component::ComponentConstants::COMPONENT_NAME + "="
                                 + "ServiceA~" + std::string(i) + ")";
            cppmicroservices::LDAPFilter filter { filterString };             
            auto tracker = std::make_shared<cppmicroservices::ServiceTracker<test::ServiceAInt>>
                      (context, filter, nullptr);
            trackers.push_back (tracker);
            tracker->Open();
        }
  
        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg
            = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");

        for (auto const& tracker : trackers) {
            auto service = tracker->WaitForService(TIMEOUT_HALF_SECOND);
            ASSERT_TRUE(service) << "ServiceA not registered";
            tracker->Close();
        }
 
        // Clean up
        serviceBReg.Unregister();
        testBundle.Stop();
    }

    /* tFactoryTarget.multipleToOneUnregistered
     * ServiceA and ServiceB are both factory instances.
     * Five instances of ServiceA will be created which are all dependent on ServiceB~123.
     * ServiceB~123 is registered before the bundle containing ServiceA is
     * started. One all the instances of ServiceA are created, ServiceB 
     * is unregistered. This shouild result in all the ServiceA instances becoming
     * unsatisfied.
     */
    TEST_F(tFactoryTarget, multipleToOneUnregistered)
    {
        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg
            = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");

        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");

        auto tracker = std::make_shared<cppmicroservices::ServiceTracker<test::ServiceAInt>>(context,nullptr);
        tracker->Open();
        EXPECT_EQ(tracker->GetTrackingCount(), 0) << "No ServiceA instances yet. Tracking Count should be 0";

        // Create ServiceA~1, ServiceA~2, ServiceA~3, ServiceA~4, ServiceA~5 configuration objects.
        // Specify a dynamic target so that the test::ServiceBInt dependency for
        // ServiceA~1 will only be satisfied by ServiceB~123 instance.
        int instanceCount = 5;
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        auto range = { "1", "2", "3", "4", "5" };
        for (auto const& i : range)
        {
            auto service = getFactoryService<test::ServiceAInt>("ServiceA", i, props);
            ASSERT_TRUE(service);
        }
        auto sRefs = tracker->GetServiceReferences();           
        EXPECT_EQ(sRefs.size(), instanceCount) << "All instances of ServiceA should be created.";

        serviceBReg.Unregister();  
        auto fut = std::async(std::launch::async,
                              [&]()
                              {
                                  while (!tracker->IsEmpty()) {};
                                  return;
                              });
        fut.wait_for(TIMEOUT_HALF_SECOND);
 
        ASSERT_TRUE(tracker->IsEmpty()) << "All ServiceA instances should have been destroyed.";       
        testBundle.Stop();
    }

    /* tFactoryTarget.multipleToOneConcurrent
     * ServiceA and ServiceB are both factory instances.
     * Many instances of ServiceA will be created which are all dependent on ServiceB~123.
     * ServiceB~123 is registered before the bundle containing ServiceA is
     * started.
     */
    TEST_F(tFactoryTarget, multipleToOneConcurrent)
    {
        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg
            = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");

        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");

        // Create configuration objects for ServicA instances configuration objects
        // concurrently.
        // Specify a dynamic target so that the test::ServiceBInt dependency for
        // ServiceA~1 will only be satisfied by ServiceB~123 instance.
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        
        std::function<bool()> createFunc = [&]() -> bool
        {
            auto service = getFactoryService<test::ServiceAInt>("ServiceA", "", props);
            return service ? true : false;
        };
        auto results = ConcurrentInvoke(createFunc);
        EXPECT_TRUE(!results.empty());
        EXPECT_TRUE(std::all_of(results.cbegin(), results.cend(), [](bool result) { return result; }));

        // Clean up
        serviceBReg.Unregister();
        testBundle.Stop();
    }

    // Test ComponentFactoryImpl constructor with invalid arguments
    TEST_F(tFactoryTarget, ctorInvalidArgs)
    {
        std::shared_ptr<cppmicroservices::logservice::LogService> logger;
        std::shared_ptr <cppmicroservices::scrimpl::SCRAsyncWorkService> asyncWorkService;
        std::shared_ptr<cppmicroservices::scrimpl::SCRExtensionRegistry> bundleRegistry;
        EXPECT_THROW({ cppmicroservices::scrimpl::ComponentFactoryImpl componentFactory(context,
                                                                                logger,
                                                                                asyncWorkService,
                                                                                bundleRegistry);
        }, std::invalid_argument);
    }

    /* This test verifies that if an invalid LDAP filter is received in the properties for a configuration 
     * object then DS intercepts the exception and logs it. Also, if a std::exception occurs while creating
     * the factory instance DS intercepts the exception and logs it. 
     */
    TEST_F(tFactoryTarget, testExceptionLogging)
    {
        // Create a mock ComponentFactoryImpl object. We will call the CreateFactoryComponent method for this object.
        auto mockLogger = std::make_shared<cppmicroservices::scrimpl::MockLogger>();
        auto asyncWorkService = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(context, mockLogger);
        auto bundleRegistry = std::make_shared<cppmicroservices::scrimpl::SCRExtensionRegistry>(mockLogger);
        cppmicroservices::scrimpl::ComponentFactoryImpl componentFactory(context,
                                                                         mockLogger,
                                                                         asyncWorkService,
                                                                         bundleRegistry);

        // Create some component meta data and insert a reference into the refsMetadata vector
        auto mockMetadata = std::make_shared<cppmicroservices::scrimpl::metadata::ComponentMetadata>();      
        cppmicroservices::scrimpl::metadata::ReferenceMetadata reference;
        reference.name = "ServiceB";
        reference.interfaceName = "ServiceBInt";
        mockMetadata->refsMetadata.push_back(reference);
        auto notifier = std::make_shared<cppmicroservices::scrimpl::ConfigurationNotifier>(context,
                                                                mockLogger,
                                                                asyncWorkService,
                                                                bundleRegistry);
    
        //Create a mock ComponentConfigurationImpl object with the metadata containing the 
        //reference for ServiceBInt.
        auto fakeRegistry = std::make_shared<cppmicroservices::scrimpl::ComponentRegistry>();
        auto fakeCompConfig = std::make_shared<cppmicroservices::scrimpl::SingletonComponentConfigurationImpl>(mockMetadata,
                                                                              framework,
                                                                              fakeRegistry,
                                                                              mockLogger,
                                                                              notifier);
        
         std::shared_ptr<cppmicroservices::scrimpl::ComponentConfigurationImpl> mgr = fakeCompConfig;
        // set logging expectations
        auto exceptionLDAPFilter
            = testing::AllOf(testing::HasSubstr("CreateFactoryComponent failed because of invalid target ldap filter"));
        EXPECT_CALL(*mockLogger, Log(cppmicroservices::logservice::SeverityLevel::LOG_ERROR, exceptionLDAPFilter)).Times(1);

        //Create properties with a bad LDAPFilter (It's missing a close parenthesis after 123).
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123");      

        // When the ComponentFactoryImpl CreateFactoryComponent method is called it will log an error 
        // exceptionLDAPFilter and throw an invalid_argument exception
         EXPECT_THROW({ componentFactory.CreateFactoryComponent("serviceA~123", mgr, props); },
                     std::invalid_argument);

    }

    /* testInvalidDynamicTargetLogging.
     * This test verifies that if a dynamic target is specified in the properties of a singletone component,
     * DS logs the error. Dyanmic targets are only allowed for factory instance construction.
     */
    TEST_F(tFactoryTarget, testInvalidDynamicTargetLogging)
    {
        // Create a ConfigurationNotifier with a mock Logger.
         auto mockLogger = std::make_shared<cppmicroservices::scrimpl::MockLogger>();
         auto asyncWorkService = std::make_shared<cppmicroservices::scrimpl::SCRAsyncWorkService>(context, mockLogger);
         auto bundleRegistry = std::make_shared<cppmicroservices::scrimpl::SCRExtensionRegistry>(mockLogger);
         auto notifier = std::make_shared<cppmicroservices::scrimpl::ConfigurationNotifier>(context,
                                                                                            mockLogger,
                                                                                            asyncWorkService,
                                                                                            bundleRegistry);

         // Create some component meta data and insert a reference into the refsMetadata vector
         auto mockMetadata = std::make_shared<cppmicroservices::scrimpl::metadata::ComponentMetadata>();
         cppmicroservices::scrimpl::metadata::ReferenceMetadata reference;
         reference.name = "ServiceB";
         reference.interfaceName = "ServiceBInt";
         mockMetadata->refsMetadata.push_back(reference);

         // Create a mock ComponentConfigurationImpl object with the metadata containing the
         // reference for ServiceBInt.
         auto fakeRegistry = std::make_shared<cppmicroservices::scrimpl::ComponentRegistry>();
         auto fakeCompConfig
             = std::make_shared<cppmicroservices::scrimpl::SingletonComponentConfigurationImpl>(mockMetadata,
                                                                                                framework,
                                                                                                fakeRegistry,
                                                                                                mockLogger,
                                                                                                notifier);
         std::shared_ptr<cppmicroservices::scrimpl::ComponentConfigurationImpl> mgr = fakeCompConfig;

        // set logging expectations
        auto invalidTarget = testing::AllOf(
             testing::HasSubstr("Properties for component"), 
             testing::HasSubstr("contains a dynamic target for interface"));
        EXPECT_CALL(*mockLogger, Log(cppmicroservices::logservice::SeverityLevel::LOG_ERROR, invalidTarget))
             .Times(1);

        // Create properties with a dynamic target for interface ServiceBInt
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");    
        notifier->LogInvalidDynamicTargetInProperties(props, mgr);
    }    

    /* tFactoryTarget.dynamicTargetForSingleton.
     * ServiceA and ServiceB are both factory instances.
     * ServiceA~1 is dependent on ServiceB~123.
     * Construct ServiceA~1. 
     * Verify it exists and it's properties can be read
     * Update ServiceA~1 properties including a dynamic target.
     * Verify that dynamic target had no effect (GetService still succeeds) 
     * Verify that the properties have been updated.
     */
    TEST_F(tFactoryTarget, dynamicTargetForSingleton)
    {
        // Register the ServiceB~123 factory service with a mock implementation
        auto serviceBReg
            = registerSvc<test::ServiceBInt, MockServiceBImpl>("ServiceB~123", "ServiceBId", "ServiceB~123");

        // Install and start the bundle containing the ServiceA factory.
        // DS is now listening for factory configuration objects of the form
        // ServiceA~<instance>
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");

        // Create the ServiceA~1 configuration object. Specify a dynamic target
        // so that the test::ServiceBInt dependency for  ServiceA~1 will only be
        // satisfied by ServiceB~123 instance.
        cppmicroservices::AnyMap props;
        props["ServiceB.target"] = std::string("(ServiceBId=ServiceB~123)");
        auto service = getFactoryService<test::ServiceAInt>("ServiceA", "1", props);
        ASSERT_TRUE(service);

        std::string const fooValue { "123" };
        props["foo"] = fooValue;
        auto config = configAdmin->GetFactoryConfiguration("ServiceA", "1");
        auto fut = config->Update(props);
        fut.get();

        auto instance = GetInstance<test::ServiceAInt>();
        ASSERT_TRUE(instance) << "ServiceA~1 instance not found";
        auto properties = instance->GetProperties();
        ASSERT_TRUE(properties.size() > 1) << "ServiceA~1 properties should have two entries.";

        auto iter = properties.find("foo");
        ASSERT_TRUE(iter != properties.end()) << "The foo key does not exist in the properties.";
        EXPECT_EQ(iter->second, fooValue);

        // Clean up
        serviceBReg.Unregister();
        testBundle.Stop();
    }

    TEST_F(tFactoryTarget, testRefToSingletonService)
    {
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");
        auto config = configAdmin->GetConfiguration("scopetestpid");
        cppmicroservices::AnyMap props;
        props["foo"] = 1;
        auto fut = config->Update(props);
        fut.get();

        auto service1 = getFactoryService<test::ServiceAInt>("ServiceA2", "1", {});
        auto service2 = getFactoryService<test::ServiceAInt>("ServiceA2", "2", {});
        ASSERT_TRUE(service1);
        ASSERT_TRUE(service2);
        ASSERT_EQ(service1->GetRefAddr(), service2->GetRefAddr());
    }

    TEST_F(tFactoryTarget, testRefToProtypeService)
    {
        test::InstallLib(context, "TestBundleDSFAC1");
        cppmicroservices::Bundle testBundle = StartTestBundle("TestBundleDSFAC1");
        auto config = configAdmin->GetConfiguration("scopetestpid");
        cppmicroservices::AnyMap props;
        props["foo"] = 1;
        auto fut = config->Update(props);
        fut.get();

        auto service1 = getFactoryService<test::ServiceAInt>("ServiceA3", "1", {});
        auto service2 = getFactoryService<test::ServiceAInt>("ServiceA3", "2", {});
        ASSERT_TRUE(service1);
        ASSERT_TRUE(service2);
        ASSERT_NE(service1->GetRefAddr(), service2->GetRefAddr());
    }
}; // namespace test
