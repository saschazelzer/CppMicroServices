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

#include "cppmicroservices/ServiceTracker.h"
#include "cppmicroservices/Bundle.h"
#include "cppmicroservices/BundleContext.h"
#include "cppmicroservices/Framework.h"
#include "cppmicroservices/FrameworkEvent.h"
#include "cppmicroservices/FrameworkFactory.h"
#include "cppmicroservices/GetBundleContext.h"
#include "cppmicroservices/ServiceInterface.h"
#include "cppmicroservices/ServiceReference.h"

#include "ServiceControlInterface.h"
#include "TestUtils.h"
#include <TestingConfig.h>

#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <unordered_map>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace cppmicroservices;

namespace
{

    bool
    CheckConvertibility(std::vector<ServiceReferenceU> const& refs,
                        std::vector<std::string>::const_iterator idBegin,
                        std::vector<std::string>::const_iterator idEnd)
    {
        std::vector<std::string> ids { idBegin, idEnd };

        return std::all_of(refs.cbegin(),
                           refs.cend(),
                           [interfaceIds = std::move(ids)](ServiceReferenceU const& ref)
                           {
                               return std::any_of(interfaceIds.cbegin(),
                                                  interfaceIds.cend(),
                                                  [&ref](const std::string& interfaceId)
                                                  { return ref.IsConvertibleTo(interfaceId); });
                           });
    }

    struct MyInterfaceOne
    {
        virtual ~MyInterfaceOne() {}
    };

    struct MyInterfaceTwo
    {
        virtual ~MyInterfaceTwo() {}
    };

    class MyCustomizer : public cppmicroservices::ServiceTrackerCustomizer<MyInterfaceOne>
    {

      public:
        MyCustomizer(BundleContext const& context) : m_context(context) {}

        virtual std::shared_ptr<MyInterfaceOne>
        AddingService(ServiceReference<MyInterfaceOne> const& reference)
        {
            EXPECT_TRUE(reference) << "AddingService() valid reference";
            return m_context.GetService(reference);
        }

        virtual void
        ModifiedService(ServiceReference<MyInterfaceOne> const& reference,
                        std::shared_ptr<MyInterfaceOne> const& service)
        {
            EXPECT_TRUE(reference) << "ModifiedService() valid reference";
            EXPECT_TRUE(service) << "ModifiedService() valid service";
        }

        virtual void
        RemovedService(ServiceReference<MyInterfaceOne> const& reference,
                       std::shared_ptr<MyInterfaceOne> const& service)
        {
            EXPECT_TRUE(reference) << "RemovedService() valid reference";
            EXPECT_TRUE(service) << "RemovedService() valid service";
        }

      private:
        BundleContext m_context;
    };

    template <typename Interface>
    class MockCustomizedServiceTracker : public cppmicroservices::ServiceTrackerCustomizer<Interface>
    {
      public:
        MOCK_METHOD(std::shared_ptr<Interface>,
                    AddingService,
                    (ServiceReference<Interface> const& reference),
                    (override));
        MOCK_METHOD(void,
                    ModifiedService,
                    (ServiceReference<Interface> const& reference, std::shared_ptr<Interface> const& service),
                    (override));
        MOCK_METHOD(void,
                    RemovedService,
                    (ServiceReference<Interface> const& reference, std::shared_ptr<Interface> const& service),
                    (override));
    };

    class ServiceTrackerTestFixture : public ::testing::Test
    {
      public:
        ServiceTrackerTestFixture() : framework(FrameworkFactory().NewFramework()) {};
        ~ServiceTrackerTestFixture() override = default;

        void
        SetUp() override
        {
            framework.Start();
        }
        void
        TearDown() override
        {
            framework.Stop();
            framework.WaitForStop(std::chrono::milliseconds::zero());
        }
        Framework framework;
    };
} // namespace

TEST_F(ServiceTrackerTestFixture, TestFilterString)
{
    BundleContext context = framework.GetBundleContext();
    MyCustomizer customizer(context);

    cppmicroservices::LDAPFilter filter("(" + cppmicroservices::Constants::SERVICE_ID + ">=0)");
    cppmicroservices::ServiceTracker<MyInterfaceOne> tracker(context, filter, &customizer);
    tracker.Open();

    struct MyServiceOne : public MyInterfaceOne
    {
    };
    struct MyServiceTwo : public MyInterfaceTwo
    {
    };

    auto serviceOne = std::make_shared<MyServiceOne>();
    auto serviceTwo = std::make_shared<MyServiceTwo>();

    context.RegisterService<MyInterfaceOne>(serviceOne);
    context.RegisterService<MyInterfaceTwo>(serviceTwo);

    EXPECT_EQ(tracker.GetServiceReferences().size(), 1) << "tracking count";
}

