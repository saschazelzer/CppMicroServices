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

#ifndef CPPMICROSERVICES_ANYMAP_H
#define CPPMICROSERVICES_ANYMAP_H

#include "cppmicroservices/Any.h"
#include <initializer_list>
#include <string>
#include <unordered_map>

namespace cppmicroservices
{
    class Properties;
    class LDAPExpr;

    namespace detail
    {

        struct US_Framework_EXPORT any_map_cihash
        {
            std::size_t operator()(std::string const& key) const;
        };

        struct US_Framework_EXPORT any_map_ciequal
        {
            bool operator()(std::string const& l, std::string const& r) const;
        };

    } // namespace detail

    /**
     * \ingroup MicroServicesUtils
     *
     * A map data structure which wraps different STL map types.
     *
     * This is a convenience class providing a STL associative container
     * interface for different underlying container types. Supported underlying
     * types are
     * - \c any_map::ordered_any_map (a STL map)
     * - \c any_map::unordered_any_map (a STL unordered map)
     * - \c any_map::unordered_any_cimap (a STL unordered map with case insensitive key comparison)
     *
     * This class provides most of the STL functions for associated containers,
     * including forward iterators. It is typically not instantiated by clients
     * directly, but obtained via framework API calls, returning an \c AnyMap
     * sub-class instance.
     *
     * @see AnyMap
     */
    class US_Framework_EXPORT any_map
    {

      public:
        using key_type = std::string;
        using mapped_type = Any;
        using value_type = std::pair<key_type const, mapped_type>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = value_type const&;
        using pointer = value_type*;
        using const_pointer = value_type const*;
        using ordered_any_map = std::map<std::string, Any>;
        using unordered_any_map = std::unordered_map<std::string, Any>;
        using unordered_any_cimap
            = std::unordered_map<std::string, Any, detail::any_map_cihash, detail::any_map_ciequal>;
        enum map_type : uint8_t
        {
            ORDERED_MAP,
            UNORDERED_MAP,
            UNORDERED_MAP_CASEINSENSITIVE_KEYS
        };

      private:
        class US_Framework_EXPORT iterator_base
        {
            friend class any_map;

          protected:
            enum iter_type : uint8_t
            {
                NONE,
                ORDERED,
                UNORDERED,
                UNORDERED_CI
            };

            iter_type type { NONE };

            iterator_base()

                = default;

            iterator_base(iter_type type) : type(type) {}

          public:
            using value_type = any_map::value_type;

            using iterator_category = std::forward_iterator_tag;
            using difference_type = any_map::difference_type;
        };

      public:
        class iter;

        class US_Framework_EXPORT const_iter : public iterator_base
        {
          private:
            using ociter = ordered_any_map::const_iterator;
            using uociter = unordered_any_map::const_iterator;
            using uocciiter = unordered_any_cimap::const_iterator;

          public:
            using reference = any_map::const_reference;
            using pointer = any_map::const_pointer;

            using iterator = const_iter;

            const_iter();
            const_iter(iterator const& it);
            const_iter(iter const& it);
            ~const_iter();

            const_iter(ociter&& it);
            const_iter(uociter&& it, iter_type type);

            iterator& operator=(iterator const& x);

            reference operator*() const;
            pointer operator->() const;

            iterator& operator++();
            iterator operator++(int);

            bool operator==(iterator const& x) const;
            bool operator!=(iterator const& x) const;

          private:
            ociter const& o_it() const;
            ociter& o_it();
            uociter const& uo_it() const;
            uociter& uo_it();
            uocciiter const& uoci_it() const;
            uocciiter& uoci_it();

            union
            {
                ociter* o;
                uociter* uo;
                uocciiter* uoci;
            } it;
        };

        class US_Framework_EXPORT iter : public iterator_base
        {
          private:
            using oiter = ordered_any_map::iterator;
            using uoiter = unordered_any_map::iterator;
            using uociiter = unordered_any_cimap::iterator;

          public:
            using reference = any_map::reference;
            using pointer = any_map::pointer;

            using iterator = iter;

            iter();
            iter(iter const& it);
            ~iter();

            iter(oiter&& it);
            iter(uoiter&& it, iter_type type);

            reference operator*() const;
            pointer operator->() const;

            iterator& operator++();
            iterator operator++(int);

            bool operator==(iterator const& x) const;
            bool operator!=(iterator const& x) const;

          private:
            friend class const_iter;

            oiter const& o_it() const;
            oiter& o_it();
            uoiter const& uo_it() const;
            uoiter& uo_it();
            uociiter const& uoci_it() const;
            uociiter& uoci_it();

            union
            {
                oiter* o;
                uoiter* uo;
                uociiter* uoci;
            } it;
        };

        using iterator = iter;
        using const_iterator = const_iter;

