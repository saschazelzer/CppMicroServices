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

#include "cppmicroservices/ServiceFactory.h"
#include "cppmicroservices/Bundle.h"
#include "cppmicroservices/BundleContext.h"
#include "cppmicroservices/Constants.h"
#include "cppmicroservices/Framework.h"
#include "cppmicroservices/FrameworkEvent.h"
#include "cppmicroservices/FrameworkFactory.h"
#include "cppmicroservices/GetBundleContext.h"
#include "cppmicroservices/LDAPProp.h"
#include "cppmicroservices/ListenerToken.h"
#include "cppmicroservices/ServiceObjects.h"

#include "cppmicroservices/detail/Threads.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "TestUtils.h"
#include "TestingConfig.h"

#include <future>
#include <thread>

using namespace cppmicroservices;
using namespace cppmicroservices::testing;

namespace cppmicroservices
{

    struct TestBundleH
    {
        virtual ~TestBundleH() {}
    };

    struct TestBundleH2
    {
        virtual ~TestBundleH2() {}
    };
} // namespace cppmicroservices
namespace
{
    // Service interfaces
    struct ITestServiceA
    {
        virtual ~ITestServiceA() {}
    };

    struct ITestServiceB
    {
        virtual ~ITestServiceB() {}
    };

    // Service implementations
    struct TestServiceAImpl : public ITestServiceA
    {
    };

    // Mocks
    class MockFactory : public ServiceFactory
    {
      public:
        MOCK_METHOD2(GetService, InterfaceMapConstPtr(Bundle const&, ServiceRegistrationBase const&));
        MOCK_METHOD3(UngetService, void(Bundle const&, ServiceRegistrationBase const&, InterfaceMapConstPtr const&));
    };

} // namespace

class ServiceFactoryTest : public ::testing::Test
{
  public:
    ServiceFactoryTest() : framework(FrameworkFactory().NewFramework()) {};
    ~ServiceFactoryTest() override = default;
    void
    SetUp() override
    {
        framework.Start();
        context = framework.GetBundleContext();
    }

    void
    TearDown() override
    {
        framework.Stop();
        framework.WaitForStop(std::chrono::milliseconds::zero());
    }

  protected:
    Framework framework;
    BundleContext context;
};

