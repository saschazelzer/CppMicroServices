#include "ChangeNamespace.hpp"

#include <cstring>
#include <filesystem>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>

namespace fs = std::filesystem;

class FileHandler;

// Path operations:
// Path comparison function
int compare_paths(fs::path const& a, fs::path const& b);

// Path equality check
inline bool
equal_paths(fs::path const& a, fs::path const& b)
{
    return compare_paths(a, b) == 0;
}

// Custom comparator for path ordering
struct path_less
{
    bool
    operator()(fs::path const& a, fs::path const& b) const
    {
        return compare_paths(a, b) < 0;
    }
};

// Implementation of the CNApplication interface
class ChangeNamespaceImpl : public CNApplication
{
  public:
    ChangeNamespaceImpl();
    ~ChangeNamespaceImpl() = default;

    // Utility functions for file type checking
    static bool is_source_file(fs::path const& p);
    static bool is_test_config_file(fs::path const& p);
    static bool is_manifest_json_file(fs::path const& p);

  private:
    // Implementation of CNApplication interface methods:

    void set_cppms_path(std::string_view p) override;
    void set_destination(std::string_view p) override;
    void set_namespace(std::string_view name) override;
    void set_namespace_alias(bool) override;
    [[nodiscard]] std::string get_cppms_path() const override;
    [[nodiscard]] std::string get_destination() const override;
    [[nodiscard]] std::string get_namespace() const override;

    // run: Main execution function for the namespace changing process
    // Checks if the CppMicroServices source and destination paths exist
    // Processes all files in the CppMicroServices directory
    // Handles pending paths and copies modified files
    int run() override;

    // Helper functions:

    // add_path: Adds the given file path to the list of paths to be copied
    // If the file is a source file, calls add_file_dependencies() to process its includes
    void add_path(fs::path const& p);

    // add_pending_path: Adds the given file to the list of pending paths to be parsed
    void
    add_pending_path(fs::path const& p)
    {
        m_pending_paths.push(p);
    }

    // add_directory: Iterates through all entries in the given directory
    // For each entry:
    //   - Removes the CppMicroServices path prefix
    //   - Adds the entry to the dependency tree
    //   - Adds the entry to the pending paths for future processing
    void add_directory(fs::path const& p);

    // add_file: Adds the given file path to the list of paths to be copied
    // If the file is a source file, calls add_file_dependencies() to process its includes
    void add_file(fs::path const& p);

    // copy_path: Copies a file from the source path to the destination path
    // If the file already exists in the destination, it's overwritten
    // For source files or test config files:
    //   - Reads the file content
    //   - Applies namespace replacements using regex
    //   - Adds namespace alias if required
    //   - Writes the modified content to the destination
    // For manifest JSON files:
    //   - Applies specific replacements for JSON content
    // For other files:
    //   - Performs a binary copy
    void copy_path(fs::path const& p);

    // add_file_dependencies: Processes include statements in the given file
    // Uses regex to find include statements and optional discard messages
    // For each include found:
    //   - If there's a discard message, logs it and skips the include
    //   - Otherwise, checks if the included file exists
    //   - Adds the included file to the dependencies and pending paths if not already present
    // Handles filesystem errors during parsing
    void add_file_dependencies(fs::path const& p, bool scanfile);

    // create_path: Recursively creates the directory structure for a given path
    // Ensures that all parent directories exist before creating a new directory
    void create_path(fs::path const& p);

    bool m_namespace_alias;                     // make "cppms" a namespace alias when doing a namespace rename.
    fs::path m_cppms_path;                      // the path to the cppms root
    fs::path m_dest_path;                       // the path to copy to
    std::set<fs::path, path_less> m_copy_paths; // list of files to copy
    std::map<fs::path, fs::path, path_less> m_dependencies;    // dependency information
    std::string m_namespace_name;                              // namespace rename.
    std::queue<fs::path, std::list<fs::path>> m_pending_paths; // Queue of paths we haven't scanned yet
};