TEST_F(ServiceTrackerTestFixture, TestServiceTracker)
{
    BundleContext context = framework.GetBundleContext();

    auto bundle = cppmicroservices::testing::InstallLib(context, "TestBundleS");
    bundle.Start();

    // 1. Create a ServiceTracker with ServiceTrackerCustomizer == null

    std::string s1("cppmicroservices::TestBundleSService");
    ServiceReferenceU servref = context.GetServiceReference(s1 + "0");

    ASSERT_TRUE(servref) << "Test if registered service of id cppmicroservices::TestBundleSService0";

    ServiceReference<ServiceControlInterface> servCtrlRef = context.GetServiceReference<ServiceControlInterface>();
    ASSERT_TRUE(servCtrlRef) << "Test if constrol service was registered";

    auto serviceController = context.GetService(servCtrlRef);
    ASSERT_TRUE(serviceController) << "Test valid service controller";

    std::unique_ptr<ServiceTracker<void>> st1(new ServiceTracker<void>(context, servref));

    // 2. Check the size method with an unopened service tracker

    ASSERT_EQ(st1->Size(), 0) << "Test if size == 0";

    // 3. Open the service tracker and see what it finds,
    // expect to find one instance of the implementation,
    // "org.cppmicroservices.TestBundleSService0"

    st1->Open();
    std::vector<ServiceReferenceU> sa2 = st1->GetServiceReferences();

    ASSERT_EQ(sa2.size(), 1) << "Checking ServiceTracker size";
    ASSERT_EQ(s1 + "0", sa2[0].GetInterfaceId()) << "Checking service implementation name";

#ifdef US_ENABLE_THREADING_SUPPORT
    // 4. Test notifications via closing the tracker
    {
        ServiceTracker<void> st2(context, "dummy");
        st2.Open();

        // wait indefinitely
        auto fut1 = std::async(std::launch::async, [&st2] { return st2.WaitForService(); });
        // wait "long enough"
        auto fut2 = std::async(std::launch::async, [&st2] { return st2.WaitForService(std::chrono::minutes(1)); });

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        ASSERT_EQ(fut1.wait_for(std::chrono::milliseconds(1)), US_FUTURE_TIMEOUT) << "Waiter not notified yet";
        ASSERT_EQ(fut2.wait_for(std::chrono::milliseconds(1)), US_FUTURE_TIMEOUT) << "Waiter not notified yet";

        st2.Close();

        // Closing the tracker should notify the waiters
        auto wait_until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        ASSERT_EQ(fut1.wait_until(wait_until), US_FUTURE_READY) << "Closed service tracker notifies waiters";
        ASSERT_EQ(fut2.wait_until(wait_until), US_FUTURE_READY) << "Closed service tracker notifies waiters";
    }
#endif

    // 5. Close this service tracker
    st1->Close();

    // 6. Check the size method, now when the servicetracker is closed
    ASSERT_EQ(st1->Size(), 0) << "Checking ServiceTracker size";

    // 7. Check if we still track anything , we should get null
    sa2 = st1->GetServiceReferences();
    ASSERT_TRUE(sa2.empty()) << "Checking ServiceTracker size";

    // 8. A new Servicetracker, this time with a filter for the object
    std::string fs = std::string("(") + Constants::OBJECTCLASS + "=" + s1 + "*" + ")";
    LDAPFilter f1(fs);
    st1.reset(new ServiceTracker<void>(context, f1));
    // add a service
    serviceController->ServiceControl(1, "register", 7);

    // 9. Open the service tracker and see what it finds,
    // expect to find two instances of references to
    // "org.cppmicroservices.TestBundleSService*"
    // i.e. they refer to the same piece of code

    std::vector<std::string> ids;
    ids.push_back((s1 + "0"));
    ids.push_back((s1 + "1"));
    ids.push_back((s1 + "2"));
    ids.push_back((s1 + "3"));

    st1->Open();
    sa2 = st1->GetServiceReferences();
    ASSERT_EQ(sa2.size(), 2) << "Checking service reference count";
    ASSERT_TRUE(CheckConvertibility(sa2, ids.cbegin(), ids.cbegin() + 2)) << "Check for 2 expected interface ids";

    // 10. Get libTestBundleS to register one more service and see if it appears
    serviceController->ServiceControl(2, "register", 1);
    sa2 = st1->GetServiceReferences();

    ASSERT_EQ(sa2.size(), 3) << "Checking service reference count";

    ASSERT_TRUE(CheckConvertibility(sa2, ids.cbegin(), ids.cbegin() + 3)) << "Check for 3 expected interface ids";

    // 11. Get libTestBundleS to register one more service and see if it appears
    serviceController->ServiceControl(3, "register", 2);
    sa2 = st1->GetServiceReferences();
    ASSERT_EQ(sa2.size(), 4) << "Checking service reference count";
    ASSERT_TRUE(CheckConvertibility(sa2, ids.cbegin(), ids.cend())) << "Check for 4 expected interface ids";

    // 12. Get libTestBundleS to unregister one service and see if it disappears
    serviceController->ServiceControl(3, "unregister", 0);
    sa2 = st1->GetServiceReferences();
    ASSERT_EQ(sa2.size(), 3) << "Checking service reference count";

    // 13. Get the highest ranking service reference, it should have ranking 7
    ServiceReferenceU h1 = st1->GetServiceReference();
    int rank = any_cast<int>(h1.GetProperty(Constants::SERVICE_RANKING));
    ASSERT_EQ(rank, 7) << "Check service rank";

    // 14. Get the service of the highest ranked service reference

    auto o1 = st1->GetService(h1);
    ASSERT_TRUE(o1.get() != nullptr && !o1->empty()) << "Check for non-null service";

    // 14a Get the highest ranked service, directly this time
    auto o3 = st1->GetService();
    ASSERT_TRUE(o3.get() != nullptr && !o3->empty()) << "Check for non-null service";
    ASSERT_EQ(o1, o3) << "Check for equal service instances";

    // 15. Now release the tracking of that service and then try to get it
    //     from the servicetracker, which should yield a null object
    serviceController->ServiceControl(1, "unregister", 7);
    auto o2 = st1->GetService(h1);
    ASSERT_TRUE(!o2 || !o2.get()) << "Check that service is null";

    // 16. Get all service objects this tracker tracks, it should be 2
    auto ts1 = st1->GetServices();
    ASSERT_EQ(ts1.size(), 2) << "Check service count";

    // 17. Test the remove method.
    //     First register another service, then remove it being tracked
    serviceController->ServiceControl(1, "register", 7);
    h1 = st1->GetServiceReference();
    auto sa3 = st1->GetServiceReferences();
    ASSERT_EQ(sa3.size(), 3) << "Check service reference count";
    ASSERT_TRUE(CheckConvertibility(sa3, ids.cbegin(), ids.cbegin() + 3)) << "Check for 3 expected interface ids";

    st1->Remove(h1); // remove tracking on one servref
    sa2 = st1->GetServiceReferences();
    ASSERT_EQ(sa2.size(), 2) << "Check service reference count";

    // 18. Test the addingService method,add a service reference

    // 19. Test the removedService method, remove a service reference

    // 20. Test the waitForService method
    auto o9 = st1->WaitForService(std::chrono::milliseconds(50));
    ASSERT_TRUE(o9 && !o9->empty()) << "Checking WaitForService method";

    // Test that the RemovedService callback is triggered when closing a service tracker
    MockCustomizedServiceTracker<MyInterfaceOne> customizer;

    // expect that closing the tracker results in RemovedService being called.
    ON_CALL(customizer, AddingService(::testing::_))
        .WillByDefault(::testing::Return(std::make_shared<MyInterfaceOne>()));
    EXPECT_CALL(customizer, AddingService(::testing::_)).Times(::testing::Exactly(1));
    EXPECT_CALL(customizer, ModifiedService(::testing::_, ::testing::_)).Times(::testing::Exactly(0));
    EXPECT_CALL(customizer, RemovedService(::testing::_, ::testing::_)).Times(::testing::Exactly(1));

    auto tracker = std::make_unique<cppmicroservices::ServiceTracker<MyInterfaceOne>>(context, &customizer);
    tracker->Open();
    struct MyServiceOne : public MyInterfaceOne
    {
    };
    auto svcReg = context.RegisterService<MyInterfaceOne>(std::make_shared<MyServiceOne>());
    tracker->Close();
}