// Tests the return type and FrameworkEvent generated when a
// ServiceFactory instance returns a map that does not contain
// all the interfaces specified in the call to RegisterService
TEST_F(ServiceFactoryTest, TestGetServiceReturnsIncompleteMap)
{
    auto sf = std::make_shared<MockFactory>();
    EXPECT_CALL(*sf, GetService(::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Invoke(
            [](Bundle const& /*bundle*/, ServiceRegistrationBase const& /*reg*/)
            {
                std::shared_ptr<ITestServiceA> implPtr = std::make_shared<TestServiceAImpl>();
                return MakeInterfaceMap<ITestServiceA>(implPtr);
            }));
    EXPECT_CALL(*sf, UngetService(::testing::_, ::testing::_, ::testing::_)).Times(0);

    ServiceRegistration<ITestServiceA, ITestServiceB> reg1 = context.RegisterService<ITestServiceA, ITestServiceB>(
        ToFactory(sf),
        ServiceProperties({
            {Constants::SERVICE_SCOPE, Any(Constants::SCOPE_PROTOTYPE)}
    }));

    auto sref1 = context.GetServiceReference<ITestServiceA>();
    ASSERT_TRUE(static_cast<bool>(sref1));
    FrameworkEvent lastEvent;
    auto lToken = context.AddFrameworkListener([](cppmicroservices::FrameworkEvent const& evt)
                                               { ASSERT_EQ(evt.GetType(), FrameworkEvent::FRAMEWORK_WARNING); });
    ASSERT_EQ(context.GetService<ITestServiceA>(sref1), nullptr);
    auto sref2 = context.GetServiceReference<ITestServiceB>();
    ASSERT_TRUE(static_cast<bool>(sref2));
    ASSERT_EQ(context.GetService<ITestServiceB>(sref2), nullptr);
    context.RemoveListener(std::move(lToken));
}

// Tests the return type and FrameworkEvent generated when a
// ServiceFactory instance throws an exception in it's GetService
// callback
TEST_F(ServiceFactoryTest, TestGetServiceThrows)
{
    auto sf = std::make_shared<MockFactory>();
    std::string exceptionMsg("ServiceFactory threw an unknown exception.");
    EXPECT_CALL(*sf, GetService(::testing::_, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Throw(std::runtime_error(exceptionMsg)));
    EXPECT_CALL(*sf, UngetService(::testing::_, ::testing::_, ::testing::_)).Times(0);

    ServiceRegistration<ITestServiceA> reg1
        = context.RegisterService<ITestServiceA>(ToFactory(sf),
                                                 ServiceProperties({
                                                     {Constants::SERVICE_SCOPE, Any(Constants::SCOPE_PROTOTYPE)}
    }));

    auto sref = context.GetServiceReference<ITestServiceA>();
    ASSERT_TRUE(static_cast<bool>(sref));
    auto lToken = context.AddFrameworkListener(
        [&exceptionMsg](cppmicroservices::FrameworkEvent const& evt)
        {
            ASSERT_EQ(evt.GetType(), FrameworkEvent::FRAMEWORK_ERROR);
            ASSERT_NE(evt.GetThrowable(), nullptr);
            EXPECT_NO_THROW(
                try { std::rethrow_exception(evt.GetThrowable()); } catch (const std::runtime_error& err) {
                    ASSERT_STREQ(err.what(), exceptionMsg.c_str());
                });
        });
    ASSERT_EQ(context.GetService<ITestServiceA>(sref), nullptr);
    context.RemoveListener(std::move(lToken));
}

// Tests the return type and FrameworkEvent generated when a
// ServiceFactory instance throws an exception in it's GetService
// callback
TEST_F(ServiceFactoryTest, TestGetServiceObjectThrows)
{
    auto sf = std::make_shared<MockFactory>();
    std::string exceptionMsg("ServiceFactory threw an unknown exception.");
    EXPECT_CALL(*sf, GetService(::testing::_, ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Throw(std::runtime_error(exceptionMsg)));
    EXPECT_CALL(*sf, UngetService(::testing::_, ::testing::_, ::testing::_)).Times(0);

    (void)context.RegisterService<ITestServiceA>(ToFactory(sf),
                                                 ServiceProperties({
                                                     {Constants::SERVICE_SCOPE, Any(Constants::SCOPE_PROTOTYPE)}
    }));

    auto sref = context.GetServiceReference<ITestServiceA>();
    auto serviceObjects = context.GetServiceObjects<ITestServiceA>(sref);

    // Next line used to crash if a service implementation threw an exception in the
    // constructor. This test case checks to make sure that the fix is in place.
    EXPECT_NO_THROW((void)serviceObjects.GetService());
}

// Tests the return type and FrameworkEvent generated when a
// ServiceFactory instance returns nullptr
TEST_F(ServiceFactoryTest, TestGetServiceReturnsNull)
{
    auto sf = std::make_shared<MockFactory>();
    EXPECT_CALL(*sf, GetService(::testing::_, ::testing::_)).Times(1).WillRepeatedly(::testing::Return(nullptr));
    EXPECT_CALL(*sf, UngetService(::testing::_, ::testing::_, ::testing::_)).Times(0);

    ServiceRegistration<ITestServiceA> reg1
        = context.RegisterService<ITestServiceA>(ToFactory(sf),
                                                 ServiceProperties({
                                                     {Constants::SERVICE_SCOPE, Any(Constants::SCOPE_PROTOTYPE)}
    }));

    auto sref = context.GetServiceReference<ITestServiceA>();
    ASSERT_TRUE(static_cast<bool>(sref));
    auto lToken = context.AddFrameworkListener([](cppmicroservices::FrameworkEvent const& evt)
                                               { ASSERT_EQ(evt.GetType(), FrameworkEvent::FRAMEWORK_ERROR); });
    ASSERT_EQ(context.GetService<ITestServiceA>(sref), nullptr);
    context.RemoveListener(std::move(lToken));
}

TEST_F(ServiceFactoryTest, TestServiceFactoryPrototypeScope)
{
    // Install and start test bundle H, a service factory and test that the methods
    // in that interface works.
    auto bundle = InstallLib(context, "TestBundleH");
    // Test for existing bundle TestBundleH
    ASSERT_TRUE(bundle);

    auto bundleH = GetBundle("TestBundleH", context);
    // Test for existing bundle TestBundleH
    ASSERT_TRUE(bundleH);

    bundleH.Start();

    // Check that a service reference exist
    ServiceReference<TestBundleH2> const sr1 = context.GetServiceReference<TestBundleH2>();
    ASSERT_TRUE(sr1);
    ASSERT_EQ(sr1.GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_PROTOTYPE);

    ServiceObjects<TestBundleH2> svcObjects = context.GetServiceObjects(sr1);
    auto prototypeServiceH2 = svcObjects.GetService();

    const ServiceReferenceU sr1void = context.GetServiceReference(us_service_interface_iid<TestBundleH2>());
    ServiceObjects<void> svcObjectsVoid = context.GetServiceObjects(sr1void);
    InterfaceMapConstPtr prototypeServiceH2Void = svcObjectsVoid.GetService();
    ASSERT_NE(prototypeServiceH2Void->find(us_service_interface_iid<TestBundleH2>()), prototypeServiceH2Void->end());

#ifdef US_BUILD_SHARED_LIBS
    // There should be only one service in use
    ASSERT_EQ(context.GetBundle().GetServicesInUse().size(), 1);
#endif

    auto bundleScopeService = context.GetService(sr1);
    ASSERT_TRUE(bundleScopeService);
    ASSERT_NE(bundleScopeService, prototypeServiceH2);

    ASSERT_NE(prototypeServiceH2, prototypeServiceH2Void->find(us_service_interface_iid<TestBundleH2>())->second);

    auto bundleScopeService2 = context.GetService(sr1);
    ASSERT_EQ(bundleScopeService, bundleScopeService2);

#ifdef US_BUILD_SHARED_LIBS
    std::vector<ServiceReferenceU> usedRefs = context.GetBundle().GetServicesInUse();
    ASSERT_EQ(usedRefs.size(), 1);
    ASSERT_EQ(usedRefs[0], sr1);
#endif

    std::string filter = "(" + Constants::SERVICE_ID + "=" + sr1.GetProperty(Constants::SERVICE_ID).ToString() + ")";
    ServiceReference<TestBundleH> const sr2 = context.GetServiceReferences<TestBundleH>(filter).front();
    // Service shall be present
    ASSERT_TRUE(sr2);
    ASSERT_EQ(sr2.GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_PROTOTYPE);
    ASSERT_EQ(any_cast<long>(sr2.GetProperty(Constants::SERVICE_ID)),
              any_cast<long>(sr1.GetProperty(Constants::SERVICE_ID)));

#ifdef US_BUILD_SHARED_LIBS
    // There should still be only one service in use
    usedRefs = context.GetBundle().GetServicesInUse();
    // services in use
    ASSERT_EQ(usedRefs.size(), 1);
#endif

    ServiceObjects<TestBundleH2> svcObjects2 = std::move(svcObjects);
    ServiceObjects<TestBundleH2> svcObjects3 = context.GetServiceObjects(sr1);

    prototypeServiceH2 = svcObjects2.GetService();
    auto prototypeServiceH2_2 = svcObjects3.GetService();

    ASSERT_TRUE(prototypeServiceH2_2);
    ASSERT_NE(prototypeServiceH2_2, prototypeServiceH2);

    bundleH.Stop();
}

TEST_F(ServiceFactoryTest, TestServiceFactoryBundleScope)
{

    // Install and start test bundle H, a service factory and test that the methods
    // in that interface works.

    auto bundle = InstallLib(context, "TestBundleH");
    // Test for existing bundle TestBundleH
    ASSERT_TRUE(bundle);

    auto bundleH = GetBundle("TestBundleH", context);
    // Test for existing bundle TestBundleH
    ASSERT_TRUE(bundleH);

    bundleH.Start();

    std::vector<ServiceReferenceU> registeredRefs = bundleH.GetRegisteredServices();
    // Test that the # of registered services is seven.
    ASSERT_EQ(registeredRefs.size(), 7);
    // Test that First service is bundle scope
    ASSERT_EQ(registeredRefs[0].GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_BUNDLE);
    // Test that Second service is bundle scope
    ASSERT_EQ(registeredRefs[1].GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_BUNDLE);
    // Test that Third service is bundle scope
    ASSERT_EQ(registeredRefs[2].GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_BUNDLE);
    // Test that Fourth service is bundle scope
    ASSERT_EQ(registeredRefs[3].GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_BUNDLE);
    // Test that Fifth service is bundle scope
    ASSERT_EQ(registeredRefs[4].GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_BUNDLE);
    // Test that Sixth service is prototype scope
    ASSERT_EQ(registeredRefs[5].GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_BUNDLE);
    // Test that Seventh service is prototype scope
    ASSERT_EQ(registeredRefs[6].GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_PROTOTYPE);

    // Check that a service reference exists
    const ServiceReferenceU sr1 = context.GetServiceReference("cppmicroservices::TestBundleH");
    ASSERT_TRUE(sr1);
    // service is bundle scope
    ASSERT_EQ(sr1.GetProperty(Constants::SERVICE_SCOPE).ToString(), Constants::SCOPE_BUNDLE);

    InterfaceMapConstPtr service = context.GetService(sr1);
    ASSERT_TRUE(service);
    // Must Return at least 1 service object
    EXPECT_GE(static_cast<int>(service->size()), 1);
    InterfaceMap::const_iterator serviceIter = service->find("cppmicroservices::TestBundleH");
    // Find a service object implementing the 'cppmicroservices::TestBundleH' interface
    ASSERT_NE(serviceIter, service->end());
    // Test that The service object is valid (i.e. not nullptr)
    ASSERT_NE(serviceIter->second, nullptr);

    InterfaceMapConstPtr service2 = context.GetService(sr1);
    // Test that Two service interface maps are equal
    ASSERT_EQ(*(service.get()), *(service2.get()));

    std::vector<ServiceReferenceU> usedRefs = context.GetBundle().GetServicesInUse();
    ASSERT_EQ(usedRefs.size(), 1);
    ASSERT_EQ(usedRefs[0], sr1);

    InterfaceMapConstPtr service3 = bundleH.GetBundleContext().GetService(sr1);
    ASSERT_NE(service.get(), service3.get());

    // release any service objects before stopping the bundle
    service.reset();
    service2.reset();
    service3.reset();

    bundleH.Stop();
}

#ifdef US_ENABLE_THREADING_SUPPORT

// test that concurrent calls to ServiceFactory::GetService and ServiceFactory::UngetService
// don't cause race conditions.
TEST_F(ServiceFactoryTest, TestConcurrentServiceFactory)
{
    auto framework2 = framework;

    auto bundle = cppmicroservices::testing::InstallLib(framework2.GetBundleContext(), "TestBundleH");
    bundle.Start();

    std::vector<std::thread> worker_threads;
    for (size_t i = 0; i < 100; ++i)
    {
        worker_threads.push_back(std::thread(
            [framework2]()
            {
                auto frameworkCtx = framework2.GetBundleContext();
                ASSERT_TRUE(frameworkCtx) << "Failed to get Framework's bundle context. "
                                             "Terminating the thread...";

                for (int i = 0; i < 100; ++i)
                {
                    auto ref = frameworkCtx.GetServiceReference<cppmicroservices::TestBundleH2>();
                    if (ref)
                    {
                        std::shared_ptr<cppmicroservices::TestBundleH2> svc = frameworkCtx.GetService(ref);
                        ASSERT_TRUE(svc) << "Failed to retrieve a valid service object";
                    }
                }
            }));
    }

    for (auto& t : worker_threads)
        t.join();
}
#endif

TEST_F(ServiceFactoryTest, TestServiceFactoryBundleScopeErrorConditions)
{
    bool mainStarted = false;

    // Start the embedded test driver bundle. It is needed by
    // the recursive service factory tests.
    for (auto b : context.GetBundles())
    {
        if (b.GetSymbolicName() == "main")
        {
            b.Start();
            mainStarted = true;
            break;
        }
    }

    ASSERT_TRUE(mainStarted);

    auto bundle = InstallLib(context, "TestBundleH");
    // Test for existing bundle TestBundleH
    ASSERT_TRUE(bundle);

    auto bundleH = GetBundle("TestBundleH", context);
    // Test for existing bundle TestBundleH
    ASSERT_TRUE(bundleH);

    bundleH.Start();

    struct
        : detail::MultiThreaded<>
        , std::vector<FrameworkEvent>
    {
    } fwEvents;
    auto eventCountListener = [&fwEvents](FrameworkEvent const& event) { fwEvents.Lock(), fwEvents.push_back(event); };
    auto listenerToken = bundleH.GetBundleContext().AddFrameworkListener(eventCountListener);

    // Test that a service factory which returns a nullptr returns an invalid (nullptr) shared_ptr
    std::string returnsNullPtrFilter(LDAPProp("returns_nullptr") == true);
    auto serviceRefs(context.GetServiceReferences("cppmicroservices::TestBundleH", returnsNullPtrFilter));
    // Number of service references returned is 1.
    ASSERT_EQ(serviceRefs.size(), 1);
    // Test that 'returns_nullptr' service property is 'true'
    ASSERT_EQ("1", serviceRefs[0].GetProperty(std::string("returns_nullptr")).ToString());
    // Test that the service object returned is a nullptr
    ASSERT_EQ(nullptr, context.GetService(serviceRefs[0]));
    // Test that one FrameworkEvent was sent
    ASSERT_EQ(1, fwEvents.size());

    bundleH.GetBundleContext().RemoveListener(std::move(listenerToken));

    fwEvents.clear();
    listenerToken = bundleH.GetBundleContext().AddFrameworkListener(eventCountListener);

    // Test getting a service object using an interface which isn't implemented by the service factory
    std::string returnsWrongInterfaceFilter(LDAPProp("returns_wrong_interface") == true);
    auto svcNoInterfaceRefs(context.GetServiceReferences("cppmicroservices::TestBundleH", returnsWrongInterfaceFilter));
    // Test that Number of service references returned is 1
    ASSERT_EQ(svcNoInterfaceRefs.size(), 1);
    // Test that 'returns_wrong_interface' service property is 'true'
    ASSERT_EQ("1", svcNoInterfaceRefs[0].GetProperty(std::string("returns_wrong_interface")).ToString());
    // Test that the service object returned is a nullptr
    ASSERT_EQ(nullptr, context.GetService(svcNoInterfaceRefs[0]));
    // Test that one FrameworkEvent was sent
    ASSERT_EQ(1, fwEvents.size());

    bundleH.GetBundleContext().RemoveListener(std::move(listenerToken));

    fwEvents.clear();
    listenerToken = bundleH.GetBundleContext().AddFrameworkListener(eventCountListener);

    // Test getting a service object from a service factory which throws an exception
    std::string getServiceThrowsFilter(LDAPProp("getservice_exception") == true);
    auto svcGetServiceThrowsRefs(context.GetServiceReferences("cppmicroservices::TestBundleH", getServiceThrowsFilter));
    // Test that Number of service references returned is 1.
    ASSERT_EQ(svcGetServiceThrowsRefs.size(), 1);
    // Test that 'getservice_exception' service property is 'true'
    ASSERT_EQ("1", svcGetServiceThrowsRefs[0].GetProperty(std::string("getservice_exception")).ToString());
    // Test that the service object returned is a nullptr
    ASSERT_EQ(nullptr, context.GetService(svcGetServiceThrowsRefs[0]));
    // Test that one FrameworkEvent was sent
    ASSERT_EQ(1, fwEvents.size());

    bundleH.GetBundleContext().RemoveListener(std::move(listenerToken));

    fwEvents.clear();
    listenerToken = bundleH.GetBundleContext().AddFrameworkListener(eventCountListener);

    std::string unGetServiceThrowsFilter(LDAPProp("ungetservice_exception") == true);
    auto svcUngetServiceThrowsRefs(
        context.GetServiceReferences("cppmicroservices::TestBundleH", unGetServiceThrowsFilter));
    // test that Number of service references returned is 1.
    ASSERT_EQ(svcUngetServiceThrowsRefs.size(), 1);
    // Test that 'ungetservice_exception' service property is 'true'
    ASSERT_EQ("1", svcUngetServiceThrowsRefs[0].GetProperty(std::string("ungetservice_exception")).ToString());
    {
        auto sfUngetServiceThrowSvc = context.GetService(svcUngetServiceThrowsRefs[0]);
    } // When sfUngetServiceThrowSvc goes out of scope, ServiceFactory::UngetService should be called
    // Test that one FrameworkEvent was sent
    ASSERT_EQ(1, fwEvents.size());

    fwEvents.clear();
    std::string recursiveGetServiceFilter(LDAPProp("getservice_recursion") == true);
    auto svcRecursiveGetServiceRefs(
        context.GetServiceReferences("cppmicroservices::TestBundleH", recursiveGetServiceFilter));
    // Test that Number of service references returned is 1.
    ASSERT_EQ(svcRecursiveGetServiceRefs.size(), 1);
    // Test that 'getservice_recursion' service property is 'true'
    ASSERT_EQ("1", svcRecursiveGetServiceRefs[0].GetProperty(std::string("getservice_recursion")).ToString());
#if !defined(US_ENABLE_THREADING_SUPPORT) || defined(US_HAVE_THREAD_LOCAL)
    // Test that the service object returned is a nullptr
    ASSERT_EQ(nullptr, context.GetService(svcRecursiveGetServiceRefs[0]));
    // Test that one FrameworkEvent was sent
    ASSERT_EQ(2, fwEvents.size());
    // Test for correct framwork event type
    ASSERT_EQ(fwEvents[0].GetType(), FrameworkEvent::FRAMEWORK_ERROR);
    // Test for correct framwork event bundle
    ASSERT_EQ(fwEvents[0].GetBundle(), context.GetBundle());

    // Test for correct service exception type (recursion)
    EXPECT_NO_THROW(
        try { std::rethrow_exception(fwEvents[0].GetThrowable()); } catch (ServiceException const& exc) {
            ASSERT_EQ(exc.GetType(), ServiceException::FACTORY_RECURSION);
        });

    EXPECT_THROW(std::rethrow_exception(fwEvents[0].GetThrowable()), cppmicroservices::ServiceException);
#endif

#ifdef US_ENABLE_THREADING_SUPPORT
#    ifdef US_HAVE_THREAD_LOCAL
    {
        std::vector<std::future<void>> futures;
        for (std::size_t i = 0; i < 10; ++i)
        {
            futures.push_back(std::async(std::launch::async,
                                         [&]
                                         {
                                             for (std::size_t j = 0; j < 100; ++j)
                                             {
                                                 // Get and automatically unget the service
                                                 auto svc = context.GetService(svcRecursiveGetServiceRefs[0]);
                                             }
                                         }));
        }

        for (auto& fut : futures)
        {
            fut.wait();
        }
    }
#    endif
#else
    {
        // Get and automatically unget the service
        auto svc = context.GetService(svcRecursiveGetServiceRefs[0]);
    }
#endif

    // Test recursive service factory GetService calls, but with different
    // calling bundles. This should work.
    fwEvents.clear();

    // Test that the service object returned is not a nullptr
    ASSERT_NE(nullptr, GetBundleContext().GetService(svcRecursiveGetServiceRefs[0]));
    // Test that no FrameworkEvent was sent
    ASSERT_TRUE(fwEvents.empty());
}
