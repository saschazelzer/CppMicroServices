#include <memory>
#include <string>

class CNApplication;
using cn_app_ptr = std::shared_ptr<CNApplication>;

// Abstract base class for namespace changing application
class CNApplication
{
  public:
    virtual ~CNApplication() = default;
    virtual void set_cppms_path(std::string_view p) = 0;   // Set the path to the CppMicroServices source directory
    virtual void set_destination(std::string_view p) = 0;  // Set the destination path for modified files
    virtual void set_namespace(std::string_view name) = 0; // Set the new namespace name
    virtual void set_namespace_alias(bool) = 0;            // Set whether to create an alias for the old namespace
    [[nodiscard]] virtual std::string get_cppms_path() const = 0;  // Get the CppMicroServices source path
    [[nodiscard]] virtual std::string get_destination() const = 0; // Get the destination path
    [[nodiscard]] virtual std::string get_namespace() const = 0;   // Get the new namespace name

    virtual int run() = 0; // Execute the namespace changing process

    static cn_app_ptr create(); // Factory method to create a CNApplication instance
};