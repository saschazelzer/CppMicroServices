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
#include <chrono>
#include <cstdio>
#include <iostream>

#include "gmock/gmock.h"

#include <cppmicroservices/BundleContext.h>
#include <cppmicroservices/BundleImport.h>
#include <cppmicroservices/Constants.h>
#include <cppmicroservices/Framework.h>
#include <cppmicroservices/FrameworkEvent.h>
#include <cppmicroservices/FrameworkFactory.h>

#include "cppmicroservices/servicecomponent/detail/ComponentInstanceImpl.hpp"
#include <cppmicroservices/ServiceInterface.h>

using cppmicroservices::service::component::ComponentContext;
using cppmicroservices::service::component::detail::Binder;
using cppmicroservices::service::component::detail::ComponentInstanceImpl;
using cppmicroservices::service::component::detail::DynamicBinder;
using cppmicroservices::service::component::detail::StaticBinder;

namespace
{

    // dummy types used for testing
    struct ServiceDependency1
    {
    };
    struct ServiceDependency2
    {
    };
    struct TestServiceInterface1
    {
    };
    struct ServiceDependency3
    {
    };
    struct TestServiceInterface2
    {
    };
    struct TestServiceInterface3
    {
    };

    // Test class to simulate a service component
    // TODO: Use Mock to add more behavior verification
    class TestServiceImpl1 final
        : public TestServiceInterface1
        , public TestServiceInterface2
    {
      public:
        TestServiceImpl1() : foo(nullptr), bar(nullptr), activated(false) {}

        TestServiceImpl1(std::shared_ptr<ServiceDependency1> const& f, std::shared_ptr<ServiceDependency2> const& b)
            : foo(f)
            , bar(b)
            , activated(false)
        {
        }

        TestServiceImpl1(std::shared_ptr<ServiceDependency2> const& b) : foo(nullptr), bar(b), activated(false) {}

        virtual ~TestServiceImpl1() {}

        void
        BindFoo(std::shared_ptr<ServiceDependency1> const& f)
        {
            foo = f;
        }

        void
        UnbindFoo(std::shared_ptr<ServiceDependency1> const& f)
        {
            if (foo == f)
            {
                foo = nullptr;
            }
        }

        void
        Activate()
        {
            // this is the wrong signature method.
        }

        void
        Activate(std::shared_ptr<ComponentContext> const&)
        {
            activated = true;
        }

        void
        Deactivate(std::shared_ptr<ComponentContext> const&)
        {
            activated = false;
        }

        void
        Deactivate(std::shared_ptr<ComponentContext> const&, bool)
        {
            // this is the wrong signature method.
        }

        bool
        IsActivated()
        {
            return activated;
        }

        std::shared_ptr<ServiceDependency1>
        GetFoo() const
        {
            return foo;
        }
        std::shared_ptr<ServiceDependency2>
        GetBar() const
        {
            return bar;
        }

      private:
        std::shared_ptr<ServiceDependency1> foo; // dynamic dependency - can change during the lifetime of this object
        std::shared_ptr<ServiceDependency2> const
            bar; // static dependency - does not change during the lifetime of this object
        bool activated;
    };

    // Test class to simulate a service component with service dependencies using multiple cardinality
    class TestServiceImpl2
    {
      public:
        TestServiceImpl2(std::vector<std::shared_ptr<ServiceDependency1>> const& d1,
                         std::shared_ptr<ServiceDependency2> const& d2,
                         std::vector<std::shared_ptr<ServiceDependency3>> const& d3)
            : dep1Refs(d1)
            , dep2(d2)
            , dep3Refs(d3)
        {
        }

        TestServiceImpl2(std::vector<std::shared_ptr<ServiceDependency1>> const& d1,
                         std::shared_ptr<ServiceDependency2> const& d2)
            : dep1Refs(d1)
            , dep2(d2)
            , dep3Refs({})
        {
        }

        virtual ~TestServiceImpl2() {}

        void
        BindDep3(std::shared_ptr<ServiceDependency3> const& d3)
        {
            if (std::find(dep3Refs.begin(), dep3Refs.end(), d3) == dep3Refs.end())
            {
                dep3Refs.push_back(d3);
            }
        }

