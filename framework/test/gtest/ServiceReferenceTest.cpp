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

#include "cppmicroservices/BundleContext.h"
#include "cppmicroservices/Constants.h"
#include "cppmicroservices/Framework.h"
#include "cppmicroservices/FrameworkEvent.h"
#include "cppmicroservices/FrameworkFactory.h"
#include "cppmicroservices/ServiceObjects.h"
#include "cppmicroservices/ServiceRegistration.h"
#include "gtest/gtest.h"
#include <array>
#include <unordered_set>

using namespace cppmicroservices;

namespace ServiceNS
{

    struct ITestServiceA
    {
        virtual int getValue() const = 0;
        virtual ~ITestServiceA() {}
    };

    struct ITestServiceB
    {
        virtual int getValue() const = 0;
        virtual ~ITestServiceB() {}
    };
} // namespace ServiceNS

namespace
{
    struct TestServiceA : public ServiceNS::ITestServiceA
    {
        int
        getValue() const
        {
            return 42;
        }
    };

    struct TestServiceB : public ServiceNS::ITestServiceB
    {
        int
        getValue() const
        {
            return 1729;
        }
    };
} // namespace

class ServiceReferenceTest : public ::testing::Test
{
  public:
    ServiceReferenceTest() : framework(FrameworkFactory().NewFramework()) {};
    ~ServiceReferenceTest() override = default;
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

// This test exercises the 2 ways to register a service
//   a. using the name of the interface i.e. "Foo::Bar"
//   b. using the type of the interface i.e. <Foo::Bar>
// multiplied with 2 ways to get the service reference
//   a. using the name of the interface
//   b. using the type of the interface
// NOTE: We do have tests for this in different places,
// but this test consolidates all these modes together.
TEST_F(ServiceReferenceTest, TestRegisterAndGetServiceReferenceTest)
{
    auto context = framework.GetBundleContext();

    auto impl = std::make_shared<TestServiceA>();
    (void)context.RegisterService<ServiceNS::ITestServiceA>(impl);

    auto sr1 = context.GetServiceReference<ServiceNS::ITestServiceA>();
    ASSERT_EQ(sr1.GetInterfaceId(), "ServiceNS::ITestServiceA");
    auto service1 = context.GetService(sr1);
    ASSERT_EQ(service1->getValue(), 42);

    auto sr2 = context.GetServiceReference("ServiceNS::ITestServiceA");
    ASSERT_EQ(sr2.GetInterfaceId(), "ServiceNS::ITestServiceA");
    auto interfacemap2 = context.GetService(sr2);
    auto service_void2 = interfacemap2->at("ServiceNS::ITestServiceA");
    auto service2 = std::static_pointer_cast<ServiceNS::ITestServiceA>(service_void2);
    ASSERT_EQ(service2->getValue(), 42);

    InterfaceMap im;
    im["ServiceNS::ITestServiceB"] = std::make_shared<TestServiceB>();
    auto reg = context.RegisterService(std::make_shared<InterfaceMap const>(im));
    US_UNUSED(reg);

    auto sr3 = context.GetServiceReference("ServiceNS::ITestServiceB");
    ASSERT_EQ(sr3.GetInterfaceId(), "ServiceNS::ITestServiceB");
    auto interfacemap3 = context.GetService(sr3);
    auto service_void3 = interfacemap3->at("ServiceNS::ITestServiceB");
    auto service3 = std::static_pointer_cast<ServiceNS::ITestServiceB>(service_void3);
    ASSERT_EQ(service3->getValue(), 1729);

    auto sr4 = context.GetServiceReference<ServiceNS::ITestServiceB>();
    ASSERT_EQ(sr4.GetInterfaceId(), "ServiceNS::ITestServiceB");
    auto service4 = context.GetService(sr4);
    ASSERT_EQ(service4->getValue(), 1729);
}

TEST_F(ServiceReferenceTest, TestGetServiceReferenceWithMultipleRegistrations)
{
    auto context = framework.GetBundleContext();
    // register multiple service impls against the same interface
    // verify that GetServiceReference returns the first registered service
    std::vector<std::shared_ptr<TestServiceA>> implArr;
    for (int i = 0; i < 10; ++i)
    {
        auto impl = std::make_shared<TestServiceA>();
        implArr.push_back(impl);
        (void)context.RegisterService<ServiceNS::ITestServiceA>(impl);
    }

    auto sRefArr = context.GetServiceReferences<ServiceNS::ITestServiceA>();
    ASSERT_EQ(sRefArr.size(), implArr.size());

    // verify GetServiceReference returns the best match. In this case,
    // the service with the least id
    auto sRef = context.GetServiceReference<ServiceNS::ITestServiceA>();
    auto bestMatchServiceId = any_cast<long int>(sRef.GetProperty(Constants::SERVICE_ID));
    EXPECT_TRUE(std::none_of(sRefArr.begin(),
                             sRefArr.end(),
                             [&bestMatchServiceId](ServiceReferenceBase const& sRef)
                             {
                                 auto serviceId = any_cast<long int>(sRef.GetProperty(Constants::SERVICE_ID));
                                 return (serviceId < bestMatchServiceId);
                             }));
}

TEST_F(ServiceReferenceTest, TestGetServiceReferenceWithRanking)
{
    auto context = framework.GetBundleContext();
    // register multiple instance of services with varying ranks
    std::vector<std::shared_ptr<TestServiceA>> implArr;
    for (int i = 0; i < 10; ++i)
    {
        auto impl = std::make_shared<TestServiceA>();
        implArr.push_back(impl);
        (void)context.RegisterService<ServiceNS::ITestServiceA>(impl,
                                                                ServiceProperties({
                                                                    {Constants::SERVICE_RANKING, i}
        }));
    }

    auto sRefArr = context.GetServiceReferences<ServiceNS::ITestServiceA>();
    ASSERT_EQ(sRefArr.size(), implArr.size());

    // verify GetServiceReference returns the best match. In this case,
    // the service with the highest rank
    auto sRef = context.GetServiceReference<ServiceNS::ITestServiceA>();
    auto bestMatchServiceRank = any_cast<int>(sRef.GetProperty(Constants::SERVICE_RANKING));
    EXPECT_TRUE(std::none_of(sRefArr.begin(),
                             sRefArr.end(),
                             [&bestMatchServiceRank](ServiceReferenceBase const& sRef)
                             {
                                 auto serviceRank = any_cast<int>(sRef.GetProperty(Constants::SERVICE_RANKING));
                                 return (serviceRank > bestMatchServiceRank);
                             }));
}

TEST_F(ServiceReferenceTest, TestGetServiceReferenceWithMultipleRegistrationsPerRank)
{
    auto context = framework.GetBundleContext();
    // register multiple instance of services for each rank in a range of ranks
    std::vector<std::shared_ptr<TestServiceA>> implArr;
    for (int rank = 0; rank < 5; ++rank)
    {
        for (int serviceCount = 0; serviceCount < 5; ++serviceCount)
        {
            auto impl = std::make_shared<TestServiceA>();
            implArr.push_back(impl);
            (void)context.RegisterService<ServiceNS::ITestServiceA>(impl,
                                                                    ServiceProperties({
                                                                        {Constants::SERVICE_RANKING, rank}
            }));
        }
    }

    auto sRefArr = context.GetServiceReferences<ServiceNS::ITestServiceA>();
    ASSERT_EQ(sRefArr.size(), implArr.size());

    // verify GetServiceReference returns the best match. In this case,
    // the service with the higest rank and least id
    auto sRef = context.GetServiceReference<ServiceNS::ITestServiceA>();
    auto bestMatchServiceId = any_cast<long int>(sRef.GetProperty(Constants::SERVICE_ID));
    auto bestMatchServiceRank = any_cast<int>(sRef.GetProperty(Constants::SERVICE_RANKING));
    EXPECT_TRUE(std::none_of(sRefArr.begin(),
                             sRefArr.end(),
                             [&bestMatchServiceRank, &bestMatchServiceId](ServiceReferenceBase const& sRef)
                             {
                                 auto serviceRank = any_cast<int>(sRef.GetProperty(Constants::SERVICE_RANKING));
                                 auto serviceId = any_cast<long int>(sRef.GetProperty(Constants::SERVICE_ID));
                                 return (serviceRank > bestMatchServiceRank)
                                        || (serviceRank == bestMatchServiceRank && serviceId < bestMatchServiceId);
                             }));
}

TEST_F(ServiceReferenceTest, TestGetServiceReferenceWithModifiedProperties)
{
    auto context = framework.GetBundleContext();
    std::array<cppmicroservices::ServiceRegistration<ServiceNS::ITestServiceA>, 2> regArr;
    regArr[0] = context.RegisterService<ServiceNS::ITestServiceA>(std::make_shared<TestServiceA>());
    regArr[1] = context.RegisterService<ServiceNS::ITestServiceA>(std::make_shared<TestServiceA>());

    ASSERT_EQ(context.GetServiceReference<ServiceNS::ITestServiceA>(), regArr[0].GetReference());

    // modify the lower priority registration to bump up the priority
    regArr[1].SetProperties(ServiceProperties({
        {Constants::SERVICE_RANKING, 10}
    }));
    ASSERT_EQ(context.GetServiceReference<ServiceNS::ITestServiceA>(), regArr[1].GetReference());
}

TEST_F(ServiceReferenceTest, TestRegisterUnregisterCompareReference)
{
    auto context = framework.GetBundleContext();
    std::unordered_set<cppmicroservices::ServiceReference<ServiceNS::ITestServiceA>> set;
    cppmicroservices::ServiceReference<ServiceNS::ITestServiceA> sr1;
    cppmicroservices::ServiceReference<ServiceNS::ITestServiceA> sr2;

    auto impl = std::make_shared<TestServiceA>();
    {
        auto reg = context.RegisterService<ServiceNS::ITestServiceA>(impl);

        sr1 = context.GetServiceReference<ServiceNS::ITestServiceA>();
        sr2 = context.GetServiceReference<ServiceNS::ITestServiceA>();

        set.insert(sr1);
        ASSERT_NE(set.find(sr2), set.end());

        reg.Unregister();
    }

    ASSERT_NE(set.find(sr2), set.end());
}
