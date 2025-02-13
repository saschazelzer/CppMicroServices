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

#include "cppmicroservices/AnyMap.h"
#include "cppmicroservices/GlobalConfig.h"

#include "gtest/gtest.h"

using namespace cppmicroservices;

TEST(AnyMapTest, CheckExceptions)
{
    // Testing throw of invalid_argument from the free function
    // AtCompoundKey(const AnyMap& m, const AnyMap::key_type& key)
    AnyMap uo(AnyMap::UNORDERED_MAP);
    uo["hi"] = std::string("hi");
    EXPECT_THROW(uo.AtCompoundKey("hi."), std::invalid_argument);
}

TEST(AnyMapTest, AtCompoundKey)
{
    // Testing nested vector<Any> compound access
    std::vector<Any> child { Any(1), Any(2) };
    AnyMap uo { AnyMap::UNORDERED_MAP, { { "hi", std::vector<Any> { Any { std::vector<Any> { 1, 2 } } } } } };
    ASSERT_EQ(uo.AtCompoundKey("hi.0.0"), 1);
}

TEST(AnyMapTest, IteratorTest)
{
    AnyMap o = {
        any_map::ORDERED_MAP,
        {{ "a", 1 }, { "b", 2 }, { "c", 3 }}
    };
    AnyMap::const_iter ociter(o.begin());
    AnyMap::const_iter ociter1(o.cbegin());

    AnyMap uo = {
        AnyMap::UNORDERED_MAP,
        {{ "1", 1 }, { "2", 2 }}
    };
    AnyMap::const_iter uociter(uo.begin());
    AnyMap::const_iter uociter1(uo.cbegin());

    AnyMap uoci {
        {{ "do", 1 }, { "re", 2 }}
    };
    AnyMap::const_iter uoccciiter(uoci.begin());
    AnyMap::const_iter uoccciiter1(uoci.cbegin());

    AnyMap::const_iter ociter_temp(AnyMap(AnyMap::ORDERED_MAP).cbegin());

    // Testing deref and increment operators
    ASSERT_EQ((*ociter1).second.ToString(), std::string("1"));
    ASSERT_EQ((*(++ociter1)).second.ToString(), std::string("2"));
    ASSERT_EQ((*(ociter1++)).second.ToString(), std::string("2"));
    int i = 0;
    for (AnyMap::const_iter oc_it = o.cbegin(); oc_it != o.cend(); ++oc_it)
    {
        ++i;
        ASSERT_EQ(i, any_cast<int>(oc_it->second));
    }

    // Testing exception when an invalid iterator is dereferenced.
    AnyMap::const_iter nciter, nciter2;
    EXPECT_THROW(*nciter, std::logic_error);
    EXPECT_THROW(++nciter, std::logic_error);
    EXPECT_THROW(nciter++, std::logic_error);
    EXPECT_THROW(US_UNUSED(nciter->second), std::logic_error);
    ASSERT_EQ(nciter, nciter2);

    // Testing ++ operator
    ASSERT_TRUE(any_cast<int>((*(uociter++)).second) > 0);
    ASSERT_TRUE(any_cast<int>(uociter->second) > 0);
    ASSERT_TRUE(any_cast<int>((*(uoccciiter++)).second) > 0);

    // Testing operator==
    auto ociter2(o.cbegin());
    ASSERT_EQ(ociter, ociter2);

    // Testing iterator copy ctor.
    AnyMap::iter oiter(o.begin());
    AnyMap::iter oiter_copy(o.begin());
    AnyMap::iter uoiter(uo.begin());
    AnyMap::iter uoiter_copy(uoiter);
    AnyMap::iter uociiter(uoci.begin());
    AnyMap::iter niter;
    AnyMap::iter niter_copy(niter);
    AnyMap::const_iter nciter3(niter);

    // Testing iterator equality operator
    ASSERT_EQ(oiter, oiter_copy);
    ASSERT_EQ(uoiter, uoiter_copy);
    ASSERT_EQ(niter, niter_copy);

    // Testing iterator deref operator
    ASSERT_EQ((*oiter).second.ToString(), std::string("1"));
    ASSERT_TRUE(any_cast<int>((*uoiter).second) > 0);
    EXPECT_THROW(*niter, std::logic_error);

    // Testing iterator arrow operator
    ASSERT_TRUE(any_cast<int>(uoiter->second) > 0);
    ASSERT_TRUE(any_cast<int>(uociiter->second) > 0);
    EXPECT_THROW(US_UNUSED(niter->second), std::logic_error);

    // Testing iterator pre-increment operator
    ASSERT_EQ((*(++oiter)).second.ToString(), std::string("2"));
    ASSERT_TRUE(any_cast<int>((*(++uoiter)).second) > 0);
    EXPECT_THROW(++niter, std::logic_error);

    // Testing iterator post-increment operator
    ASSERT_EQ((*(oiter++)).second.ToString(), std::string("2"));
    ASSERT_TRUE(any_cast<int>((*(uoiter++)).second) > 0);
    ASSERT_TRUE(any_cast<int>((uociiter++)->second) > 0);
    EXPECT_THROW(niter++, std::logic_error);
}