        void
        UnbindDep3(std::shared_ptr<ServiceDependency3> const& d3)
        {
            auto pos = std::find(dep3Refs.begin(), dep3Refs.end(), d3);
            if (pos != dep3Refs.end())
            {
                *pos = nullptr;
                dep3Refs.erase(pos);
            }
        }

        void
        Activate(std::shared_ptr<ComponentContext> const&)
        {
            // activated = true;
        }

        void
        Deactivate(std::shared_ptr<ComponentContext> const&)
        {
            // activated = false;
        }

        std::vector<std::shared_ptr<ServiceDependency1>>
        GetDep1() const
        {
            return dep1Refs;
        }

        std::shared_ptr<ServiceDependency2>
        GetDep2() const
        {
            return dep2;
        }

        std::vector<std::shared_ptr<ServiceDependency3>>
        GetDep3() const
        {
            return dep3Refs;
        }

      private:
        std::vector<std::shared_ptr<ServiceDependency1>> const dep1Refs; // static dependency with multiple cardinality
        std::shared_ptr<ServiceDependency2> const dep2;                  // static dependency with unary cardinality
        std::vector<std::shared_ptr<ServiceDependency3>> dep3Refs;       // dynamic dependency with multiple cardinality
    };

    class TestServiceImplWithRefRefDep : public TestServiceInterface1
    {
      public:
        TestServiceImplWithRefRefDep() = default;

        TestServiceImplWithRefRefDep(std::shared_ptr<ServiceDependency1> const&& f) : foo(f) {}

        TestServiceImplWithRefRefDep(TestServiceImplWithRefRefDep const& other) = default;
        TestServiceImplWithRefRefDep(TestServiceImplWithRefRefDep&& other) noexcept = default;
        TestServiceImplWithRefRefDep& operator=(TestServiceImplWithRefRefDep const& other) = default;
        TestServiceImplWithRefRefDep& operator=(TestServiceImplWithRefRefDep&& other) noexcept = default;

        virtual ~TestServiceImplWithRefRefDep() = default;

        [[nodiscard]] std::shared_ptr<ServiceDependency1>
        GetFoo() const
        {
            return foo;
        }

      private:
        std::shared_ptr<ServiceDependency1> foo;
    };

    class TestServiceImplWithSharedPtrDep : public TestServiceInterface1
    {
      public:
        TestServiceImplWithSharedPtrDep() = default;

        TestServiceImplWithSharedPtrDep(std::shared_ptr<ServiceDependency1> f) : foo(f) {}

        TestServiceImplWithSharedPtrDep(TestServiceImplWithSharedPtrDep const& other) = default;
        TestServiceImplWithSharedPtrDep(TestServiceImplWithSharedPtrDep&& other) noexcept = default;
        TestServiceImplWithSharedPtrDep& operator=(TestServiceImplWithSharedPtrDep const& other) = default;
        TestServiceImplWithSharedPtrDep& operator=(TestServiceImplWithSharedPtrDep&& other) noexcept = default;

        virtual ~TestServiceImplWithSharedPtrDep() = default;

        [[nodiscard]] std::shared_ptr<ServiceDependency1>
        GetFoo() const
        {
            return foo;
        }

      private:
        std::shared_ptr<ServiceDependency1> foo;
    };

    class TestServiceImplWithDefaultCtor : public TestServiceInterface1
    {
      public:
        TestServiceImplWithDefaultCtor() = default;
        ~TestServiceImplWithDefaultCtor() = default;

        TestServiceImplWithDefaultCtor(TestServiceImplWithDefaultCtor const& other) = default;
        TestServiceImplWithDefaultCtor& operator=(TestServiceImplWithDefaultCtor const& other) = default;
        TestServiceImplWithDefaultCtor(TestServiceImplWithDefaultCtor&& other) noexcept = default;
        TestServiceImplWithDefaultCtor& operator=(TestServiceImplWithDefaultCtor&& other) noexcept = default;

        bool
        defCon()
        {
            return def;
        }

      private:
        std::shared_ptr<cppmicroservices::AnyMap> props;
        bool def = true;
    };

