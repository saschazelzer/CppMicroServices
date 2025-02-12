#include <filesystem>
#include <memory>

// FileHandler: A class for file content management
// Provides read-only access to file contents with copy-on-write semantics
class FileHandler
{
  public:
    using reference = char const&;
    using const_reference = reference;
    using iterator = char const*;
    using const_iterator = iterator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using value_type = char;
    using pointer = char const*;
    using const_pointer = pointer;

    FileHandler();
    FileHandler(std::filesystem::path const& p);
    ~FileHandler();
    FileHandler(FileHandler const& that);
    FileHandler& operator=(FileHandler const& that);
    FileHandler(FileHandler&& other) noexcept;
    FileHandler& operator=(FileHandler&& other) noexcept;

    void close();                              // Clears the content of the FileHandler
    void open(std::filesystem::path const& p); // Loads content from the specified file

    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] const_iterator end() const;
    [[nodiscard]] size_type size() const;
    [[nodiscard]] size_type max_size() const;
    [[nodiscard]] bool empty() const;
    const_reference operator[](size_type n) const;
    [[nodiscard]] const_reference at(size_type n) const;
    [[nodiscard]] const_reference front() const;
    [[nodiscard]] const_reference back() const;
    void swap(FileHandler& that) noexcept; // Swaps the content with another FileHandler

  private:
    void cow(); // Implements copy-on-write behavior

    struct implementation;
    std::shared_ptr<implementation> impl_ptr;
};