TEST_F(ServiceTrackerTestFixture, GetTrackingCount)
{
    BundleContext context = framework.GetBundleContext();

    cppmicroservices::ServiceTracker<MyInterfaceOne> tracker(context);
    ASSERT_EQ(tracker.GetTrackingCount(), -1);
    tracker.Open();
    ASSERT_EQ(tracker.GetTrackingCount(), 0);

    struct MyServiceOne : public MyInterfaceOne
    {
    };
    auto svcReg = context.RegisterService<MyInterfaceOne>(std::make_shared<MyServiceOne>());
    ASSERT_EQ(tracker.GetTrackingCount(), 1);

    svcReg.SetProperties(ServiceProperties({
        {"foo", Any { 1 }}
    }));
    ASSERT_EQ(tracker.GetTrackingCount(), 2);

    (void)context.RegisterService<MyInterfaceOne>(std::make_shared<MyServiceOne>());
    ASSERT_EQ(tracker.GetTrackingCount(), 3);

    svcReg.Unregister();
    ASSERT_EQ(tracker.GetTrackingCount(), 4);

    tracker.Close();
    ASSERT_EQ(tracker.GetTrackingCount(), -1);
}

TEST_F(ServiceTrackerTestFixture, GetTracked)
{
    BundleContext context = framework.GetBundleContext();
    cppmicroservices::ServiceTracker<MyInterfaceOne> tracker(context);
    std::unordered_map<ServiceReference<MyInterfaceOne>, std::shared_ptr<MyInterfaceOne>> tracked;
    tracker.GetTracked(tracked);
    ASSERT_TRUE(tracked.empty());
    tracker.Open();
    ASSERT_TRUE(tracked.empty());

    struct MyServiceOne : public MyInterfaceOne
    {
    };
    auto svcReg = context.RegisterService<MyInterfaceOne>(std::make_shared<MyServiceOne>());
    tracker.GetTracked(tracked);
    ASSERT_EQ(tracked.size(), 1ul);

    tracked.clear();
    tracker.Close();
    tracker.GetTracked(tracked);
    ASSERT_TRUE(tracked.empty());
}