        /**
         * @brief initializer_list constructor
         *
         * Construct an AnyMap of type "type" with the content of the
         * initialized with the content of the the initializer list. Allows for inline
         * initialization akin to:
         *
         *     any_map cache_bundle1 {
         *         any_map::ORDERED_MAP, {
         *             {"a", std::string("A")},
         *             {"b", std::string("B")},
         *             {"c", std::string("C")}
         *         }
         *     };
         *
         *  The primary purpose of this constructor in any_map is in support of the initializer_list
         *  constructors in the AnyMap subclass.
         *
         * @param type the map_type of the AnyMap
         * @param l a std::initializer_list<value_type> used to initialize the content of the AnyMap.
         */
        any_map(map_type type, std::initializer_list<value_type> l = {});
        any_map(ordered_any_map const& m);
        any_map(ordered_any_map&& m);
        any_map(unordered_any_map const& m);
        any_map(unordered_any_map&& m);
        any_map(unordered_any_cimap const& m);
        any_map(unordered_any_cimap&& m);

        any_map(any_map const& m);
        any_map& operator=(any_map const& m);

        any_map(any_map&& m) noexcept;
        any_map& operator=(any_map&& m) noexcept;

        ~any_map();

        iter begin();
        const_iter begin() const;
        const_iter cbegin() const;
        iter end();
        const_iter end() const;
        const_iter cend() const;

        bool empty() const;
        size_type size() const;
        size_type count(key_type const& key) const;
        void clear();

        mapped_type& at(key_type const& key);
        mapped_type const& at(key_type const& key) const;

        mapped_type& operator[](key_type const& key);
        mapped_type& operator[](key_type&& key);

        std::pair<iterator, bool> insert(value_type const& value);

        template <class... Args>
        std::pair<iterator, bool>
        emplace(Args&&... args)
        {
            switch (type)
            {
                case map_type::ORDERED_MAP:
                {
                    return o_m().emplace(std::forward<Args>(args)...);
                }
                case map_type::UNORDERED_MAP:
                {
                    auto p = uo_m().emplace(std::forward<Args>(args)...);
                    return { iterator(std::move(p.first), iterator::UNORDERED), p.second };
                }
                case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                {
                    auto p = uoci_m().emplace(std::forward<Args>(args)...);
                    return { iterator(std::move(p.first), iterator::UNORDERED_CI), p.second };
                }
                default:
                    throw std::logic_error("invalid map type");
            }
        }

        /**
         * return the iterator to the value referenced by key
         */
        const_iterator find(key_type const& key) const;

        /**
         * Erase entry for value for 'key'
         * @param key the key for the entry to Erase
         * @return the number of elements erased.
         */
        size_type erase(key_type const& key);

        /**
         * Compare the content of this map with those of rhs
         * @param rhs an any_map to compare
         * @return bool true rhs contains the same content as this
         */
        bool operator==(any_map const& rhs) const;
        bool
        operator!=(any_map const& rhs) const
        {
            return !(operator==(rhs));
        }

      protected:
        map_type type;

      private:
        friend class Properties;
        friend class LDAPExpr;

        // Private "fast" and type-checked functions for working with map iterators
        // and finding elements (these functions should only be called in a context)
        // where the type of the map is guaranteed.
        //
        // These functions bypass the creation of "any_map::iterator" and "any_map::const_iterator"
        // objects as construction of those are slow. Once the AnyMap class is refactored, these
        // functions will likely be unnecessary as the begin(), end(), and find() functions will
        // inherently do what these functions do.
        ordered_any_map::const_iterator beginOM_TypeChecked() const;
        ordered_any_map::const_iterator endOM_TypeChecked() const;
        ordered_any_map::const_iterator findOM_TypeChecked(key_type const& key) const;
        unordered_any_map::const_iterator beginUO_TypeChecked() const;
        unordered_any_map::const_iterator endUO_TypeChecked() const;
        unordered_any_map::const_iterator findUO_TypeChecked(key_type const& key) const;
        unordered_any_cimap::const_iterator beginUOCI_TypeChecked() const;
        unordered_any_cimap::const_iterator endUOCI_TypeChecked() const;
        unordered_any_cimap::const_iterator findUOCI_TypeChecked(key_type const& key) const;
        // =========================================================================

        ordered_any_map const& o_m() const;
        ordered_any_map& o_m();
        unordered_any_map const& uo_m() const;
        unordered_any_map& uo_m();
        unordered_any_cimap const& uoci_m() const;
        unordered_any_cimap& uoci_m();

        inline void copy_from(any_map const& m);
        inline void move_from(any_map&& m) noexcept;
        inline void destroy() noexcept;

        union
        {
            ordered_any_map* o;
            unordered_any_map* uo;
            unordered_any_cimap* uoci;
        } map;
    };