    class TestServiceImplWithConfigAndDefaultCtor : public TestServiceInterface1
    {
      public:
        TestServiceImplWithConfigAndDefaultCtor() = default;
        TestServiceImplWithConfigAndDefaultCtor(std::shared_ptr<cppmicroservices::AnyMap> properties)
            : props(properties)
            , def(false)
        {
            return;
        }
        ~TestServiceImplWithConfigAndDefaultCtor() = default;

        TestServiceImplWithConfigAndDefaultCtor(TestServiceImplWithConfigAndDefaultCtor const& other) = default;
        TestServiceImplWithConfigAndDefaultCtor& operator=(TestServiceImplWithConfigAndDefaultCtor const& other)
            = default;
        TestServiceImplWithConfigAndDefaultCtor(TestServiceImplWithConfigAndDefaultCtor&& other) noexcept = default;
        TestServiceImplWithConfigAndDefaultCtor& operator=(TestServiceImplWithConfigAndDefaultCtor&& other) noexcept
            = default;

        bool
        defCon()
        {
            return def;
        }

      private:
        std::shared_ptr<cppmicroservices::AnyMap> props;
        bool def = true;
    };

    /**
     * This class is used to mock the behavior of a ComponentContext object
     * created by the declarative services runtime implementation.
     */
    class MockComponentContext : public ComponentContext
    {
      public:
        MOCK_CONST_METHOD0(GetProperties, std::unordered_map<std::string, cppmicroservices::Any>(void));
        MOCK_CONST_METHOD0(GetBundleContext, cppmicroservices::BundleContext(void));
        MOCK_CONST_METHOD0(GetUsingBundle, cppmicroservices::Bundle(void));
        MOCK_CONST_METHOD0(GetServiceReference, cppmicroservices::ServiceReferenceBase(void));
        MOCK_METHOD1(EnableComponent, void(std::string const&));
        MOCK_METHOD1(DisableComponent, void(std::string const&));
        MOCK_CONST_METHOD2(LocateServices, std::vector<std::shared_ptr<void>>(std::string const&, std::string const&));
        MOCK_CONST_METHOD2(LocateService, std::shared_ptr<void>(std::string const&, std::string const&));
    };

    /**
     * This test point is used to verify the ComponentInstanceImpl works properly
     * for a Service Component that does not provide any services and does not
     * consume any services
     */
    TEST(ComponentInstance, VerifyZeroServicesZeroDependencies)
    {
        ComponentInstanceImpl<TestServiceImpl1> compInstance;
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_FALSE(iMap); // empty map is returned since no service is provided
        auto compObj = compInstance.GetInstance();
        ASSERT_FALSE(compObj);
        auto mockContext = std::make_shared<MockComponentContext>();
        EXPECT_CALL(*(mockContext.get()), LocateService(testing::_, testing::_))
            .Times(0); // ensure the mock context never gets a call to LocateService
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure none of the dependencies are bound. Dependencies are only bound if they are declared.
        ASSERT_FALSE(compObj->GetFoo());
        ASSERT_FALSE(compObj->GetBar());
    }

    TEST(ComponentInstanceImpl, VerifyZeroServicesZeroDependencies)
    {
        ComponentInstanceImpl<TestServiceImpl1> compInstance;
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_FALSE(iMap); // empty map is returned since no service is provided
        auto compObj = compInstance.GetInstance();
        ASSERT_FALSE(compObj);
        auto mockContext = std::make_shared<MockComponentContext>();
        EXPECT_CALL(*(mockContext.get()), LocateService(testing::_, testing::_))
            .Times(0); // ensure the mock context never gets a call to LocateService
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure none of the dependencies are bound. Dependencies are only bound if they are declared.
        ASSERT_FALSE(compObj->GetFoo());
        ASSERT_FALSE(compObj->GetBar());
    }