TEST_F(ServiceTrackerTestFixture, IsEmpty)
{
    BundleContext context = framework.GetBundleContext();
    cppmicroservices::ServiceTracker<MyInterfaceOne> tracker(context);
    ASSERT_TRUE(tracker.IsEmpty());
    tracker.Open();
    ASSERT_TRUE(tracker.IsEmpty());

    struct MyServiceOne : public MyInterfaceOne
    {
    };
    auto svcReg = context.RegisterService<MyInterfaceOne>(std::make_shared<MyServiceOne>());
    ASSERT_FALSE(tracker.IsEmpty());

    tracker.Close();
    ASSERT_TRUE(tracker.IsEmpty());
}

#ifdef US_ENABLE_THREADING_SUPPORT
namespace
{
    class FooService
    {
      public:
        virtual ~FooService() = default;
    };

    class FooServiceImpl final : public FooService
    {
      public:
        ~FooServiceImpl() = default;
    };

    class CustomFooTracker final : public cppmicroservices::ServiceTrackerCustomizer<FooService>
    {
        /**
         * Called when a service is started.
         */
        std::shared_ptr<FooService>
        AddingService(::cppmicroservices::ServiceReference<FooService> const& reference) override
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return reference.GetBundle().GetBundleContext().GetService(reference);
        }

        /**
         * Called when a service is modified. We don't care about this.
         */
        void
        ModifiedService(::cppmicroservices::ServiceReference<FooService> const&,
                        std::shared_ptr<FooService> const&) override
        {
        }

        /**
         * Called when a service is removed.
         */
        void
        RemovedService(::cppmicroservices::ServiceReference<FooService> const&,
                       std::shared_ptr<FooService> const&) override
        {
        }
    };
} // namespace

