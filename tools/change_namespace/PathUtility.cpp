#include "ChangeNamespaceImpl.hpp"
#include <cctype>

int
compare_paths(fs::path const& a, fs::path const& b)
{
    std::string const& as = a.generic_string();
    std::string const& bs = b.generic_string();
    std::string::const_iterator i, j, k, l;
    i = as.begin();
    j = as.end();
    k = bs.begin();
    l = bs.end();
    while (i != j)
    {
        if (k == l)
        {
            return -1;
        }
        int r = std::tolower(*i) - std::tolower(*k);
        if (r)
        {
            return r;
        }
        ++i;
        ++k;
    }
    return (k == l) ? 0 : 1;
}