    TEST(ComponentInstance, validateConstructorCall)
    {
        ComponentInstanceImpl<TestServiceImplWithDefaultCtor, std::tuple<TestServiceInterface1>> compInstance;
        auto mockContext = std::make_shared<MockComponentContext>();
        EXPECT_CALL(*(mockContext.get()), LocateService(testing::_, testing::_))
            .Times(0); // ensure the mock context never gets a call to LocateService
        compInstance.CreateInstance(mockContext);
        ASSERT_EQ(compInstance.GetInstance()->defCon(), true);

        ComponentInstanceImpl<TestServiceImplWithConfigAndDefaultCtor, std::tuple<TestServiceInterface1>> compInstance2;
        compInstance2.CreateInstance(mockContext);
        ASSERT_EQ(compInstance2.GetInstance()->defCon(), false);
    }
    /**
     * This test point is used to verify the ComponentInstanceImpl works properly
     * for a Service Component that provides a single service but does not consume
     * any services
     */
    TEST(ComponentInstance, VerifyWithSingleServiceZeroDependencies)
    {
        ComponentInstanceImpl<TestServiceImpl1, std::tuple<TestServiceInterface1>> compInstance;
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_TRUE(iMap);
        ASSERT_EQ(iMap->size(), size_t(1));
        ASSERT_EQ(iMap->count(us_service_interface_iid<TestServiceInterface1>()), size_t(1));
        auto mockContext = std::make_shared<MockComponentContext>();
        EXPECT_CALL(*(mockContext.get()), LocateService(testing::_, testing::_))
            .Times(0); // ensure the mock context never gets a call to LocateService
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure none of the dependencies are bound. Dependencies are only bound if they are declared.
        ASSERT_FALSE(compObj->GetFoo());
        ASSERT_FALSE(compObj->GetBar());
    }

    TEST(ComponentInstanceImpl, VerifyWithSingleServiceZeroDependencies)
    {
        ComponentInstanceImpl<TestServiceImpl1, std::tuple<TestServiceInterface1>> compInstance;
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_TRUE(iMap);
        ASSERT_EQ(iMap->size(), size_t(1));
        ASSERT_EQ(iMap->count(us_service_interface_iid<TestServiceInterface1>()), size_t(1));
        auto mockContext = std::make_shared<MockComponentContext>();
        EXPECT_CALL(*(mockContext.get()), LocateService(testing::_, testing::_))
            .Times(0); // ensure the mock context never gets a call to LocateService
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure none of the dependencies are bound. Dependencies are only bound if they are declared.
        ASSERT_FALSE(compObj->GetFoo());
        ASSERT_FALSE(compObj->GetBar());
    }

    /**
     * This test point is used to verify the ComponentInstanceImpl works properly
     * for a Service Component that provides multiple services but does not consume
     * any services
     */
    TEST(ComponentInstance, VerifyWithMultipleServiceZeroDependencies)
    {
        ComponentInstanceImpl<TestServiceImpl1, std::tuple<TestServiceInterface1, TestServiceInterface2>> compInstance;
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_TRUE(iMap);
        ASSERT_EQ(iMap->size(), size_t(2));
        ASSERT_EQ(iMap->count(us_service_interface_iid<TestServiceInterface1>()), size_t(1));
        ASSERT_EQ(iMap->count(us_service_interface_iid<TestServiceInterface2>()), size_t(1));
        auto mockContext = std::make_shared<MockComponentContext>();
        EXPECT_CALL(*(mockContext.get()), LocateService(testing::_, testing::_))
            .Times(0); // ensure the mock context never gets a call to LocateService
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure none of the dependencies are bound. Dependencies are only bound if they are declared.
        ASSERT_FALSE(compObj->GetFoo());
        ASSERT_FALSE(compObj->GetBar());
    }

    TEST(ComponentInstanceImpl, VerifyWithMultipleServiceZeroDependencies)
    {
        ComponentInstanceImpl<TestServiceImpl1, std::tuple<TestServiceInterface1, TestServiceInterface2>> compInstance;
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_TRUE(iMap);
        ASSERT_EQ(iMap->size(), size_t(2));
        ASSERT_EQ(iMap->count(us_service_interface_iid<TestServiceInterface1>()), size_t(1));
        ASSERT_EQ(iMap->count(us_service_interface_iid<TestServiceInterface2>()), size_t(1));
        auto mockContext = std::make_shared<MockComponentContext>();
        EXPECT_CALL(*(mockContext.get()), LocateService(testing::_, testing::_))
            .Times(0); // ensure the mock context never gets a call to LocateService
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure none of the dependencies are bound. Dependencies are only bound if they are declared.
        ASSERT_FALSE(compObj->GetFoo());
        ASSERT_FALSE(compObj->GetBar());
    }

