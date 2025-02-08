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
#include "../../src/metadata/MetadataParserFactory.hpp"
#include "../../src/metadata/MetadataParserImpl.hpp"
#include "../../src/metadata/ServiceMetadata.hpp"
#include "Mocks.hpp"
#include "gtest/gtest.h"
#include <cppmicroservices/FrameworkEvent.h>
#include <cppmicroservices/FrameworkFactory.h>

using cppmicroservices::AnyMap;
using cppmicroservices::scrimpl::FakeLogger;
using cppmicroservices::scrimpl::metadata::MetadataParserFactory;
using cppmicroservices::scrimpl::metadata::MetadataParserImplV1;
using cppmicroservices::scrimpl::metadata::ServiceMetadata;

namespace
{
    // Classes derive from this to provide input test cases
    struct TestInputs
    {
        AnyMap const&
        operator[](std::size_t i) const
        {
            return metadatas[i];
        }

        std::vector<AnyMap> metadatas;
    };

    // An instance of this class represents a test case.
    // For the inputs in ValidInputs corresponding to the index metadataIndex,
    // the members of the parsed metadata are expected to be the ones provided
    // in the constructor.
    struct ServiceMetadataParserValidState
    {
        ServiceMetadataParserValidState(std::size_t _metadataIndex,
                                        std::string _serviceScope,
                                        std::vector<std::string> _interfaces)
            : metadataIndex(_metadataIndex)
            , serviceScope(std::move(_serviceScope))
            , interfaces(std::move(_interfaces))
        {
        }

        std::size_t metadataIndex;
        std::string serviceScope;
        std::vector<std::string> interfaces;

        friend std::ostream&
        operator<<(std::ostream& os, ServiceMetadataParserValidState const& obj)
        {
            os << "Metadata Index: " << obj.metadataIndex << " service scope: " << obj.serviceScope
               << " interfaces: [ ";
            std::for_each(obj.interfaces.begin(),
                          obj.interfaces.end(),
                          [&os](std::string const& interface) { os << interface << " "; });
            return os << "]\n";
        }
    };

    class ValidServiceMetadataTest : public ::testing::TestWithParam<ServiceMetadataParserValidState>
    {
      public:
        std::shared_ptr<FakeLogger>
        GetLogger()
        {
            return logger;
        }

      protected:
        std::shared_ptr<FakeLogger> logger;
        virtual void
        SetUp()
        {
            logger = std::make_shared<FakeLogger>();
        }
    };