TEST(AnyMapTest, AnyMap)
{
    AnyMap::ordered_any_map o {
        {{ "do", 1 }, { "re", 2 }}
    };
    AnyMap o_anymap(o);
    AnyMap o_anymap_copy(o_anymap);

    AnyMap uo_anymap = {
        AnyMap::UNORDERED_MAP,
        {{ "do", 1 }, { "re", 2 }}
    };

    AnyMap::unordered_any_cimap uco;
    AnyMap uco_anymap(uco);

    AnyMap o_anymap1(AnyMap::ORDERED_MAP);
    o_anymap1 = o_anymap_copy;
    AnyMap uo_anymap1(AnyMap::UNORDERED_MAP);
    uo_anymap1 = uo_anymap;
    AnyMap uco_anymap1(AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
    uco_anymap1 = uco_anymap;

    AnyMap o1(AnyMap::ORDERED_MAP);
    o1["key"] = 42;
    AnyMap uo1(AnyMap::UNORDERED_MAP);
    uo1 = o1;
    ASSERT_EQ(any_cast<int>(uo1.at("key")), 42);

    // Testing AnyMap::empty()
    ASSERT_TRUE(uco_anymap1.empty());

    // Testing AnyMap::clear()
    AnyMap o_anymap2(o_anymap);
    o_anymap2.clear();
    uco_anymap1.clear();
    uco_anymap1["DO"] = 1;
    uco_anymap1["RE"] = 2;

    // Testing AnyMap::at()
    ASSERT_EQ(any_cast<int>(uo_anymap1.at("re")), 2);

    // Testing AnyMap::operator[] (const)
    std::string const key = "re";
    o_anymap1[key] = 10;
    uo_anymap1[key] = 10;
    uo_anymap1.insert(std::make_pair(std::string("mi"), Any(3)));
    uco_anymap1[key] = 10;

    // Testing AnyMap::find()
    ASSERT_TRUE(o_anymap1.find("re") != o_anymap1.end());
    ASSERT_TRUE(uo_anymap1.find("re") != uo_anymap1.end());

    // Testing AnyMap::GetType()
    ASSERT_EQ(uco_anymap1.GetType(), AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);

    // Testing any_value_to_* free functions
    std::ostringstream stream1, stream2;
    any_value_to_string(stream1, o_anymap);
    ASSERT_EQ(stream1.str(), "{do : 1, re : 2}");
    any_value_to_json(stream2, o_anymap1);
    ASSERT_EQ(stream2.str(), "{\"do\" : 1, \"re\" : 10}");
}

TEST(AnyMapTest, MoveConstructor)
{
    testing::FLAGS_gtest_death_test_style = "threadsafe";
    AnyMap o = {
        AnyMap::ORDERED_MAP,
        {{ "do", 1 }, { "re", 2 }}
    };

    AnyMap o_anymap_move_ctor(std::move(o));
    ASSERT_EQ(any_cast<int>(o_anymap_move_ctor.at("do")), 1);
    ASSERT_DEATH({ o.size(); }, ".*") << "This call should result in a crash because "
                                         "the object has been moved from";

    AnyMap uo(AnyMap::UNORDERED_MAP);
    AnyMap uo_move(std::move(uo));
    ASSERT_EQ(uo_move.size(), 0u) << "Size of an empty moved-to AnyMap must be 0";

    AnyMap uoci(AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
    AnyMap uoci_move(std::move(uoci));
    ASSERT_EQ(uoci_move.size(), 0u) << "Size of an empty moved-to AnyMap must be 0";
}

TEST(AnyMapTest, MoveAssignment)
{
    testing::FLAGS_gtest_death_test_style = "threadsafe";
    AnyMap o {
        AnyMap::ORDERED_MAP,
        {{ "do", 1 }, { "re", 2 }}
    };

    AnyMap uo_anymap_move_assign(AnyMap::UNORDERED_MAP);
    uo_anymap_move_assign = std::move(o);
    ASSERT_EQ(any_cast<int>(uo_anymap_move_assign.at("re")), 2);
    ASSERT_DEATH(o.size(), ".*") << "This call should result in a crash because "
                                    "the object has been moved from";
}

void AnyMapTest_CopyAssignment_Helper(any_map::map_type t1, any_map::map_type t2) {
    AnyMap m1 = {
        t1,
        {{ "do", 1}, {"re", 2 }}
    };
    any_map::const_iter m1i = m1.begin();

    AnyMap m2 = {
        t2,
        {{ "fi", 3}, {"la", 4}}
    };
    any_map::const_iter m2i = m2.begin();

    ASSERT_NE(m1, m2) << "Different maps should not be equal";
    ASSERT_NE(m1i, m2i) << "Different maps should have different iterators";
    m1 = m2;
    ASSERT_EQ(m1, m2) << "Copy assignment should result in equal maps";
    ASSERT_EQ(m1.GetType(), t2) << "Assignment should update map type";

    m2.erase("fi");
    AnyMap m3 = {
        t2,
        {{ "fi", 3}, {"la", 4}}
    };
    ASSERT_EQ(m1, m3) << "Copied map should not reference its creator";

    any_map::const_iter m3i = m3.begin();
    ASSERT_NE(m1i, m3i) << "Copied map should have a different iterator";
    m3i = m1i;
    ASSERT_EQ(m1i, m3i) << "Copy-assigning an AnyMap iterator should make identical iterators";
}

TEST(AnyMapTest, CopyAssignment)
{
    AnyMapTest_CopyAssignment_Helper(AnyMap::ORDERED_MAP, AnyMap::UNORDERED_MAP);
    AnyMapTest_CopyAssignment_Helper(AnyMap::UNORDERED_MAP, AnyMap::ORDERED_MAP);
    AnyMapTest_CopyAssignment_Helper(AnyMap::ORDERED_MAP,
                                     AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
}

TEST(AnyMapTest, CIHash)
{
    std::string allUpper = "THIS IS A TEST";
    std::string allLower = "this is a test";

    any_map::unordered_any_cimap::hasher hash;
    std::size_t hashUpper = hash(allUpper);
    std::size_t hashLower = hash(allLower);

    ASSERT_EQ(hashUpper, hashLower);
    ASSERT_EQ(true, 0 != hashUpper);
    ASSERT_EQ(true, 0 != hashLower);
}

TEST(AnyMapTest, CIHashUnique)
{
    std::string v1 = "ABC";
    std::string v2 = "CAB";

    any_map::unordered_any_cimap::hasher hash;
    std::size_t hashV1 = hash(v1);
    std::size_t hashV2 = hash(v2);

    ASSERT_EQ(true, hashV1 != hashV2);
}

TEST(AnyMapTest, GeneralUsage)
{
    ASSERT_THROW(AnyMap m(static_cast<AnyMap::map_type>(100)), std::logic_error);

    AnyMap om(AnyMap::ORDERED_MAP);
    ASSERT_EQ(0, om.size());
    ASSERT_TRUE(om.empty());
    ASSERT_EQ(0, om.count("key1"));

    auto it = om.insert(std::make_pair(std::string("key1"), Any(std::string("val1"))));
    ASSERT_TRUE(it.second);
    ASSERT_EQ("key1", it.first->first);
    ASSERT_EQ(std::string("val1"), any_cast<std::string>(it.first->second));
    ASSERT_EQ(std::string("val1"), any_cast<std::string>(om["key1"]));

    /* Create a AnyMap with the following JSON representation:
     *
     * {
     *   key1 : "val1",
     *   uoci : {
     *     FiRST : 1,
     *     SECOND : 2,
     *     vec : [
     *       "one",
     *       2,
     *       {
     *         hi : "hi",
     *         there : "there"
     *       }
     *     ]
     *   },
     *   dot.key : 5
     * }
     *
     */
    AnyMap uoci(AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
    uoci["FiRST"] = 1;
    uoci["SECOND"] = 2;

    AnyMap uo(AnyMap::UNORDERED_MAP);
    uo["hi"] = std::string("hi");
    uo["there"] = std::string("there");

    std::vector<Any> anyVec { Any(std::string("one")), Any(2), Any(uo) };
    uoci["vec"] = anyVec;

    om["uoci"] = uoci;
    om["dot.key"] = 5;

    ASSERT_EQ(std::string("val1"), any_cast<std::string>(om.AtCompoundKey("key1")));
    ASSERT_EQ(1, any_cast<int>(om.AtCompoundKey("uoci.first")));
    ASSERT_EQ(2, any_cast<int>(om.AtCompoundKey("uoci.second")));
    ASSERT_EQ(std::string("one"), any_cast<std::string>(om.AtCompoundKey("uoci.Vec.0")));
    ASSERT_EQ(std::string("there"), any_cast<std::string>(om.AtCompoundKey("uoci.Vec.2.there")));

    Any emptyAny;
    ASSERT_EQ(std::string("val1"), any_cast<std::string>(om.AtCompoundKey("key1", emptyAny)));
    ASSERT_EQ(1, any_cast<int>(om.AtCompoundKey("uoci.first", emptyAny)));
    ASSERT_EQ(2, any_cast<int>(om.AtCompoundKey("uoci.second", emptyAny)));
    ASSERT_EQ(std::string("one"), any_cast<std::string>(om.AtCompoundKey("uoci.Vec.0")));
    ASSERT_EQ(std::string("there"), any_cast<std::string>(om.AtCompoundKey("uoci.Vec.2.there")));

    std::set<std::string> keys;
    for (auto p : uoci)
    {
        keys.insert(p.first);
    }

    auto key = keys.begin();
    ASSERT_EQ(3, keys.size());
    ASSERT_EQ("FiRST", *key++);
    ASSERT_EQ("SECOND", *key++);
    ASSERT_EQ("vec", *key++);

    ASSERT_THROW(om.at("Key1"), std::out_of_range);
    ASSERT_THROW(om.AtCompoundKey("dot.key"), std::out_of_range);
    ASSERT_THROW(uoci.AtCompoundKey("Vec.bla"), std::invalid_argument);
    ASSERT_THROW(uoci.AtCompoundKey("Vec.1.bla"), std::invalid_argument);

    ASSERT_NO_THROW(om.AtCompoundKey("dot.key", emptyAny));
    ASSERT_NO_THROW({
        auto val = uoci.AtCompoundKey("Vec.bla", emptyAny);
        ASSERT_TRUE(val.Empty());
    });
    ASSERT_NO_THROW({
        auto val1 = uoci.AtCompoundKey("Vec.1.bla", Any(std::string("")));
        ASSERT_TRUE(!val1.Empty());
        ASSERT_EQ(std::string(""), any_cast<std::string>(val1));
    });
}

cppmicroservices::AnyMap
manifest_from_cache(cppmicroservices::any_map::key_type const& key, cppmicroservices::any_map& cache)
{
    using namespace cppmicroservices;

    AnyMap& bundles = ref_any_cast<AnyMap>(cache["bundles"]);
    if (bundles.find(key) != std::end(bundles))
    {
        try
        {
            AnyMap result = std::move(ref_any_cast<AnyMap>(bundles[key]));
            bundles.erase(key);
            return result;
        }
        catch (...)
        {
        }
    }
    return AnyMap(AnyMap::UNORDERED_MAP_CASEINSENSITIVE_KEYS);
}

TEST(AnyMapTest, ManifestFromCache)
{
    AnyMap cache {};
    AnyMap cache_bundles {};
    AnyMap cache_bundle1 = {
        {{ "a", std::string("A") }, { "b", std::string("B") }, { "c", std::string("C") }}
    };

    AnyMap cache_bundle2 = {
        {{ "d", std::string("D") }, { "e", std::string("E") }, { "f", std::string("F") }}
    };

    auto cache_bundle1_copy = cache_bundle1;
    auto cache_bundle2_copy = cache_bundle2;

    cache_bundles.emplace(std::string("bundle1"), std::move(cache_bundle1));
    cache_bundles.emplace(std::string("bundle2"), std::move(cache_bundle2));
    cache["created"] = 1234567890;
    cache["version"] = 1;
    cache.emplace(std::string("bundles"), std::move(cache_bundles));
    EXPECT_EQ(3, cache.size()); // created, version, bundles

    auto const& bundles = ref_any_cast<AnyMap>(cache.at("bundles"));
    EXPECT_EQ(2, bundles.size()); // bundle1, bundle2

    auto bundle1 = manifest_from_cache(std::string("bundle1"), cache);
    EXPECT_EQ(cache_bundle1_copy, bundle1);
    EXPECT_EQ(1, bundles.size());

    auto bundle2 = manifest_from_cache(std::string("bundle2"), cache);
    EXPECT_EQ(cache_bundle2_copy, bundle2);
    EXPECT_EQ(0, bundles.size());
}

TEST(AnyMapTest, InitializerList)
{
    AnyMap noType {
        {{ "a", 1 }, { "b", 2 }}
    };
    EXPECT_EQ(any_map::UNORDERED_MAP_CASEINSENSITIVE_KEYS, noType.GetType());
    EXPECT_EQ(2, noType.size());
    EXPECT_EQ(Any(1), noType.at("a"));
    EXPECT_EQ(Any(2), noType.at("b"));

    AnyMap ordered_any_map {
        any_map::ORDERED_MAP,
        {{ "c", 3 }, { "d", 4 }}
    };
    EXPECT_EQ(any_map::ORDERED_MAP, ordered_any_map.GetType());
    EXPECT_EQ(2, ordered_any_map.size());
    EXPECT_EQ(Any(3), ordered_any_map.at("c"));
    EXPECT_EQ(Any(4), ordered_any_map.at("d"));

    AnyMap unordered_any_map {
        any_map::UNORDERED_MAP,
        {{ "e", 5 }, { "f", 6 }}
    };
    EXPECT_EQ(any_map::UNORDERED_MAP, unordered_any_map.GetType());
    EXPECT_EQ(2, unordered_any_map.size());
    EXPECT_EQ(Any(5), unordered_any_map.at("e"));
    EXPECT_EQ(Any(6), unordered_any_map.at("f"));

    AnyMap unordered_any_cimap {
        any_map::UNORDERED_MAP_CASEINSENSITIVE_KEYS,
        {{ "g", 7 }, { "h", 8 }}
    };
    EXPECT_EQ(any_map::UNORDERED_MAP_CASEINSENSITIVE_KEYS, unordered_any_cimap.GetType());
    EXPECT_EQ(2, unordered_any_cimap.size());
    EXPECT_EQ(Any(7), unordered_any_cimap.at("g"));
    EXPECT_EQ(Any(8), unordered_any_cimap.at("h"));
    AnyMap nested {
        {{ "n", 9 }, { "o", 10 }, { "p", std::string("C") }}
    };
    AnyMap unordered_any_nested {
        {
         { "j", 7 },
         { "k", 8 },
         { "l", std::string("B") },
         { "m", AnyMap { { { "n", 9 }, { "o", 10 }, { "p", std::string("C") } } } },
         }
    };
    EXPECT_EQ(any_map::UNORDERED_MAP_CASEINSENSITIVE_KEYS, unordered_any_nested.GetType());
    EXPECT_EQ(4, unordered_any_nested.size());
    EXPECT_EQ(Any(7), unordered_any_nested.at("j"));
    EXPECT_EQ(Any(8), unordered_any_nested.at("k"));
    EXPECT_EQ(std::string("B"), any_cast<std::string>(unordered_any_nested.at("l")));
    EXPECT_EQ(AnyMap(nested), any_cast<AnyMap>(unordered_any_nested.at("m")));
    EXPECT_EQ(Any(9), any_cast<AnyMap>(unordered_any_nested.at("m")).at("n"));
    EXPECT_EQ(Any(10), any_cast<AnyMap>(unordered_any_nested.at("m")).at("o"));
    EXPECT_EQ(std::string("C"), any_cast<std::string>(any_cast<AnyMap>(unordered_any_nested.at("m")).at("p")));
}

TEST(AnyMapTest, AnyMapToCPP)
{
    AnyMap::ordered_any_map o {
        {{ "do", 1 }, { "re", 2 }}
    };
    AnyMap o_anymap(o);

    // Testing any_value_to_* free functions
    std::ostringstream stream1, stream2, stream3, stream4, stream5;
    any_value_to_cpp(stream1, o_anymap);
    ASSERT_EQ(stream1.str(), R"(AnyMap { ORDERED_MAP, {{"do" , 1}, {"re" , 2}}})");

    AnyMap::ordered_any_map o1 {
        {{ "do", std::string("try") }, { "strings", std::string("and") }, { "bools", true }, { "or", false }}
    };
    AnyMap o1_anymap(o1);

    // Testing any_value_to_* free functions
    any_value_to_cpp(stream2, o1_anymap);
    ASSERT_EQ(
        stream2.str(),
        R"(AnyMap { ORDERED_MAP, {{"bools" , true}, {"do" , std::string("try")}, {"or" , false}, {"strings" , std::string("and")}}})");

    AnyMap::ordered_any_map o2 {
        {{ "j", 7 },
         { "k", 8 },
         { "l", std::string("B") },
         { "m", AnyMap::ordered_any_map { { { "n", 9 }, { "o", 10 }, { "p", std::string("C") } } } }}
    };
    AnyMap o2_anymap(o2);

    any_value_to_cpp(stream3, o2_anymap);
    ASSERT_EQ(
        stream3.str(),
        R"(AnyMap { ORDERED_MAP, {{"j" , 7}, {"k" , 8}, {"l" , std::string("B")}, {"m" , AnyMap { ORDERED_MAP, {{"n" , 9}, {"o" , 10}, {"p" , std::string("C")}}}}}})");

    any_value_to_cpp(stream4, o2_anymap, 2, 0);
    ASSERT_EQ(stream4.str(), R"(AnyMap { ORDERED_MAP, {
  {"j" , 7}, 
  {"k" , 8}, 
  {"l" , std::string("B")}, 
  {"m" , AnyMap { ORDERED_MAP, {
    {"n" , 9}, 
    {"o" , 10}, 
    {"p" , std::string("C")}
  }}}
}})");

    AnyMap::ordered_any_map o3 {
        {{ "mi", 7.9 },
         { "ca", 6.7776 },
         { "m", std::vector<cppmicroservices::Any> { { std::string("yes") }, { 5 } } }}
    };
    AnyMap o3_anymap(o3);

    // Testing any_value_to_* free functions
    any_value_to_cpp(stream5, o3_anymap, 2, 0);
    ASSERT_EQ(stream5.str(), R"(AnyMap { ORDERED_MAP, {
  {"ca" , 6.7776}, 
  {"m" , AnyVector {{
    std::string("yes"),
    5
  }}}, 
  {"mi" , 7.9}
}})");
}

