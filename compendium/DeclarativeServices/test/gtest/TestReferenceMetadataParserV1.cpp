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
#include "../../src/metadata/ReferenceMetadata.hpp"
#include "Mocks.hpp"
#include "gtest/gtest.h"
#include <cppmicroservices/FrameworkEvent.h>
#include <cppmicroservices/FrameworkFactory.h>
#include <iostream>
#include <unordered_map>

using cppmicroservices::AnyMap;
using cppmicroservices::scrimpl::FakeLogger;
using cppmicroservices::scrimpl::metadata::MetadataParserFactory;
using cppmicroservices::scrimpl::metadata::MetadataParserImplV1;
using cppmicroservices::scrimpl::metadata::ReferenceMetadata;

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
    struct ReferenceMetadataParserValidState
    {
        ReferenceMetadataParserValidState(std::size_t _metadataIndex,
                                          std::string _interface,
                                          std::string _name,
                                          std::string _cardinality,
                                          std::string _policy,
                                          std::string _policyOption,
                                          std::string _scope,
                                          std::string _target,
                                          std::size_t _minCardinality,
                                          std::size_t _maxCardinality)
            : metadataIndex(_metadataIndex)
            , interface(std::move(_interface))
            , name(std::move(_name))
            , cardinality(std::move(_cardinality))
            , policy(std::move(_policy))
            , policyOption(std::move(_policyOption))
            , scope(std::move(_scope))
            , target(std::move(_target))
            , minCardinality(_minCardinality)
            , maxCardinality(_maxCardinality)
        {
        }

        std::size_t metadataIndex;
        std::string interface;
        std::string name;
        std::string cardinality;
        std::string policy;
        std::string policyOption;
        std::string scope;
        std::string target;
        std::size_t minCardinality;
        std::size_t maxCardinality;

        friend std::ostream&
        operator<<(std::ostream& os, ReferenceMetadataParserValidState const& obj)
        {
            return os << "Metadata Index: " << obj.metadataIndex
                      << " Interface: " << obj.interface << " name: " << obj.name << " cardinality: " << obj.cardinality
                      << " minimum cardinality: " << obj.minCardinality
                      << " maximum cardinality: " << obj.maxCardinality << " policy: " << obj.policy
                      << " policy Option: " << obj.policyOption << " scope: " << obj.scope << " target: " << obj.target
                      << "\n";
        }
    };

    class ValidReferenceMetadataTest : public ::testing::TestWithParam<ReferenceMetadataParserValidState>
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

    // Valid reference metadata inputs
    struct ValidInputs : public TestInputs
    {
        ValidInputs()
        {
            // ConstructWithCustomName
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {"interface", std::string("com.bar.foo")},
                {     "name",         std::string("bar")}
            })));

            // ConstructWithCardinality_OPTIONALUNARY
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {       "name", std::string("com.bar.foo")},
                {  "interface", std::string("com.bar.foo")},
                {"cardinality",        std::string("0..1")}
            })));

            // ConstructWithCardinality_MANDATORYUNARY
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {       "name", std::string("com.bar.foo")},
                {  "interface", std::string("com.bar.foo")},
                {"cardinality",        std::string("1..1")}
            })));

            // ConstructWithCardinality_OPTIONALMULTIPLE
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {       "name", std::string("com.bar.foo")},
                {  "interface", std::string("com.bar.foo")},
                {"cardinality",        std::string("0..n")}
            })));

            // ConstructWithCardinality_MANDATORYMULTIPLE
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {       "name", std::string("com.bar.foo")},
                {  "interface", std::string("com.bar.foo")},
                {"cardinality",        std::string("1..n")}
            })));

            // ConstructWithPolicy_STATIC
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {     "name", std::string("com.bar.foo")},
                {"interface", std::string("com.bar.foo")},
                {   "policy",      std::string("static")}
            })));

            // ConstructWithPolicy_DYNAMIC
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {     "name", std::string("com.bar.foo")},
                {"interface", std::string("com.bar.foo")},
                {   "policy",     std::string("dynamic")}
            })));

            // ConstructWithPolicyOption_RELUCTANT
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {         "name", std::string("com.bar.foo")},
                {    "interface", std::string("com.bar.foo")},
                {"policy-option",   std::string("reluctant")}
            })));

            // ConstructWithPolicyOption_GREEDY
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {         "name", std::string("com.bar.foo")},
                {    "interface", std::string("com.bar.foo")},
                {"policy-option",      std::string("greedy")}
            })));

            // ConstructWithValid_Target
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {    "interface", std::string("com.bar.foo")},
                {         "name",         std::string("foo")},
                {  "cardinality",        std::string("0..n")},
                {       "policy",     std::string("dynamic")},
                {"policy-option",      std::string("greedy")},
                {       "target",        std::string("LDAP")}
            })));
        }
    };

    TEST_P(ValidReferenceMetadataTest, TestReferenceMetadataSuccessModes)
    {
        ReferenceMetadataParserValidState rmvs = GetParam();
        auto inputs = ValidInputs();
        std::size_t i = rmvs.metadataIndex;
        MetadataParserImplV1 metadataparser(GetLogger());
        auto prop = metadataparser.CreateReferenceMetadata(inputs[i]);

        ASSERT_EQ(prop.interfaceName, rmvs.interface);
        ASSERT_EQ(prop.name, rmvs.name);
        ASSERT_EQ(prop.cardinality, rmvs.cardinality);
        ASSERT_EQ(prop.minCardinality, rmvs.minCardinality);
        ASSERT_EQ(prop.maxCardinality, rmvs.maxCardinality);
        ASSERT_EQ(prop.policy, rmvs.policy);
        ASSERT_EQ(prop.policyOption, rmvs.policyOption);
        ASSERT_EQ(prop.scope, rmvs.scope);
        ASSERT_EQ(prop.target, rmvs.target);
    }

    INSTANTIATE_TEST_SUITE_P(
        SuccessModes,
        ValidReferenceMetadataTest,
        testing::Values(ReferenceMetadataParserValidState(0,
                                                          "com.bar.foo",
                                                          "bar",
                                                          "1..1",
                                                          "static",
                                                          "reluctant",
                                                          "bundle",
                                                          "",
                                                          1,
                                                          1),
                        ReferenceMetadataParserValidState(1,
                                                          "com.bar.foo",
                                                          "com.bar.foo",
                                                          "0..1",
                                                          "static",
                                                          "reluctant",
                                                          "bundle",
                                                          "",
                                                          0,
                                                          1),
                        ReferenceMetadataParserValidState(2,
                                                          "com.bar.foo",
                                                          "com.bar.foo",
                                                          "1..1",
                                                          "static",
                                                          "reluctant",
                                                          "bundle",
                                                          "",
                                                          1,
                                                          1),
                        ReferenceMetadataParserValidState(3,
                                                          "com.bar.foo",
                                                          "com.bar.foo",
                                                          "0..n",
                                                          "static",
                                                          "reluctant",
                                                          "bundle",
                                                          "",
                                                          0,
                                                          std::numeric_limits<std::size_t>::max()),
                        ReferenceMetadataParserValidState(4,
                                                          "com.bar.foo",
                                                          "com.bar.foo",
                                                          "1..n",
                                                          "static",
                                                          "reluctant",
                                                          "bundle",
                                                          "",
                                                          1,
                                                          std::numeric_limits<std::size_t>::max()),
                        ReferenceMetadataParserValidState(5,
                                                          "com.bar.foo",
                                                          "com.bar.foo",
                                                          "1..1",
                                                          "static",
                                                          "reluctant",
                                                          "bundle",
                                                          "",
                                                          1,
                                                          1),
                        ReferenceMetadataParserValidState(6,
                                                          "com.bar.foo",
                                                          "com.bar.foo",
                                                          "1..1",
                                                          "dynamic",
                                                          "reluctant",
                                                          "bundle",
                                                          "",
                                                          1,
                                                          1),
                        ReferenceMetadataParserValidState(7,
                                                          "com.bar.foo",
                                                          "com.bar.foo",
                                                          "1..1",
                                                          "static",
                                                          "reluctant",
                                                          "bundle",
                                                          "",
                                                          1,
                                                          1),
                        ReferenceMetadataParserValidState(8,
                                                          "com.bar.foo",
                                                          "com.bar.foo",
                                                          "1..1",
                                                          "static",
                                                          "greedy",
                                                          "bundle",
                                                          "",
                                                          1,
                                                          1),
                        ReferenceMetadataParserValidState(9,
                                                          "com.bar.foo",
                                                          "foo",
                                                          "0..n",
                                                          "dynamic",
                                                          "greedy",
                                                          "bundle",
                                                          "LDAP",
                                                          0,
                                                          std::numeric_limits<std::size_t>::max())));

    // For the metadata in InvalidInputs corresponding to metadataIndex,
    // we expect the exception message output by the Metadata Parser to be
    // exactly errorOutput. Instead, if we expect the errorOutput to be
    // contained in the generated error message, we set isPartial = true. (This
    // mode is useful when we don't want to specify really long error messages)
    struct ReferenceMetadataParserInvalidState
    {
        ReferenceMetadataParserInvalidState(std::size_t _metadataIndex,
                                            std::string _errorOutput,
                                            bool _isPartial = false)
            : metadataIndex(_metadataIndex)
            , errorOutput(std::move(_errorOutput))
            , isPartial(_isPartial)
        {
        }

        std::size_t metadataIndex;
        std::string errorOutput;
        bool isPartial;

        friend std::ostream&
        operator<<(std::ostream& os, ReferenceMetadataParserInvalidState const& obj)
        {
            return os << "Metadata Index: " << obj.metadataIndex << " error output: " << obj.errorOutput
                      << "  Perform partial match: " << (obj.isPartial ? "Yes" : "No") << "\n";
        }
    };

    class InvalidReferenceMetadataTest : public ::testing::TestWithParam<ReferenceMetadataParserInvalidState>
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
            // ConstructWithNoInterface
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {         "name",       std::string("foo")},
                {  "cardinality",      std::string("0..1")},
                {       "policy",    std::string("static")},
                {"policy-option", std::string("reluctant")},
                {        "scope",    std::string("bundle")}
            })));

            // ConstructWithNoName
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {"interface", std::string("com.bar.foo")}
            })));

            // ConstructWithCardinality_Invalid
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {       "name", std::string("com.bar.foo")},
                {  "interface", std::string("com.bar.foo")},
                {"cardinality",         std::string("foo")}
            })));

            // ConstructWithPolicy_Invalid
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {     "name", std::string("com.bar.foo")},
                {"interface", std::string("com.bar.foo")},
                {   "policy",         std::string("foo")}
            })));

            // ConstructWithPolicyOption_Invalid
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {         "name", std::string("com.bar.foo")},
                {    "interface", std::string("com.bar.foo")},
                {"policy-option",         std::string("foo")}
            })));

            // ConstructWithCardinality_Illegal
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {       "name", std::string("com.bar.foo")},
                {  "interface", std::string("com.bar.foo")},
                {"cardinality",                         42}
            })));

            // ConstructWithEmptyInterfaceName
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {"interface", std::string("")},
                {     "name", std::string("")}
            })));

            // ConstructWithNameBadType
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {"interface", std::string("foo.bar")},
                {     "name",                   true}
            })));

            // ConstructWithEmptyTarget
            metadatas.push_back(AnyMap(std::unordered_map<std::string, cppmicroservices::Any>({
                {     "name", std::string("com.bar.foo")},
                {"interface", std::string("com.bar.foo")},
                {   "target",            std::string("")}
            })));
        }
    };

    TEST_P(InvalidReferenceMetadataTest, TestReferenceMetadataFailureModes)
    {
        ReferenceMetadataParserInvalidState rmis = GetParam();
        auto inputs = InvalidInputs();
        std::size_t i = rmis.metadataIndex;
        try
        {
            MetadataParserImplV1 metadataparser(GetLogger());
            auto sMetadata = metadataparser.CreateReferenceMetadata(inputs[i]);
            FAIL() << "This failure suggests that parsing has succeeded. "
                      "Shouldn't happen for failure mode tests";
        }
        catch (std::exception const& err)
        {
            std::string exceptionStr { err.what() };
            if (!rmis.isPartial)
            {
                ASSERT_THAT(exceptionStr, ::testing::StrEq(rmis.errorOutput));
            }
            else
            {
                ASSERT_THAT(exceptionStr, ::testing::HasSubstr(rmis.errorOutput));
            }
        }
    }

    INSTANTIATE_TEST_SUITE_P(
        FailureModes,
        InvalidReferenceMetadataTest,
        testing::Values(
            ReferenceMetadataParserInvalidState(0, "Missing key 'interface' in the manifest."),
            ReferenceMetadataParserInvalidState(1, "Missing key 'name' in the manifest."),
            ReferenceMetadataParserInvalidState(
                2,
                "Invalid value 'foo'. The valid choices are : [0..1, 1..1, 0..n, 1..n]."),
            ReferenceMetadataParserInvalidState(3, "Invalid value 'foo'. The valid choices are : [static, dynamic]."),
            ReferenceMetadataParserInvalidState(4, "Invalid value 'foo'. The valid choices are : [greedy, reluctant]."),
            ReferenceMetadataParserInvalidState(5,
                                                "Unexpected type for the name 'cardinality'. Exception: "
                                                "cppmicroservices::BadAnyCastException",
                                                /*isPartial=*/true),
            ReferenceMetadataParserInvalidState(6, "Value for the name 'interface' cannot be empty."),
            ReferenceMetadataParserInvalidState(7,
                                                "Unexpected type for the name 'name'. Exception: "
                                                "cppmicroservices::BadAnyCastException:",
                                                /*isPartial=*/true),
            ReferenceMetadataParserInvalidState(8, "Value for the name 'target' cannot be empty.")));
} // namespace