    TEST(ComponentInstanceImpl, VerifyWithStaticDependencies)
    {
        auto f = cppmicroservices::FrameworkFactory().NewFramework();
        f.Start();
        auto fc = f.GetBundleContext();
        auto reg = fc.RegisterService<ServiceDependency1>(std::make_shared<ServiceDependency1>());
        auto reg1 = fc.RegisterService<ServiceDependency2>(std::make_shared<ServiceDependency2>());

        ComponentInstanceImpl<TestServiceImpl1,
                              std::tuple<>,
                              std::shared_ptr<ServiceDependency1>,
                              std::shared_ptr<ServiceDependency2>>
            compInstance({ ("foo"), ("bar") }, {});
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_FALSE(iMap);

        auto mockContext = std::make_shared<MockComponentContext>();
        auto locateService = [&fc](std::string const& type) -> std::shared_ptr<void>
        {
            auto sRef = fc.GetServiceReference(type);
            if (sRef)
            {
                auto serv = fc.GetService(sRef);
                return serv->at(type);
            }
            else
            {
                return nullptr;
            }
        };

        EXPECT_CALL(*(mockContext.get()), GetBundleContext()).WillRepeatedly(testing::Invoke([&fc]() { return fc; }));

        EXPECT_CALL(*(mockContext.get()), LocateService("foo", us_service_interface_iid<ServiceDependency1>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateService))); // ensure the mock context received a call to LocateService for dependency foo