TEST(AnyMapTest, AnyMapToCPPKitchenSink)
{
    std::ostringstream stream1;

    AnyMap::ordered_any_map o {
        {{ "j", 7 },
         { "k", 8 },
         { "l", std::string("B") },
         { "m",
         AnyMap::ordered_any_map {
         { { "n", 9 },
         { "o", 10 },
         { "p",
         std::vector<cppmicroservices::Any> {
         { std::string("yes") },
         { 5 },
         { AnyMap::ordered_any_map {
         { { "key1", 1 },
         { "key2", false },
         { "key3",
         std::vector<cppmicroservices::Any> { { false },
         { 6.7889 },
         { std::string("me!") } } } } } } } } } } }}
    };
    AnyMap uo_anymap(o);
    any_value_to_cpp(stream1, uo_anymap, 2, 0);
    ASSERT_EQ(stream1.str(), R"(AnyMap { ORDERED_MAP, {
  {"j" , 7}, 
  {"k" , 8}, 
  {"l" , std::string("B")}, 
  {"m" , AnyMap { ORDERED_MAP, {
    {"n" , 9}, 
    {"o" , 10}, 
    {"p" , AnyVector {{
      std::string("yes"),
      5,
      AnyMap { ORDERED_MAP, {
        {"key1" , 1}, 
        {"key2" , false}, 
        {"key3" , AnyVector {{
          false,
          6.7889,
          std::string("me!")
        }}}
      }}
    }}}
  }}}
}})");
}