    /**
     * \ingroup MicroServicesUtils
     *
     * A map data structure with support for compound keys.
     *
     * This class adds convenience functions on top of the \c any_map
     * class. The \c any_map is a recursive data structure, and its values can be
     * retrieved via standard map functions or by using a dotted key notation
     * specifying a compound key.
     *
     * @see any_map
     */
    class US_Framework_EXPORT AnyMap : public any_map
    {
      public:
        /**
         * @brief initializer_list constructor
         *
         * Construct an AnyMap of type UNORDERED_MAP_CASEINSENSITIVE_KEYS with the content of the
         * the initializer list. Allows for inline initialization akin to:
         *
         *     AnyMap cache_bundle1 {
         *         {"a", std::string("A")},
         *         {"b", std::string("B")},
         *         {"c", std::string("C")}
         *     };
         * @param l a std::initializer_list<value_type> used to initialize the content of the AnyMap.
         *
         */
        AnyMap(std::initializer_list<any_map::value_type> l = {});
        /**
         * @brief initializer_list constructor
         *
         * Construct an AnyMap of type "type" with the content of the the initializer list. Allows
         * for inline initialization akin to:
         *
         *     AnyMap cache_bundle1 {
         *         AnyMap::ORDERED_MAP, {
         *             {"a", std::string("A")},
         *             {"b", std::string("B")},
         *             {"c", std::string("C")}
         *         }
         *     };
         *
         * @param type the map_type of the AnyMap
         * @param l a std::initializer_list<value_type> used to initialize the content of the AnyMap.
         *
         */
        AnyMap(map_type type, std::initializer_list<any_map::value_type> l = {});
        AnyMap(ordered_any_map const& m);
        AnyMap(ordered_any_map&& m);
        AnyMap(unordered_any_map const& m);
        AnyMap(unordered_any_map&& m);
        AnyMap(unordered_any_cimap const& m);
        AnyMap(unordered_any_cimap&& m);

        /**
         * Get the underlying STL container type.
         *
         * @return The STL container type holding the map data.
         */
        map_type GetType() const;

        /**
         * Get a key's value, using a compound key notation.
         *
         * A compound key consists of one or more key names, concatenated with
         * the '.' (dot) character. Each key except the last requires the referenced
         * Any object to be of type \c AnyMap or \c std::vector<Any>. Containers
         * of type \c std::vector<Any> are indexed using 0-based numerical key names.
         *
         * For example, a \c AnyMap object holding data of the following layout
         * \code{.json}
         * {
         *   one: 1,
         *   two: "two",
         *   three: {
         *     a: "anton",
         *     b: [ 3, 8 ]
         *   }
         * }
         * \endcode
         * can be queried using the following notation:
         * \code
         * map.AtCompoundKey("one");       // returns Any(1)
         * map.AtCompoundKey("three.a");   // returns Any(std::string("anton"))
         * map.AtCompoundKey("three.b.1"); // returns Any(8)
         * \endcode
         *
         * @param key The key hierachy to query.
         * @return A reference to the key's value.
         *
         * @throws std::invalid_argument if the \c Any value for a given key is not of type \c AnyMap or \c
         * std::vector<Any>.
         * @throws std::out_of_range if the key is not found or a numerical index would fall out of the range of an \c
         * int type.
         */
        mapped_type const& AtCompoundKey(key_type const& key) const;

        /**
         * Return a key's value, using a compound key notation if the key is found in the map or
         * return the provided default value if the key is not found
         *
         * A compound key consists of one or more key names, concatenated with
         * the '.' (dot) character. Each key except the last requires the referenced
         * Any object to be of type \c AnyMap or \c std::vector<Any>. Containers
         * of type \c std::vector<Any> are indexed using 0-based numerical key names.
         *
         * For example, a \c AnyMap object holding data of the following layout
         * \code{.json}
         * {
         *   one: 1,
         *   two: "two",
         *   three: {
         *     a: "anton",
         *     b: [ 3, 8 ]
         *   }
         * }
         * \endcode
         * can be queried using the following notation:
         * \code
         * map.AtCompoundKey("one", Any());       // returns Any(1)
         * map.AtCompoundKey("four", Any());       // returns Any()
         * map.AtCompoundKey("three.a", Any());          // returns Any(std::string("anton"))
         * map.AtCompoundKey("three.c", Any());          // returns Any()
         * map.AtCompoundKey("three.b.1", Any());        // returns Any(8)
         * map.AtCompoundKey("three.b.4", Any());        // returns Any()
         * \endcode
         *
         * @param key The key hierachy to query.
         * @param defaultValue is the value to be returned if the key is not found
         * @return A copy of the key's value.
         */
        mapped_type AtCompoundKey(key_type const& key, mapped_type defaultValue) const noexcept;
    };

    template <>
    US_Framework_EXPORT std::ostream& any_value_to_string(std::ostream& os, AnyMap const& m);

    template <>
    US_Framework_EXPORT std::ostream& any_value_to_json(std::ostream& os,
                                                        AnyMap const& m,
                                                        uint8_t const increment,
                                                        int32_t const indent);

    template <>
    US_Framework_EXPORT std::ostream& any_value_to_cpp(std::ostream& os,
                                                       AnyMap const& m,
                                                       uint8_t const increment,
                                                       int32_t const indent);

} // namespace cppmicroservices

#endif // CPPMICROSERVICES_ANYMAP_H
