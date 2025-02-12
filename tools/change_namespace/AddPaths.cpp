#include "ChangeNamespaceImpl.hpp"
#include "FileHandler.hpp"

#include <array>
#include <iostream>
#include <regex>

void
ChangeNamespaceImpl::add_path(fs::path const& p)
{
    fs::path normalized_path = p;
    normalized_path = normalized_path.lexically_normal();
    if (fs::exists(m_cppms_path / normalized_path))
    {
        if (fs::is_directory(m_cppms_path / normalized_path))
        {
            add_directory(normalized_path);
        }
        else
        {
            add_file(normalized_path);
        }
    }
    else
    {
        std::cerr << "CAUTION: dependent file " << p.string() << " does not exist." << std::endl;
        std::cerr << "   Found while scanning file " << m_dependencies[p].string() << std::endl;
    }
}

void
ChangeNamespaceImpl::add_directory(fs::path const& p)
{
    // Parse files and directories
    for (auto const& entry : fs::directory_iterator(m_cppms_path / p))
    {
        std::string s = entry.path().string();
        if (!m_cppms_path.string().empty())
        {
            s.erase(0, m_cppms_path.string().size() + 1);
        }
        fs::path np = s;
        if (!m_dependencies.count(np))
        {
            m_dependencies[np] = p; // set up dependency tree
            add_pending_path(np);
        }
    }
}

void
ChangeNamespaceImpl::add_file(fs::path const& p)
{
    // If the file is already parsed, return
    if (m_copy_paths.find(p) != m_copy_paths.end())
    {
        return;
    }

    // Else add the file to our list
    m_copy_paths.insert(p);

    // For source files, scan for dependencies:
    if (is_source_file(p))
    {
        add_file_dependencies(p, false);
    }
}

void
ChangeNamespaceImpl::add_file_dependencies(fs::path const& p, bool scanfile)
{
    static const std::regex e(
        R"(^[[:blank:]]*(?://@bcp[[:blank:]]+([^\n]*)\n)?#[[:blank:]]*include[[:blank:]]*["<]([^">]+)[">])");

    if (!m_dependencies.count(p))
    {
        m_dependencies[p] = p; // set terminal dependency
    }

    FileHandler handler;
    if (scanfile)
    {
        handler.open(p);
    }
    else
    {
        handler.open(m_cppms_path / p);
    }

    const std::vector<int> subs = { 1, 2 };
    std::cregex_token_iterator i(handler.begin(), handler.end(), e, subs);
    std::cregex_token_iterator j;

    for (auto it = i; it != j;)
    {
        fs::path include_file;
        try
        {
            if (const std::string discard_message = *it++; discard_message.size())
            {
                // The include is optional and should be discarded:
                std::cout << "Optional functionality won't be copied: " << discard_message << std::endl;
                std::cout << "Add the file " << *it
                          << " to the list of dependencies to extract to copy this functionality." << std::endl;
                ++it;
                continue;
            }
            include_file = it->str();
            fs::path test_file(m_cppms_path / p.parent_path() / include_file);
            if (fs::exists(test_file) && !fs::is_directory(test_file)
                && (p.parent_path().string() != "CppMicroServices"))
            {
                if (!m_dependencies.count(p.parent_path() / include_file))
                {
                    m_dependencies[p.parent_path() / include_file] = p;
                    add_pending_path(p.parent_path() / include_file);
                }
            }
            else if (fs::exists(m_cppms_path / include_file))
            {
                if (!m_dependencies.count(include_file))
                {
                    m_dependencies[include_file] = p;
                    add_pending_path(include_file);
                }
            }
            ++it;
        }
        catch (fs::filesystem_error const&)
        {
            std::cerr << "Can't parse filename " << *it << " included by file " << p.string() << std::endl;
            ++it;
            continue;
        }
    }
}