    // Valid service metadata inputs
    struct ValidInputs : public TestInputs
    {
        ValidInputs()
        {
            //  CheckWithInterfaceNoScope
            std::vector<cppmicroservices::Any> interfaces { cppmicroservices::Any(std::string("Interface1")),
                                                            cppmicroservices::Any(std::string("Interface2")) };
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {"interfaces", cppmicroservices::Any(interfaces)}
            })));

            // CheckWithInterfaceAndScope_SINGLETON
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {"interfaces",               cppmicroservices::Any(interfaces)},
                {     "scope", cppmicroservices::Any(std::string("singleton"))}
            })));

            // CheckWithInterfaceAndScope_PROTOTYPE
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {"interfaces",               cppmicroservices::Any(interfaces)},
                {     "scope", cppmicroservices::Any(std::string("prototype"))}
            })));

            // CheckWithInterfaceAndScope_BUNDLE
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {"interfaces",            cppmicroservices::Any(interfaces)},
                {     "scope", cppmicroservices::Any(std::string("bundle"))}
            })));
        }
    };

    TEST_P(ValidServiceMetadataTest, TestServiceMetadataSuccessModes)
    {
        ServiceMetadataParserValidState smvs = GetParam();
        auto inputs = ValidInputs();
        std::size_t i = smvs.metadataIndex;
        MetadataParserImplV1 metadataparser(GetLogger());
        auto prop = metadataparser.CreateServiceMetadata(inputs[i]);
        ASSERT_EQ(prop.scope, smvs.serviceScope);
        ASSERT_THAT(prop.interfaces, ::testing::ContainerEq(smvs.interfaces));
    }

    INSTANTIATE_TEST_SUITE_P(
        SuccessModes,
        ValidServiceMetadataTest,
        testing::Values(ServiceMetadataParserValidState(0, "singleton", { "Interface1", "Interface2" }),
                        ServiceMetadataParserValidState(1, "singleton", { "Interface1", "Interface2" }),
                        ServiceMetadataParserValidState(2, "prototype", { "Interface1", "Interface2" }),
                        ServiceMetadataParserValidState(3, "bundle", { "Interface1", "Interface2" })));

    // For the metadata in InvalidInputs corresponding to metadataIndex,
    // we expect the exception message output by the Metadata Parser to be
    // exactly errorOutput. Instead, if we expect the errorOutput to be
    // contained in the generated error message, we set isPartial = true. (This
    // mode is useful when we don't want to specify really long error messages)
    struct ServiceMetadataParserInvalidState
    {
        ServiceMetadataParserInvalidState(std::size_t _metadataIndex, std::string _errorOutput, bool _isPartial = false)
            : metadataIndex(_metadataIndex)
            , errorOutput(std::move(_errorOutput))
            , isPartial(_isPartial)
        {
        }

        std::size_t metadataIndex;
        std::string errorOutput;
        bool isPartial;

        friend std::ostream&
        operator<<(std::ostream& os, ServiceMetadataParserInvalidState const& obj)
        {
            return os << "";
            return os << "Metadata Index: " << obj.metadataIndex << " error output: " << obj.errorOutput
                      << "  Perform partial match: " << (obj.isPartial ? "Yes" : "No") << "\n";
        }
    };

    class InvalidServiceMetadataTest : public ::testing::TestWithParam<ServiceMetadataParserInvalidState>
    {
      public:
        std::shared_ptr<FakeLogger>
        GetLogger()
        {
            return logger;
        }

      protected:
        std::shared_ptr<FakeLogger> logger;
        virtual void
        SetUp()
        {
            logger = std::make_shared<FakeLogger>();
        }
    };

    struct InvalidInputs : public TestInputs
    {
        InvalidInputs()
        {
            // ConstructorWithNoInterface
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {std::string("scope"), cppmicroservices::Any(std::string("prototype"))}
            })));

            // ConstructorWithInterfaceAndInvalidScope
            std::vector<cppmicroservices::Any> interfaces { cppmicroservices::Any(std::string("Interface1")),
                                                            cppmicroservices::Any(std::string("Interface2")) };
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {std::string("interfaces"),            cppmicroservices::Any(interfaces)},
                {     std::string("scope"), cppmicroservices::Any(std::string("foobar"))}
            })));

            // ConstructorWithInterfaceAndIllegalScope
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {std::string("interfaces"), cppmicroservices::Any(interfaces)},
                {     std::string("scope"),         cppmicroservices::Any(42)}
            })));

            // ConstructorWithIllegalInterface
            interfaces = { cppmicroservices::Any(true) };
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {std::string("interfaces"), cppmicroservices::Any(interfaces)}
            })));
        }
    };

    TEST_P(InvalidServiceMetadataTest, TestServiceMetadataFailureModes)
    {
        ServiceMetadataParserInvalidState smis = GetParam();
        auto inputs = InvalidInputs();
        std::size_t i = smis.metadataIndex;
        try
        {
            MetadataParserImplV1 metadataparser(GetLogger());
            auto sMetadata = metadataparser.CreateServiceMetadata(inputs[i]);
            FAIL() << "This failure suggests that parsing has succeeded. "
                      "Shouldn't happen for failure mode tests";
        }
        catch (std::exception const& err)
        {
            std::string exceptionMsg { err.what() };
            if (!smis.isPartial)
            {
                ASSERT_THAT(exceptionMsg, ::testing::StrEq(smis.errorOutput));
            }
            else
            {
                ASSERT_THAT(exceptionMsg, ::testing::HasSubstr(smis.errorOutput));
            }
        }
    }

    INSTANTIATE_TEST_SUITE_P(
        FailureModes,
        InvalidServiceMetadataTest,
        testing::Values(ServiceMetadataParserInvalidState(0, "Missing key 'interfaces' in the manifest."),
                        ServiceMetadataParserInvalidState(1,
                                                          "Invalid value 'foobar'. The valid choices are : [bundle, "
                                                          "prototype, singleton]."),
                        ServiceMetadataParserInvalidState(2,
                                                          "Unexpected type for the name 'scope'. Exception: "
                                                          "cppmicroservices::BadAnyCastException",
                                                          /*isPartial=*/true),
                        ServiceMetadataParserInvalidState(3,
                                                          "Exception: cppmicroservices::BadAnyCastException:",
                                                          /*isPartial=*/true)));
} // namespace