        EXPECT_CALL(*(mockContext.get()), LocateService("bar", us_service_interface_iid<ServiceDependency2>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateService))); // ensure the mock context received a call to LocateService for dependency bar

        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure the dependencies are bound when the object is constructed
        ASSERT_TRUE(compObj->GetFoo());
        ASSERT_TRUE(compObj->GetBar());

        EXPECT_THROW(compInstance.InvokeBindMethod("foo", fc.GetServiceReference<ServiceDependency1>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeBindMethod("bar", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeUnbindMethod("foo", fc.GetServiceReference<ServiceDependency1>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeUnbindMethod("bar", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);

        f.Stop();
        f.WaitForStop(std::chrono::milliseconds::zero());
    }

    TEST(ComponentInstanceImpl, VerifyWithDynamicDependencies)
    {
        auto f = cppmicroservices::FrameworkFactory().NewFramework();
        f.Start();
        auto fc = f.GetBundleContext();
        auto reg = fc.RegisterService<ServiceDependency1>(std::make_shared<ServiceDependency1>());
        auto reg1 = fc.RegisterService<ServiceDependency2>(std::make_shared<ServiceDependency2>());

        std::vector<std::shared_ptr<Binder<TestServiceImpl1>>> binders;
        binders.push_back(
            std::make_shared<DynamicBinder<TestServiceImpl1, ServiceDependency1>>("foo",
                                                                                  &TestServiceImpl1::BindFoo,
                                                                                  &TestServiceImpl1::UnbindFoo));

        ComponentInstanceImpl<TestServiceImpl1, std::tuple<>, std::shared_ptr<ServiceDependency2>> compInstance(
            { ("bar") },
            binders);
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_FALSE(iMap);
        ASSERT_EQ(compInstance.GetInstance(), nullptr);

        // create a mock context and setup expectations on the context
        auto mockContext = std::make_shared<MockComponentContext>();
        auto locateService = [&fc](std::string const& type) -> std::shared_ptr<void>
        {
            auto sRef = fc.GetServiceReference(type);
            if (sRef)
            {
                auto serv = fc.GetService(sRef);
                return serv->at(type);
            }
            else
            {
                return nullptr;
            }
        };

        EXPECT_CALL(*(mockContext.get()), GetBundleContext()).WillRepeatedly(testing::Invoke([&fc]() { return fc; }));

        EXPECT_CALL(*(mockContext.get()), LocateService("foo", us_service_interface_iid<ServiceDependency1>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateService))); // ensure the mock context received a call to LocateService for dependency foo

        EXPECT_CALL(*(mockContext.get()), LocateService("bar", us_service_interface_iid<ServiceDependency2>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateService))); // ensure the mock context received a call to LocateService for dependency bar

        // use mock context to create an instance of the component implementation class.
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure the dependencies are bound when the object is constructed
        ASSERT_NE(compObj->GetFoo(), nullptr);
        ASSERT_NE(compObj->GetBar(), nullptr);

        // ensure only dynamic dependencies can be re-bound.
        // The runtime calls the wrapper object with the name of the reference and the ServiceReference object to use
        // for binding.
        EXPECT_THROW(compInstance.InvokeUnbindMethod("bar", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeBindMethod("bar", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);
        EXPECT_NO_THROW(compInstance.InvokeUnbindMethod("foo", fc.GetServiceReference<ServiceDependency1>()));
        ASSERT_EQ(compObj->GetFoo(), nullptr);
        EXPECT_NO_THROW(compInstance.InvokeBindMethod("foo", fc.GetServiceReference<ServiceDependency1>()));
        ASSERT_NE(compObj->GetFoo(), nullptr);

        f.Stop();
        f.WaitForStop(std::chrono::milliseconds::zero());
    }

    TEST(ComponentInstanceImpl, VerifyRRefDependencyConstruct)
    {
        auto f = cppmicroservices::FrameworkFactory().NewFramework();
        f.Start();
        auto fc = f.GetBundleContext();
        auto reg = fc.RegisterService<ServiceDependency1>(std::make_shared<ServiceDependency1>());

        ComponentInstanceImpl<TestServiceImplWithRefRefDep, std::tuple<>, std::shared_ptr<ServiceDependency1>>
            compInstance({ ("foo") }, {});

        auto mockContext = std::make_shared<MockComponentContext>();
        auto locateService = [&fc](std::string const& type) -> std::shared_ptr<void>
        {
            auto sRef = fc.GetServiceReference(type);
            if (sRef)
            {
                auto serv = fc.GetService(sRef);
                return serv->at(type);
            }
            else
            {
                return nullptr;
            }
        };

        EXPECT_CALL(*(mockContext.get()), GetBundleContext()).WillRepeatedly(testing::Invoke([&fc]() { return fc; }));

        EXPECT_CALL(*(mockContext.get()), LocateService("foo", us_service_interface_iid<ServiceDependency1>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateService))); // ensure the mock context received a call to LocateService for dependency foo

        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure the dependencies are bound when the object is constructed
        ASSERT_TRUE(compObj->GetFoo());

        f.Stop();
        f.WaitForStop(std::chrono::milliseconds::zero());
    }

    TEST(ComponentInstanceImpl, VerifySharedPtrDependencyConstruct)
    {
        auto f = cppmicroservices::FrameworkFactory().NewFramework();
        f.Start();
        auto fc = f.GetBundleContext();
        auto reg = fc.RegisterService<ServiceDependency1>(std::make_shared<ServiceDependency1>());

        ComponentInstanceImpl<TestServiceImplWithSharedPtrDep, std::tuple<>, std::shared_ptr<ServiceDependency1>>
            compInstance({ ("foo") }, {});

        auto mockContext = std::make_shared<MockComponentContext>();
        auto locateService = [&fc](std::string const& type) -> std::shared_ptr<void>
        {
            auto sRef = fc.GetServiceReference(type);
            if (sRef)
            {
                auto serv = fc.GetService(sRef);
                return serv->at(type);
            }
            else
            {
                return nullptr;
            }
        };

        EXPECT_CALL(*(mockContext.get()), GetBundleContext()).WillRepeatedly(testing::Invoke([&fc]() { return fc; }));

        EXPECT_CALL(*(mockContext.get()), LocateService("foo", us_service_interface_iid<ServiceDependency1>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateService))); // ensure the mock context received a call to LocateService for dependency foo

        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure the dependencies are bound when the object is constructed
        ASSERT_TRUE(compObj->GetFoo());

        f.Stop();
        f.WaitForStop(std::chrono::milliseconds::zero());
    }

    /**
     * This test point is used to verify the service component implementation class
     * receives lifecycle callbacks if it implements the lifecycle hook methods.
     * i.e, the Activate and Deactivate methods with correct signature.
     */
    TEST(ComponentInstance, VerifyLifeCycleHooks)
    {
        ComponentInstanceImpl<TestServiceImpl1> compInstance;
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_FALSE(iMap);
        auto compObj = compInstance.GetInstance();
        ASSERT_FALSE(compObj);
        auto mockContext = std::make_shared<MockComponentContext>();
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        compObj = compInstance.GetInstance();
        ASSERT_FALSE(compObj->IsActivated());
        compInstance.Activate();
        ASSERT_TRUE(compObj->IsActivated());
        compInstance.Deactivate();
        ASSERT_FALSE(compObj->IsActivated());
    }

    TEST(ComponentInstanceImpl, VerifyLifeCycleHooks)
    {
        ComponentInstanceImpl<TestServiceImpl1> compInstance;
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_FALSE(iMap);
        auto compObj = compInstance.GetInstance();
        ASSERT_FALSE(compObj);
        auto mockContext = std::make_shared<MockComponentContext>();
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        compObj = compInstance.GetInstance();
        ASSERT_FALSE(compObj->IsActivated());
        compInstance.Activate();
        ASSERT_TRUE(compObj->IsActivated());
        compInstance.Deactivate();
        ASSERT_FALSE(compObj->IsActivated());
    }

    TEST(ComponentInstanceImpl, VerifyWithStaticDependenciesMultipleCardinality)
    {
        auto f = cppmicroservices::FrameworkFactory().NewFramework();
        f.Start();
        auto fc = f.GetBundleContext();
        auto reg = fc.RegisterService<ServiceDependency1>(std::make_shared<ServiceDependency1>());
        auto reg1 = fc.RegisterService<ServiceDependency2>(std::make_shared<ServiceDependency2>());
        auto reg2 = fc.RegisterService<ServiceDependency3>(std::make_shared<ServiceDependency3>());

        ComponentInstanceImpl<TestServiceImpl2,
                              std::tuple<>,
                              std::vector<std::shared_ptr<ServiceDependency1>>,
                              std::shared_ptr<ServiceDependency2>,
                              std::vector<std::shared_ptr<ServiceDependency3>>>
            compInstance({ ("dep1"), ("dep2"), ("dep3") }, {});
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_FALSE(iMap);

        auto mockContext = std::make_shared<MockComponentContext>();
        auto locateService = [&fc](std::string const& type) -> std::shared_ptr<void>
        {
            auto sRef = fc.GetServiceReference(type);
            if (sRef)
            {
                auto serv = fc.GetService(sRef);
                return serv->at(type);
            }
            else
            {
                return nullptr;
            }
        };

        auto locateServices = [&fc](std::string const& type) -> std::vector<std::shared_ptr<void>>
        {
            auto sRef = fc.GetServiceReference(type);
            if (sRef)
            {
                auto serv = fc.GetService(sRef);
                return { serv->at(type) };
            }
            else
            {
                return {};
            }
        };

        EXPECT_CALL(*(mockContext.get()), GetBundleContext()).WillRepeatedly(testing::Invoke([&fc]() { return fc; }));

        EXPECT_CALL(*(mockContext.get()), LocateServices("dep1", us_service_interface_iid<ServiceDependency1>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateServices))); // ensure the mock context received a call to LocateService for dependency dep1

        EXPECT_CALL(*(mockContext.get()), LocateService("dep2", us_service_interface_iid<ServiceDependency2>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateService))); // ensure the mock context received a call to LocateService for dependency dep2

        EXPECT_CALL(*(mockContext.get()), LocateServices("dep3", us_service_interface_iid<ServiceDependency3>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateServices))); // ensure the mock context received a call to LocateService for dependency dep3

        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure the dependencies are bound when the object is constructed
        ASSERT_TRUE(compObj->GetDep1().size() > 0);
        ASSERT_TRUE(compObj->GetDep2());
        ASSERT_TRUE(compObj->GetDep3().size() > 0);

        EXPECT_THROW(compInstance.InvokeBindMethod("dep1", fc.GetServiceReference<ServiceDependency1>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeBindMethod("dep2", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeBindMethod("dep3", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeUnbindMethod("dep1", fc.GetServiceReference<ServiceDependency1>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeUnbindMethod("dep2", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeUnbindMethod("dep3", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);

        f.Stop();
        f.WaitForStop(std::chrono::milliseconds::zero());
    }

    TEST(ComponentInstanceImpl, VerifyWithDynamicDependenciesMultipleCardinality)
    {
        auto f = cppmicroservices::FrameworkFactory().NewFramework();
        f.Start();
        auto fc = f.GetBundleContext();
        auto reg = fc.RegisterService<ServiceDependency1>(std::make_shared<ServiceDependency1>());
        auto reg1 = fc.RegisterService<ServiceDependency2>(std::make_shared<ServiceDependency2>());
        auto reg2 = fc.RegisterService<ServiceDependency3>(std::make_shared<ServiceDependency3>());

        std::vector<std::shared_ptr<Binder<TestServiceImpl2>>> binders;
        binders.push_back(
            std::make_shared<DynamicBinder<TestServiceImpl2, ServiceDependency3>>("dep3",
                                                                                  &TestServiceImpl2::BindDep3,
                                                                                  &TestServiceImpl2::UnbindDep3));

        ComponentInstanceImpl<TestServiceImpl2,
                              std::tuple<>,
                              std::vector<std::shared_ptr<ServiceDependency1>>,
                              std::shared_ptr<ServiceDependency2>>
            compInstance({ ("dep1"), ("dep2") }, binders);
        auto iMap = compInstance.GetInterfaceMap();
        ASSERT_FALSE(iMap);
        ASSERT_EQ(compInstance.GetInstance(), nullptr);

        // create a mock context and setup expectations on the context
        auto mockContext = std::make_shared<MockComponentContext>();
        auto locateService = [&fc](std::string const& type) -> std::shared_ptr<void>
        {
            auto sRef = fc.GetServiceReference(type);
            if (sRef)
            {
                auto serv = fc.GetService(sRef);
                return serv->at(type);
            }
            else
            {
                return nullptr;
            }
        };

        auto locateServices = [&fc](std::string const& type) -> std::vector<std::shared_ptr<void>>
        {
            auto sRef = fc.GetServiceReference(type);
            if (sRef)
            {
                auto serv = fc.GetService(sRef);
                return { serv->at(type) };
            }
            else
            {
                return {};
            }
        };

        EXPECT_CALL(*(mockContext.get()), GetBundleContext()).WillRepeatedly(testing::Invoke([&fc]() { return fc; }));

        EXPECT_CALL(*(mockContext.get()), LocateServices("dep1", us_service_interface_iid<ServiceDependency1>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateServices))); // ensure the mock context received a call to LocateServices for dependency dep1

        EXPECT_CALL(*(mockContext.get()), LocateService("dep2", us_service_interface_iid<ServiceDependency2>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateService))); // ensure the mock context received a call to LocateService for dependency dep2

        EXPECT_CALL(*(mockContext.get()), LocateService("dep3", us_service_interface_iid<ServiceDependency3>()))
            .Times(1)
            .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
                locateService))); // ensure the mock context received a call to LocateServices for dependency dep3

        // use mock context to create an instance of the component implementation class.
        compInstance.CreateInstance(mockContext);
        compInstance.BindReferences(mockContext);
        auto compObj = compInstance.GetInstance();
        ASSERT_TRUE(compObj);
        // ensure the dependencies are bound when the object is constructed
        ASSERT_NE(compObj->GetDep1().size(), 0);
        ASSERT_NE(compObj->GetDep2(), nullptr);
        ASSERT_NE(compObj->GetDep3().size(), 0);

        // ensure only dynamic dependencies can be re-bound.
        // The runtime calls the wrapper object with the name of the reference and the ServiceReference object to use
        // for binding.
        EXPECT_THROW(compInstance.InvokeUnbindMethod("dep1", fc.GetServiceReference<ServiceDependency1>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeBindMethod("dep1", fc.GetServiceReference<ServiceDependency1>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeUnbindMethod("dep2", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);
        EXPECT_THROW(compInstance.InvokeBindMethod("dep2", fc.GetServiceReference<ServiceDependency2>()),
                     std::out_of_range);
        EXPECT_NO_THROW(compInstance.InvokeUnbindMethod("dep3", fc.GetServiceReference<ServiceDependency3>()));
        ASSERT_EQ(compObj->GetDep3().size(), 0);
        EXPECT_NO_THROW(compInstance.InvokeBindMethod("dep3", fc.GetServiceReference<ServiceDependency3>()));
        ASSERT_EQ(compObj->GetDep3().size(), 1);

        f.Stop();
        f.WaitForStop(std::chrono::milliseconds::zero());
    }
} // namespace
