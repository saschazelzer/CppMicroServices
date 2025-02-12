#include "ChangeNamespaceImpl.hpp"
#include "FileHandler.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>

void
ChangeNamespaceImpl::copy_path(fs::path const& p)
{
    assert(!fs::is_directory(m_cppms_path / p));
    if (fs::exists(m_dest_path / p))
    {
        std::cout << "Copying (and overwriting) file: " << p.string() << "\n";
        fs::remove(m_dest_path / p);
    }
    else
    {
        std::cout << "Copying file: " << p.string() << "\n";
    }

    // Create the path to the new file if it doesn't already exist:
    create_path(p.parent_path());

    // Simply copy the files under the tools/change_namespace directory (own sources)
    if (p.string().find((fs::path("tools") / "change_namespace").string()) != std::string::npos)
    {
        fs::copy(m_cppms_path / p, m_dest_path / p, fs::copy_options::overwrite_existing);
        return;
    }

    if (m_namespace_name.size() && (is_source_file(p) || is_test_config_file(p)))
    {
        //
        // v1 hold the current content, v2 is temp buffer.
        // Each time we do a search and replace the new content
        // ends up in v2: we then swap v1 and v2, and clear v2.
        //
        static std::vector<char> v1, v2;
        v1.clear();
        v2.clear();
        std::ifstream is((m_cppms_path / p));
        std::copy(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), std::back_inserter(v1));

        const std::vector<std::string> patterns = { R"((namespace\s+)cppmicroservices(_\w+)?)",
                                              R"((<)cppmicroservices((?:_\w+)?\s*(?:::)))",
                                              R"((namespace\s+\w+\s*=\s*(?:::\s*)?)cppmicroservices(_\w+)?)",
                                              R"((^\s*#\s*define\s+\w+\s+)cppmicroservices((?:_\w+)?\s*)$)",
                                              R"((\(\s*)cppmicroservices(\s*\)))",
                                              R"(()cppmicroservices((?:_\w+)?(?:::)))" };

        const std::string replacement = "$1" + m_namespace_name + "$2";

        for (auto const& pattern : patterns)
        {
            std::regex re(pattern);
            std::smatch match;

            std::vector<char> temp_output;
            std::regex_replace(std::back_inserter(temp_output), v1.begin(), v1.end(), re, replacement);
            v1 = std::move(temp_output);
        }

        if (m_namespace_alias)
        {
            const std::regex namespace_alias("(namespace)(\\s+)(" + m_namespace_name + ")(\\s*\\{)");
            std::regex_replace(std::back_inserter(v2),
                               v1.begin(),
                               v1.end(),
                               namespace_alias,
                               "$1 $3 {} $1 cppmicroservices   = $3; $1$2$3$4");
            std::swap(v1, v2);
            v2.clear();
        }

        std::ofstream os;
        os.open((m_dest_path / p), std::ios_base::out);
        if (v1.size())
        {
            os.write(&*v1.begin(), static_cast<std::streamsize>(v1.size()));
        }
        os.close();
    }
    else if (m_namespace_name.size() && is_manifest_json_file(p))
    {
        static std::vector<char> v1, v2;
        v1.clear();
        v2.clear();
        std::ifstream is((m_cppms_path / p));
        std::copy(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), std::back_inserter(v1));

        const std::string pattern = "(\")cppmicroservices(::([^\"]*)\")";

        const std::string replacement = "$1" + m_namespace_name + "$2";

        std::regex re(pattern);
        std::smatch match;

        std::vector<char> temp_output;
        std::regex_replace(std::back_inserter(temp_output), v1.begin(), v1.end(), re, replacement);
        v1 = std::move(temp_output);

        std::ofstream os;
        os.open((m_dest_path / p), std::ios_base::out);
        if (v1.size())
        {
            os.write(&*v1.begin(), static_cast<std::streamsize>(v1.size()));
        }
        os.close();
    }
    else
    {
        // binary copy:
        fs::copy_file(m_cppms_path / p, m_dest_path / p);
    }
}

void
ChangeNamespaceImpl::create_path(fs::path const& p)
{
    if (!fs::exists(m_dest_path / p))
    {
        // recurse then create the path:
        create_path(p.parent_path());
        fs::create_directory(m_dest_path / p);
    }
}