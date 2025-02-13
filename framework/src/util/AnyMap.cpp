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

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace cppmicroservices
{

    namespace detail
    {

        std::size_t
        any_map_cihash::operator()(std::string const& key) const
        {
            std::string lcase = key;
            std::transform(lcase.begin(), lcase.end(), lcase.begin(), ::tolower);
            return std::hash<std::string> {}(lcase);
        }

        bool
        any_map_ciequal::operator()(std::string const& l, std::string const& r) const
        {
            return (
                l.size() == r.size()
                && std::equal(l.begin(), l.end(), r.begin(), [](char a, char b) { return tolower(a) == tolower(b); }));
        }

        Any const& AtCompoundKey(std::vector<Any> const& v, std::string_view const& key);

        Any const&
        AtCompoundKey(AnyMap const& m, std::string_view const& key)
        {
            auto pos = key.find(".");
            if (pos != AnyMap::key_type::npos)
            {
                auto head = key.substr(0, pos);
                auto tail = key.substr(pos + 1);

                auto& h = m.at(std::string(head));
                if (h.Type() == typeid(AnyMap))
                {
                    return AtCompoundKey(ref_any_cast<AnyMap>(h), tail);
                }
                else if (h.Type() == typeid(std::vector<Any>))
                {
                    return AtCompoundKey(ref_any_cast<std::vector<Any>>(h), tail);
                }
                throw std::invalid_argument("Unsupported Any type at '" + std::string(head) + "' for dotted get");
            }
            else
            {
                return m.at(std::string(key));
            }
        }

        Any const&
        AtCompoundKey(std::vector<Any> const& v, std::string_view const& key)
        {
            auto pos = key.find(".");
            if (pos != AnyMap::key_type::npos)
            {
                auto head = key.substr(0, pos);
                auto tail = key.substr(pos + 1);

                int const index = std::stoi(std::string(head));
                auto& h = v.at(index < 0 ? v.size() + index : index);

                if (h.Type() == typeid(AnyMap))
                {
                    return AtCompoundKey(ref_any_cast<AnyMap>(h), tail);
                }
                else if (h.Type() == typeid(std::vector<Any>))
                {
                    return AtCompoundKey(ref_any_cast<std::vector<Any>>(h), tail);
                }
                throw std::invalid_argument("Unsupported Any type at '" + std::string(head) + "' for dotted get");
            }
            else
            {
                int const index = std::stoi(std::string(key));
                return v.at(index < 0 ? v.size() + index : index);
            }
        }
        Any AtCompoundKey(std::vector<Any> const& v, std::string_view const& key, Any&& defaultVal);

        Any
        AtCompoundKey(AnyMap const& m, std::string_view const& key, Any&& defaultVal)
        {
            auto pos = key.find(".");
            if (pos != AnyMap::key_type::npos)
            {
                auto const head = key.substr(0, pos);
                auto const tail = key.substr(pos + 1);
                auto itr = m.find(std::string(head));
                if (itr != m.end())
                {
                    auto& h = itr->second;
                    if (h.Type() == typeid(AnyMap))
                    {
                        return AtCompoundKey(ref_any_cast<AnyMap>(h), tail, std::move(defaultVal));
                    }
                    else if (h.Type() == typeid(std::vector<Any>))
                    {
                        return AtCompoundKey(ref_any_cast<std::vector<Any>>(h), tail, std::move(defaultVal));
                    }
                }
            }
            else
            {
                auto itr = m.find(std::string(key));
                if (itr != m.end())
                {
                    return itr->second;
                }
            }
            return std::move(defaultVal);
        }

        Any
        AtCompoundKey(std::vector<Any> const& v, std::string_view const& key, Any&& defaultval)
        {
            auto pos = key.find(".");
            auto const head = key.substr(0, pos);
            auto const tail = (pos == AnyMap::key_type::npos) ? "" : key.substr(pos + 1);

            int index = 0;
            try
            {
                index = std::stoi(std::string(head));
            }
            catch (...)
            {
                return std::move(defaultval);
            }

            if (static_cast<size_t>(std::abs(index)) < v.size())
            {
                auto& h = v[(index < 0 ? v.size() + index : index)];
                if (tail.empty())
                {
                    return h;
                }
                else if (h.Type() == typeid(AnyMap))
                {
                    return AtCompoundKey(ref_any_cast<AnyMap>(h), tail, std::move(defaultval));
                }
                else if (h.Type() == typeid(std::vector<Any>))
                {
                    return AtCompoundKey(ref_any_cast<std::vector<Any>>(h), tail, std::move(defaultval));
                }
            }
            return std::move(defaultval);
        }
    } // namespace detail

    // ----------------------------------------------------------------
    // ------------------  any_map::const_iterator  -------------------

    any_map::const_iter::const_iter() = default;

    any_map::const_iter::const_iter(any_map::const_iter const& it) : iterator_base(it.type), it { nullptr }
    {
        switch (type)
        {
            case ORDERED:
                this->it.o = new ociter(it.o_it());
                break;
            case UNORDERED:
                this->it.uo = new uociter(it.uo_it());
                break;
            case UNORDERED_CI:
                this->it.uoci = new uocciiter(it.uoci_it());
                break;
            case NONE:
                break;
            default:
                throw std::logic_error("invalid iterator type");
        }
    }

    any_map::const_iter::const_iter(any_map::iterator const& it) : iterator_base(it.type), it { nullptr }
    {
        switch (type)
        {
            case ORDERED:
                this->it.o = new ociter(it.o_it());
                break;
            case UNORDERED:
                this->it.uo = new uociter(it.uo_it());
                break;
            case UNORDERED_CI:
                this->it.uoci = new uocciiter(it.uoci_it());
                break;
            case NONE:
                break;
            default:
                throw std::logic_error("invalid iterator type");
        }
    }

    any_map::const_iter::~const_iter()
    {
        switch (type)
        {
            case ORDERED:
                delete it.o;
                break;
            case UNORDERED:
                delete it.uo;
                break;
            case UNORDERED_CI:
                delete it.uoci;
                break;
            case NONE:
                break;
        }
    }

    any_map::const_iter::const_iter(ociter&& it) : iterator_base(ORDERED) { this->it.o = new ociter(std::move(it)); }

    any_map::const_iter::const_iter(uociter&& it, iter_type type) : iterator_base(type)
    {
        switch (type)
        {
            case UNORDERED:
                this->it.uo = new uociter(std::move(it));
                break;
            case UNORDERED_CI:
                this->it.uoci = new uocciiter(std::move(it));
                break;
            default:
                throw std::logic_error("type for unordered_map iterator not supported");
        }
    }

    any_map::const_iter&
    any_map::const_iter::operator=(any_map::const_iter const& x)
    {
        switch (type)
        {
            case ORDERED:
                delete it.o;
                break;
            case UNORDERED:
                delete it.uo;
                break;
            case UNORDERED_CI:
                delete it.uoci;
                break;
            case NONE:
                break;
        }

        type = x.type;
        switch (type)
        {
            case ORDERED:
                this->it.o = new ociter(x.o_it());
                break;
            case UNORDERED:
                this->it.uo = new uociter(x.uo_it());
                break;
            case UNORDERED_CI:
                this->it.uoci = new uocciiter(x.uoci_it());
                break;
            case NONE:
                this->it = {nullptr};
                break;
            default:
                throw std::logic_error("invalid iterator type");
        }
        return *this;
    }

    any_map::const_iter::reference
    any_map::const_iter::operator*() const
    {
        switch (type)
        {
            case ORDERED:
                return *o_it();
            case UNORDERED:
                return *uo_it();
            case UNORDERED_CI:
                return *uoci_it();
            case NONE:
                throw std::logic_error("cannot dereference an invalid iterator");
            default:
                throw std::logic_error("invalid iterator type");
        }
    }

    any_map::const_iter::pointer
    any_map::const_iter::operator->() const
    {
        switch (type)
        {
            case ORDERED:
                return o_it().operator->();
            case UNORDERED:
                return uo_it().operator->();
            case UNORDERED_CI:
                return uoci_it().operator->();
            case NONE:
                throw std::logic_error("cannot dereference an invalid iterator");
            default:
                throw std::logic_error("invalid iterator type");
        }
    }

    any_map::const_iter::iterator&
    any_map::const_iter::operator++()
    {
        switch (type)
        {
            case ORDERED:
                ++o_it();
                break;
            case UNORDERED:
                ++uo_it();
                break;
            case UNORDERED_CI:
                ++uoci_it();
                break;
            case NONE:
                throw std::logic_error("cannot increment an invalid iterator");
            default:
                throw std::logic_error("invalid iterator type");
        }

        return *this;
    }

    any_map::const_iter::iterator
    any_map::const_iter::operator++(int)
    {
        iterator tmp = *this;
        switch (type)
        {
            case ORDERED:
                o_it()++;
                break;
            case UNORDERED:
                uo_it()++;
                break;
            case UNORDERED_CI:
                uoci_it()++;
                break;
            case NONE:
                throw std::logic_error("cannot increment an invalid iterator");
            default:
                throw std::logic_error("invalid iterator type");
        }
        return tmp;
    }

    bool
    any_map::const_iter::operator==(iterator const& x) const
    {
        if (type != x.type)
        {
            return false;
        }
        switch (type)
        {
            case ORDERED:
                return o_it() == x.o_it();
            case UNORDERED:
                return uo_it() == x.uo_it();
            case UNORDERED_CI:
                return uoci_it() == x.uoci_it();
            case NONE:
                return true;
            default:
                throw std::logic_error("invalid iterator type");
        }
    }

    bool
    any_map::const_iter::operator!=(iterator const& x) const
    {
        return !this->operator==(x);
    }

    any_map::const_iter::ociter const&
    any_map::const_iter::o_it() const
    {
        return *it.o;
    }

    any_map::const_iter::ociter&
    any_map::const_iter::o_it()
    {
        return *it.o;
    }

    any_map::const_iter::uociter const&
    any_map::const_iter::uo_it() const
    {
        return *it.uo;
    }

    any_map::const_iter::uociter&
    any_map::const_iter::uo_it()
    {
        return *it.uo;
    }

    any_map::const_iter::uocciiter const&
    any_map::const_iter::uoci_it() const
    {
        return *it.uoci;
    }

    any_map::const_iter::uocciiter&
    any_map::const_iter::uoci_it()
    {
        return *it.uoci;
    }

    // ----------------------------------------------------------------
    // ---------------------  AnyMap::iterator  -----------------------

    any_map::iter::iter() = default;

    any_map::iter::iter(iter const& it) : iterator_base(it.type), it { nullptr }
    {
        switch (type)
        {
            case ORDERED:
                this->it.o = new oiter(it.o_it());
                break;
            case UNORDERED:
                this->it.uo = new uoiter(it.uo_it());
                break;
            case UNORDERED_CI:
                this->it.uoci = new uociiter(it.uoci_it());
                break;
            case NONE:
                break;
            default:
                throw std::logic_error("invalid iterator type");
        }
    }

    any_map::iter::~iter()
    {
        switch (type)
        {
            case ORDERED:
                delete it.o;
                break;
            case UNORDERED:
                delete it.uo;
                break;
            case UNORDERED_CI:
                delete it.uoci;
                break;
            case NONE:
                break;
        }
    }

    any_map::iter::iter(oiter&& it) : iterator_base(ORDERED) { this->it.o = new oiter(std::move(it)); }

    any_map::iter::iter(uoiter&& it, iter_type type) : iterator_base(type)
    {
        switch (type)
        {
            case UNORDERED:
                this->it.uo = new uoiter(std::move(it));
                break;
            case UNORDERED_CI:
                this->it.uoci = new uociiter(std::move(it));
                break;
            default:
                throw std::logic_error("type for unordered_map iterator not supported");
        }
    }

    any_map::iter::reference
    any_map::iter::operator*() const
    {
        switch (type)
        {
            case ORDERED:
                return *o_it();
            case UNORDERED:
                return *uo_it();
            case UNORDERED_CI:
                return *uoci_it();
            case NONE:
                throw std::logic_error("cannot dereference an invalid iterator");
            default:
                throw std::logic_error("invalid iterator type");
        }
    }

    any_map::iter::pointer
    any_map::iter::operator->() const
    {
        switch (type)
        {
            case ORDERED:
                return o_it().operator->();
            case UNORDERED:
                return uo_it().operator->();
            case UNORDERED_CI:
                return uoci_it().operator->();
            case NONE:
                throw std::logic_error("cannot dereference an invalid iterator");
            default:
                throw std::logic_error("invalid iterator type");
        }
    }

    any_map::iter::iterator&
    any_map::iter::operator++()
    {
        switch (type)
        {
            case ORDERED:
                ++o_it();
                break;
            case UNORDERED:
                ++uo_it();
                break;
            case UNORDERED_CI:
                ++uoci_it();
                break;
            case NONE:
                throw std::logic_error("cannot increment an invalid iterator");
            default:
                throw std::logic_error("invalid iterator type");
        }

        return *this;
    }

    any_map::iter::iterator
    any_map::iter::operator++(int)
    {
        iterator tmp = *this;
        switch (type)
        {
            case ORDERED:
                o_it()++;
                break;
            case UNORDERED:
                uo_it()++;
                break;
            case UNORDERED_CI:
                uoci_it()++;
                break;
            case NONE:
                throw std::logic_error("cannot increment an invalid iterator");
            default:
                throw std::logic_error("invalid iterator type");
        }
        return tmp;
    }

    bool
    any_map::iter::operator==(iterator const& x) const
    {
        switch (type)
        {
            case ORDERED:
                return o_it() == x.o_it();
            case UNORDERED:
                return uo_it() == x.uo_it();
            case UNORDERED_CI:
                return uoci_it() == x.uoci_it();
            case NONE:
                return x.type == NONE;
            default:
                throw std::logic_error("invalid iterator type");
        }
    }

    bool
    any_map::iter::operator!=(iterator const& x) const
    {
        return !this->operator==(x);
    }

    any_map::iter::oiter const&
    any_map::iter::o_it() const
    {
        return *it.o;
    }

    any_map::iter::oiter&
    any_map::iter::o_it()
    {
        return *it.o;
    }

    any_map::iter::uoiter const&
    any_map::iter::uo_it() const
    {
        return *it.uo;
    }

    any_map::iter::uoiter&
    any_map::iter::uo_it()
    {
        return *it.uo;
    }

    any_map::iter::uociiter const&
    any_map::iter::uoci_it() const
    {
        return *it.uoci;
    }

    any_map::iter::uociiter&
    any_map::iter::uoci_it()
    {
        return *it.uoci;
    }

    // ----------------------------------------------------------
    // ------------------------  any_map  -----------------------

    any_map::any_map(map_type type, std::initializer_list<any_map::value_type> l) : type(type)
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                map.o = new ordered_any_map(l);
                break;
            case map_type::UNORDERED_MAP:
                map.uo = new unordered_any_map(l);
                break;
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                map.uoci = new unordered_any_cimap(l);
                break;
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::any_map(ordered_any_map const& m) : type(map_type::ORDERED_MAP) { map.o = new ordered_any_map(m); }

    any_map::any_map(ordered_any_map&& m) : type(map_type::ORDERED_MAP) { map.o = new ordered_any_map(std::move(m)); }

    any_map::any_map(unordered_any_map const& m) : type(map_type::UNORDERED_MAP) { map.uo = new unordered_any_map(m); }

    any_map::any_map(unordered_any_map&& m) : type(map_type::UNORDERED_MAP)
    {
        map.uo = new unordered_any_map(std::move(m));
    }

    any_map::any_map(unordered_any_cimap const& m) : type(map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS)
    {
        map.uoci = new unordered_any_cimap(m);
    }

    any_map::any_map(unordered_any_cimap&& m) : type(map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS)
    {
        map.uoci = new unordered_any_cimap(std::move(m));
    }

    any_map::any_map(any_map const& m) : type(m.type) { copy_from(m); }

    any_map&
    any_map::operator=(any_map const& m)
    {
        if (this == &m)
        {
            return *this;
        }

        destroy();
        type = m.type;
        copy_from(m);
        return *this;
    }

    any_map::any_map(any_map&& m) noexcept : type(m.type) { move_from(std::move(m)); }

    any_map&
    any_map::operator=(any_map&& m) noexcept
    {
        destroy();
        type = m.type;
        move_from(std::move(m));
        return *this;
    }

    any_map::~any_map() { destroy(); }

    any_map::iterator
    any_map::begin()
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return { o_m().begin() };
            case map_type::UNORDERED_MAP:
                return { uo_m().begin(), iter::UNORDERED };
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return { uoci_m().begin(), iter::UNORDERED_CI };
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::const_iter
    any_map::begin() const
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return { o_m().begin() };
            case map_type::UNORDERED_MAP:
                return { uo_m().begin(), const_iterator::UNORDERED };
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return { uoci_m().begin(), const_iterator::UNORDERED_CI };
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::const_iterator
    any_map::cbegin() const
    {
        return begin();
    }

    any_map::iterator
    any_map::end()
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return { o_m().end() };
            case map_type::UNORDERED_MAP:
                return { uo_m().end(), iterator::UNORDERED };
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return { uoci_m().end(), iterator::UNORDERED_CI };
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::const_iterator
    any_map::end() const
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return { o_m().end() };
            case map_type::UNORDERED_MAP:
                return { uo_m().end(), const_iterator::UNORDERED };
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return { uoci_m().end(), const_iterator::UNORDERED_CI };
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::const_iterator
    any_map::cend() const
    {
        return end();
    }

    bool
    any_map::empty() const
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return o_m().empty();
            case map_type::UNORDERED_MAP:
                return uo_m().empty();
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return uoci_m().empty();
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::size_type
    any_map::size() const
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return o_m().size();
            case map_type::UNORDERED_MAP:
                return uo_m().size();
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return uoci_m().size();
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::size_type
    any_map::count(any_map::key_type const& key) const
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return o_m().count(key);
            case map_type::UNORDERED_MAP:
                return uo_m().count(key);
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return uoci_m().count(key);
            default:
                throw std::logic_error("invalid map type");
        }
    }

    void
    any_map::clear()
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return o_m().clear();
            case map_type::UNORDERED_MAP:
                return uo_m().clear();
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return uoci_m().clear();
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::mapped_type&
    any_map::at(key_type const& key)
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return o_m().at(key);
            case map_type::UNORDERED_MAP:
                return uo_m().at(key);
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return uoci_m().at(key);
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::mapped_type const&
    any_map::at(any_map::key_type const& key) const
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return o_m().at(key);
            case map_type::UNORDERED_MAP:
                return uo_m().at(key);
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return uoci_m().at(key);
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::mapped_type&
    any_map::operator[](any_map::key_type const& key)
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return o_m()[key];
            case map_type::UNORDERED_MAP:
                return uo_m()[key];
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return uoci_m()[key];
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::mapped_type&
    any_map::operator[](any_map::key_type&& key)
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return o_m()[std::move(key)];
            case map_type::UNORDERED_MAP:
                return uo_m()[std::move(key)];
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return uoci_m()[std::move(key)];
            default:
                throw std::logic_error("invalid map type");
        }
    }

    std::pair<any_map::iterator, bool>
    any_map::insert(value_type const& value)
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
            {
                auto p = o_m().insert(value);
                return { iterator(std::move(p.first)), p.second };
            }
            case map_type::UNORDERED_MAP:
            {
                auto p = uo_m().insert(value);
                return { iterator(std::move(p.first), iterator::UNORDERED), p.second };
            }
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
            {
                auto p = uoci_m().insert(value);
                return { iterator(std::move(p.first), iterator::UNORDERED_CI), p.second };
            }
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::const_iterator
    any_map::find(key_type const& key) const
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return { o_m().find(key) };
            case map_type::UNORDERED_MAP:
                return { uo_m().find(key), const_iterator::UNORDERED };
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return { uoci_m().find(key), const_iterator::UNORDERED_CI };
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::ordered_any_map::const_iterator
    any_map::beginOM_TypeChecked() const
    {
        assert(type == ORDERED_MAP
               && "You are calling beginOM_TypeChecked() on map "
                  "whose type is not ORDERED_MAP.");
        return map.o->begin();
    }

    any_map::ordered_any_map::const_iterator
    any_map::endOM_TypeChecked() const
    {
        assert(type == ORDERED_MAP
               && "You are calling endOM_TypeChecked() on map "
                  "whose type is not ORDERED_MAP.");
        return map.o->end();
    }

    any_map::ordered_any_map::const_iterator
    any_map::findOM_TypeChecked(key_type const& key) const
    {
        assert(type == ORDERED_MAP
               && "You are calling findOM_TypeChecked() on map "
                  "whose type is not ORDERED_MAP.");
        return map.o->find(key);
    }

    any_map::unordered_any_map::const_iterator
    any_map::beginUO_TypeChecked() const
    {
        assert(type == UNORDERED_MAP
               && "You are calling beginUO_TypeChecked() on map "
                  "whose type is not UNORDERED_MAP.");
        return map.uo->begin();
    }

    any_map::unordered_any_map::const_iterator
    any_map::endUO_TypeChecked() const
    {
        assert(type == UNORDERED_MAP
               && "You are calling endUO_TypeChecked() on map "
                  "whose type is not UNORDERED_MAP.");
        return map.uo->end();
    }

    any_map::unordered_any_map::const_iterator
    any_map::findUO_TypeChecked(key_type const& key) const
    {
        assert(type == UNORDERED_MAP
               && "You are calling findUO_TypeChecked() on map "
                  "whose type is not UNORDERED_MAP.");
        return map.uo->find(key);
    }

    any_map::unordered_any_cimap::const_iterator
    any_map::beginUOCI_TypeChecked() const
    {
        assert(type == UNORDERED_MAP_CASEINSENSITIVE_KEYS
               && "You are calling beginUOCI_TypeChecked() on map "
                  "whose type is not UNORDERED_MAP_CASEINSENSITIVE_KEYS.");
        return map.uoci->begin();
    }

    any_map::unordered_any_cimap::const_iterator
    any_map::endUOCI_TypeChecked() const
    {
        assert(type == UNORDERED_MAP_CASEINSENSITIVE_KEYS
               && "You are calling endUOCI_TypeChecked() on map "
                  "whose type is not UNORDERED_MAP_CASEINSENSITIVE_KEYS.");
        return map.uoci->end();
    }

    any_map::unordered_any_cimap::const_iterator
    any_map::findUOCI_TypeChecked(key_type const& key) const
    {
        assert(type == UNORDERED_MAP_CASEINSENSITIVE_KEYS
               && "You are calling findUOCI_TypeChecked() on map "
                  "whose type is not UNORDERED_MAP_CASEINSENSITIVE_KEYS.");
        return map.uoci->find(key);
    }

    any_map::size_type
    any_map::erase(key_type const& key)
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                return o_m().erase(key);
            case map_type::UNORDERED_MAP:
                return uo_m().erase(key);
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                return uoci_m().erase(key);
            default:
                throw std::logic_error("invalid map type");
        }
    }

    any_map::ordered_any_map const&
    any_map::o_m() const
    {
        return *map.o;
    }

    any_map::ordered_any_map&
    any_map::o_m()
    {
        return *map.o;
    }

    any_map::unordered_any_map const&
    any_map::uo_m() const
    {
        return *map.uo;
    }

    any_map::unordered_any_map&
    any_map::uo_m()
    {
        return *map.uo;
    }

    any_map::unordered_any_cimap const&
    any_map::uoci_m() const
    {
        return *map.uoci;
    }

    any_map::unordered_any_cimap&
    any_map::uoci_m()
    {
        return *map.uoci;
    }

    void
    any_map::copy_from(any_map const& other)
    {
        switch (other.type)
        {
            case map_type::ORDERED_MAP:
                map.o = new ordered_any_map(other.o_m());
                break;
            case map_type::UNORDERED_MAP:
                map.uo = new unordered_any_map(other.uo_m());
                break;
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                map.uoci = new unordered_any_cimap(other.uoci_m());
                break;
            default:
                throw std::logic_error("invalid map type");
        }
    }

    void
    any_map::move_from(any_map&& other) noexcept
    {
        switch (other.type)
        {
            case map_type::ORDERED_MAP:
                map.o = other.map.o;
                other.map.o = nullptr;
                break;
            case map_type::UNORDERED_MAP:
                map.uo = other.map.uo;
                other.map.uo = nullptr;
                break;
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                map.uoci = other.map.uoci;
                other.map.uoci = nullptr;
                break;
        }
    }

    void
    any_map::destroy() noexcept
    {
        switch (type)
        {
            case map_type::ORDERED_MAP:
                delete map.o;
                break;
            case map_type::UNORDERED_MAP:
                delete map.uo;
                break;
            case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                delete map.uoci;
                break;
        }
    }

    // ----------------------------------------------------------
    // ------------------------  AnyMap  ------------------------

    AnyMap::AnyMap(std::initializer_list<any_map::value_type> l)
        : any_map(any_map::UNORDERED_MAP_CASEINSENSITIVE_KEYS, l)
    {
    }
    AnyMap::AnyMap(map_type type, std::initializer_list<any_map::value_type> l) : any_map(type, l) {}

    AnyMap::AnyMap(ordered_any_map const& m) : any_map(m) {}

    AnyMap::AnyMap(ordered_any_map&& m) : any_map(std::move(m)) {}

    AnyMap::AnyMap(unordered_any_map const& m) : any_map(m) {}

    AnyMap::AnyMap(unordered_any_map&& m) : any_map(std::move(m)) {}

    AnyMap::AnyMap(unordered_any_cimap const& m) : any_map(m) {}

    AnyMap::AnyMap(unordered_any_cimap&& m) : any_map(std::move(m)) {}

    AnyMap::map_type
    AnyMap::GetType() const
    {
        return type;
    }

    AnyMap::mapped_type const&
    AnyMap::AtCompoundKey(key_type const& key) const
    {
        return detail::AtCompoundKey(*this, key);
    }

    AnyMap::mapped_type
    AnyMap::AtCompoundKey(key_type const& key, AnyMap::mapped_type defaultValue) const noexcept
    {
        return detail::AtCompoundKey(*this, key, std::move(defaultValue));
    }

    template <>
    std::ostream&
    any_value_to_string(std::ostream& os, AnyMap const& m)
    {
        os << "{";
        using Iterator = any_map::const_iterator;
        Iterator i1 = m.begin();
        Iterator const begin = i1;
        Iterator const end = m.end();
        for (; i1 != end; ++i1)
        {
            if (i1 == begin)
            {
                os << i1->first << " : " << i1->second.ToString();
            }
            else
            {
                os << ", " << i1->first << " : " << i1->second.ToString();
            }
        }
        os << "}";
        return os;
    }

    template <>
    std::ostream&
    any_value_to_json(std::ostream& os, AnyMap const& m, uint8_t const increment, int32_t const indent)
    {
        if (m.empty())
        {
            os << "{}";
            return os;
        }

        os << "{";
        using Iterator = any_map::const_iterator;
        Iterator i1 = m.begin();
        Iterator const begin = i1;
        Iterator const end = m.end();
        for (; i1 != end; ++i1)
        {
            if (i1 != begin)
            {
                os << ", ";
            }
            newline_and_indent(os, increment, indent);
            os << "\"" << i1->first << "\" : " << i1->second.ToJSON(increment, indent + increment);
        }
        newline_and_indent(os, increment, indent - increment);
        os << "}";
        return os;
    }

    template <>
    std::ostream&
    any_value_to_cpp(std::ostream& os, AnyMap const& m, uint8_t const increment, int32_t const indent)
    {
        auto const mapType = m.GetType();
        std::string typeStr;
        switch (mapType)
        {
            case cppmicroservices::any_map::map_type::ORDERED_MAP:
                typeStr = "ORDERED_MAP";
                break;
            case cppmicroservices::any_map::map_type::UNORDERED_MAP:
                typeStr = "UNORDERED_MAP";
                break;
            case cppmicroservices::any_map::map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                typeStr = "UNORDERED_MAP_CASEINSENSITIVE_KEYS";
                break;
        }
        os << "AnyMap { " << typeStr << ", {";
        if (m.empty())
        {
            os << "}}";
            return os;
        }

        using Iterator = any_map::const_iterator;
        Iterator i1 = m.begin();
        Iterator const begin = i1;
        Iterator const end = m.end();
        for (; i1 != end; ++i1)
        {
            if (i1 != begin)
            {
                os << ", ";
            }
            newline_and_indent(os, increment, indent + increment);
            os << "{\"" << i1->first << "\" , " << i1->second.ToCPP(increment, indent + increment + increment) << "}";
        }
        newline_and_indent(os, increment, indent - increment);
        os << "}}";
        return os;
    }

    bool
    any_map::operator==(any_map const& rhs) const
    {
        if (type == rhs.type)
        {
            switch (type)
            {
                case map_type::ORDERED_MAP:
                    return (*map.o == *rhs.map.o);
                case map_type::UNORDERED_MAP:
                    return (*map.uo == *rhs.map.uo);
                case map_type::UNORDERED_MAP_CASEINSENSITIVE_KEYS:
                    return (*map.uoci == *rhs.map.uoci);
            }
        }
        return false;
    }

} // namespace cppmicroservices