TEST_F(ServiceTrackerTestFixture, ServiceTrackerCloseRace)
{
    BundleContext context = framework.GetBundleContext();
    // test for a race in SerivceTracker<T>::Close
    // that leads to a crash inside the service tracker.
    auto customTracker = std::make_unique<CustomFooTracker>();
    auto tracker = std::make_unique<cppmicroservices::ServiceTracker<FooService>>(context, customTracker.get());

    tracker->Open();

    // set sleeps at certain points in the code to
    // generate the conditions for the race.
    std::promise<void> gate;
    auto gateFuture = gate.get_future();
    auto fut = std::async(std::launch::async,
                          [&context, &gateFuture]()
                          {
                              gateFuture.get();
                              (void)context.RegisterService<FooService>(std::make_shared<FooServiceImpl>());
                          });

    gate.set_value();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    tracker->Close();

    fut.get();
}

TEST_F(ServiceTrackerTestFixture, DefaultCustomizerServiceTrackerCloseRace)
{
    BundleContext context = framework.GetBundleContext();
    // test for a race in SerivceTracker<T>::Close when no user provided
    // customizer is specified and a service event is being processed by
    // the service tracker while it is being destroyed.
    std::promise<void> gate;
    auto gateFuture = gate.get_future();

    std::atomic_bool keepRegisteringServices { true };

    auto fut = std::async(std::launch::async,
                          [&context, &gateFuture, &keepRegisteringServices]()
                          {
                              gateFuture.get();
                              while (keepRegisteringServices)
                              {
                                  (void)context.RegisterService<FooService>(std::make_shared<FooServiceImpl>());
                              }
                          });

    gate.set_value();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        ServiceTracker<FooService> scopedTracker(context);
        scopedTracker.Open();
    } // destroy scopedTracker

    keepRegisteringServices.store(false);
    fut.get();
}

TEST_F(ServiceTrackerTestFixture, ServiceTrackerConcurrentOpenClose)
{
    BundleContext context = framework.GetBundleContext();
    auto customTracker = std::make_unique<CustomFooTracker>();
    auto tracker = std::make_unique<cppmicroservices::ServiceTracker<FooService>>(context, customTracker.get());

    size_t numThreads = std::thread::hardware_concurrency();
    ASSERT_GT(numThreads, 0ull) << "number of threads is 0";
    std::vector<std::future<void>> futures;
    std::promise<void> gate;
    auto gateFuture = gate.get_future().share();
    for (size_t i = 0; i <= numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async,
                                     [i, &tracker, &gateFuture]()
                                     {
                                         gateFuture.get();
                                         for (int n = 0; n < 1000; ++n)
                                         {
                                             if (i % 2 == 0)
                                             {
                                                 tracker->Open();
                                             }
                                             else
                                             {
                                                 tracker->Close();
                                             }
                                         }
                                     }));
    }

    gate.set_value();

    for (auto& asyncFuture : futures)
    {
        asyncFuture.get();
    }
}
#endif

namespace
{

    class MyNullPtrCustomizer final : public cppmicroservices::ServiceTrackerCustomizer<MyInterfaceOne>
    {

      public:
        MyNullPtrCustomizer(BundleContext const& context) : m_context(context) {}

        virtual std::shared_ptr<MyInterfaceOne>
        AddingService(ServiceReference<MyInterfaceOne> const&)
        {
            return nullptr;
        }

        virtual void
        ModifiedService(ServiceReference<MyInterfaceOne> const&, std::shared_ptr<MyInterfaceOne> const&)
        {
        }

        virtual void
        RemovedService(ServiceReference<MyInterfaceOne> const&, std::shared_ptr<MyInterfaceOne> const&)
        {
        }

      private:
        BundleContext m_context;
    };
} // namespace

TEST_F(ServiceTrackerTestFixture, TestNullPtrServiceTrackerCustomizer)
{
    auto context = framework.GetBundleContext();
    MyNullPtrCustomizer customizer(context);

    cppmicroservices::LDAPFilter filter("(" + cppmicroservices::Constants::SERVICE_ID + ">=0)");
    cppmicroservices::ServiceTracker<MyInterfaceOne> tracker(context, filter, &customizer);
    tracker.Open();

    struct MyServiceOne : public MyInterfaceOne
    {
    };

    auto serviceOne = std::make_shared<MyServiceOne>();

    context.RegisterService<MyInterfaceOne>(serviceOne);

    ASSERT_EQ(tracker.GetServiceReferences().size(), 0) << "tracking count should be 0";

    auto trackedObj = tracker.WaitForService(std::chrono::seconds(1));
    ASSERT_EQ(nullptr, trackedObj) << "tracked object should be nullptr";
}

