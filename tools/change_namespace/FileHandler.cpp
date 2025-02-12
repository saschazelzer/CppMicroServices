#include "FileHandler.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <istream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct FileHandler::implementation
{
    std::vector<char> m_data;
};

FileHandler::FileHandler() { impl_ptr = std::make_shared<implementation>(); }

FileHandler::FileHandler(std::filesystem::path const& p)
{
    impl_ptr = std::make_shared<implementation>();
    open(p);
}

FileHandler::~FileHandler() = default;

FileHandler::FileHandler(FileHandler const&) {}

FileHandler& FileHandler::operator=(FileHandler const& that) = default;

FileHandler::FileHandler(FileHandler&& that) noexcept : impl_ptr(std::move(that.impl_ptr)) {}

FileHandler&
FileHandler::operator=(FileHandler&& that) noexcept
{
    impl_ptr = std::move(that.impl_ptr);
    return *this;
}

void
FileHandler::close()
{
    cow();
    impl_ptr->m_data.clear();
}

void
FileHandler::open(std::filesystem::path const& p)
{
    cow();
    std::ifstream is(p);
    if (!is)
    {
        std::string msg("Bad file name: ");
        msg += p.string();
        throw std::runtime_error(msg);
    }
    std::istreambuf_iterator<char> in(is);
    std::istreambuf_iterator<char> end;
    std::copy(in, end, std::back_inserter(impl_ptr->m_data));
}

FileHandler::const_iterator
FileHandler::begin() const
{
    return impl_ptr->m_data.size() ? &(impl_ptr->m_data[0]) : nullptr;
}

FileHandler::const_iterator
FileHandler::end() const
{
    return begin() + impl_ptr->m_data.size();
}

FileHandler::size_type
FileHandler::size() const
{
    return impl_ptr->m_data.size();
}

FileHandler::size_type
FileHandler::max_size() const
{
    return impl_ptr->m_data.max_size();
}

bool
FileHandler::empty() const
{
    return impl_ptr->m_data.empty();
}

FileHandler::const_reference
FileHandler::operator[](FileHandler::size_type n) const
{
    return impl_ptr->m_data[n];
}

FileHandler::const_reference
FileHandler::at(size_type n) const
{
    return impl_ptr->m_data.at(n);
}

FileHandler::const_reference
FileHandler::front() const
{
    return impl_ptr->m_data.front();
}

FileHandler::const_reference
FileHandler::back() const
{
    return impl_ptr->m_data.back();
}

void
FileHandler::swap(FileHandler& that) noexcept
{
    impl_ptr.swap(that.impl_ptr);
}

void
FileHandler::cow()
{
    if (impl_ptr.use_count() == 1)
    {
        impl_ptr = std::make_shared<implementation>(*impl_ptr);
    }
}