/// <summary>
/// opening a closed tracker should have it receive adding, modified and removed service events.
/// </summary>
TEST_F(ServiceTrackerTestFixture, TestReOpenServiceTrackerWithCustomizer)
{
    auto context = framework.GetBundleContext();
    MockCustomizedServiceTracker<MyInterfaceOne> customizer;

    // expect that closing the tracker results in RemovedService being called.
    ON_CALL(customizer, AddingService(::testing::_))
        .WillByDefault(::testing::Return(std::make_shared<MyInterfaceOne>()));
    EXPECT_CALL(customizer, AddingService(::testing::_)).Times(::testing::Exactly(3));
    EXPECT_CALL(customizer, ModifiedService(::testing::_, ::testing::_)).Times(::testing::Exactly(1));
    EXPECT_CALL(customizer, RemovedService(::testing::_, ::testing::_)).Times(::testing::Exactly(3));

    auto tracker = std::make_unique<cppmicroservices::ServiceTracker<MyInterfaceOne>>(context, &customizer);
    tracker->Open();
    struct MyServiceOne : public MyInterfaceOne
    {
    };
    auto svcReg = context.RegisterService<MyInterfaceOne>(std::make_shared<MyServiceOne>());
    tracker->Close();

    tracker->Open();
    auto svcReg2 = context.RegisterService<MyInterfaceOne>(std::make_shared<MyServiceOne>());
    svcReg2.SetProperties(ServiceProperties({
        {"test", Any(std::string("foo"))}
    }));
    tracker->Close();
}

namespace
{
    namespace foo
    {
        class Bar
        {
          public:
            virtual ~Bar() {}
        };
    } // namespace foo

    class MockFooBar : public foo::Bar
    {
    };
} // namespace

TEST_F(ServiceTrackerTestFixture, TestFilterPropertiesTypes)
{
    LDAPFilter filter("(tag=foo::bar::Baz)");
    ServiceTracker<foo::Bar> tracker(framework.GetBundleContext(), filter);
    tracker.Open();

    auto fooService = std::make_shared<MockFooBar>();
    cppmicroservices::ServiceProperties props
        = std::initializer_list<cppmicroservices::ServiceProperties::value_type> { std::make_pair("tag",
                                                                                                  ("foo::bar::Baz")) };

    auto svc = framework.GetBundleContext().RegisterService<foo::Bar>(fooService, std::move(props));

    ASSERT_EQ(tracker.GetTrackingCount(), 1);

    tracker.Close();
}

#ifdef US_ENABLE_THREADING_SUPPORT

TEST(ServiceTrackerTests, TestServiceTrackerDeadlock)
{
    Framework framework = FrameworkFactory().NewFramework();
    framework.Start();

    ServiceTracker<MyInterfaceOne> tracker(framework.GetBundleContext());
    tracker.Open();
    auto f = std::async(
        [&framework]
        {
            // technically there's a race here, so the async should ensure WaitForService is started prior to
            // terminating the framework.
            std::this_thread::sleep_for(std::chrono::seconds(1));

            framework.Stop();
            framework.WaitForStop(std::chrono::milliseconds::zero());
        });

    ASSERT_THROW(tracker.WaitForService(), std::logic_error);
}

TEST(ServiceTrackerTests, TestServiceTrackerInvalidBundle)
{
    Framework framework = FrameworkFactory().NewFramework();
    framework.Start();

    ServiceTracker<MyInterfaceOne> tracker(framework.GetBundleContext());
    tracker.Open();

    framework.Stop();
    framework.WaitForStop(std::chrono::milliseconds::zero());

    ASSERT_THROW(tracker.WaitForService(), std::logic_error);
}

// If the test doesn't throw, it is successful.
// Intended to be run with many repititions to test for sporadic failures.
TEST(ServiceTrackerTests, FrameworkTrackerCloseRace)
{
    auto framework = cppmicroservices::FrameworkFactory().NewFramework();
    framework.Start();

    ServiceTracker<MyInterfaceOne> tracker(framework.GetBundleContext());
    tracker.Open();

    auto interfaceOneService = std::make_shared<MyInterfaceOne>();

    auto svc = framework.GetBundleContext().RegisterService<MyInterfaceOne>(interfaceOneService);

    auto fut = std::async(std::launch::async,
                          [&framework]()
                          {
                              framework.Stop();
                              framework.WaitForStop(std::chrono::milliseconds::zero());
                          });
    tracker.Close();
}

#endif
