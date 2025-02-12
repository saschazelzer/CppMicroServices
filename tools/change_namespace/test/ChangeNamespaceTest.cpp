#include "CNTestingConfig.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace fs = std::filesystem;

class NamespaceReplacerTest : public ::testing::Test
{
  protected:
    fs::path tempDir; // Temporary directory for test files

    // Creates a temporary directory for each test
    void
    SetUp() override
    {
        tempDir = fs::temp_directory_path() / "change_namespace_test";
        fs::create_directories(tempDir);
    }

    // Cleans up the temporary directory after each test
    void
    TearDown() override
    {
        fs::remove_all(tempDir);
    }

    struct ReplacerParams
    {
        std::string input;
        std::string newNamespace;
        std::string fileType = "cpp";
    };

    // Runs the namespace replacer on the given input
    // Creates temporary input file, executes the tool, and returns the result
    std::string
    runReplacer(ReplacerParams const& params)
    {
        // Create temporary input file
        fs::path inputFile;
        if (params.fileType == "json")
        {
            inputFile = tempDir / "manifest.json";
        }
        else if (params.fileType == "config")
        {
            inputFile = tempDir / "config.in.h";
        }
        else
        {
            inputFile = tempDir / "input.cpp";
        }
        std::ofstream ofs(inputFile);
        ofs << params.input;
        ofs.close();

        // Create output directory
        fs::path outputDir = tempDir / "output";
        fs::create_directories(outputDir);

        // Run the executable
        runExecutable(tempDir.string(), params.newNamespace, outputDir.string());

        // Read the output file
        fs::path outputFile = outputDir / inputFile.filename();
        std::ifstream ifs(outputFile);
        std::string output((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        ifs.close();

        return output;
    }

    // Executes the namespace replacer tool with the given parameters
    // Throws an exception if the execution fails
    void
    runExecutable(std::string const& inputFile, std::string const& newNamespace, std::string const& outputDir)
    {
        const std::string command
            = std::string(EXE_PATH) + " --cppms=" + inputFile + " --namespace=" + newNamespace + " --namespace-alias " + outputDir;
        std::cout << "Test command used to change namespace: " << command << std::endl;
        int result = std::system(command.c_str());
        if (result != 0)
        {
            throw std::runtime_error("Executable failed with error code: " + std::to_string(result));
        }
    }
};

// Tests replacement of namespace declarations
TEST_F(NamespaceReplacerTest, ReplacesNamespaceDeclaration)
{
    const ReplacerParams params = { R"(
namespace cppmicroservices {
    void someFunction() {
        // Some code here
    }
}
)",
                                    "newnamespace" };
    const std::string expected = R"(
namespace newnamespace {} namespace cppmicroservices   = newnamespace; namespace newnamespace {
    void someFunction() {
        // Some code here
    }
}
)";

    const std::string result = runReplacer(params);
    EXPECT_EQ(result, expected);
}

// Tests replacement of using namespace declarations
TEST_F(NamespaceReplacerTest, ReplacesUsingNamespaceDeclaration)
{
    const ReplacerParams params = { R"(
using namespace cppmicroservices;

void someFunction() {
    // Some code here
}
)",
                                    "newnamespace" };
    const std::string expected = R"(
using namespace newnamespace;

void someFunction() {
    // Some code here
}
)";

    const std::string result = runReplacer(params);
    EXPECT_EQ(result, expected);
}

// Tests replacement of namespaces in template parameters
TEST_F(NamespaceReplacerTest, ReplacesNamespaceInTemplateParameters)
{
    const ReplacerParams params = { R"(
std::vector<cppmicroservices::ServiceReference> serviceRefs;
std::map<cppmicroservices::Bundle, cppmicroservices::framework::BundleContext> bundleContextMap;
)",
                                    "newnamespace" };
    const std::string expected = R"(
std::vector<newnamespace::ServiceReference> serviceRefs;
std::map<newnamespace::Bundle, newnamespace::framework::BundleContext> bundleContextMap;
)";

    const std::string result = runReplacer(params);
    EXPECT_EQ(result, expected);
}

// Tests replacement of namespace aliases
TEST_F(NamespaceReplacerTest, ReplacesNamespaceAlias)
{
    const ReplacerParams params = { "namespace alias = cppmicroservices;", "newnamespace" };
    const std::string expected = "namespace alias = newnamespace;";
    const std::string result = runReplacer(params);
    EXPECT_EQ(result, expected);
}

// Tests replacement of namespaces in #define macros
TEST_F(NamespaceReplacerTest, ReplacesDefine)
{
    const ReplacerParams params = { "#define MACRO cppmicroservices", "newnamespace" };
    const std::string expected = "#define MACRO newnamespace";
    const std::string result = runReplacer(params);
    EXPECT_EQ(result, expected);
}

// Tests replacement of namespaces enclosed in parentheses
TEST_F(NamespaceReplacerTest, ReplacesParenthesizedNamespace)
{
    const ReplacerParams params = { "void func(cppmicroservices) {}", "newnamespace" };
    const std::string expected = "void func(newnamespace) {}";
    const std::string result = runReplacer(params);
    EXPECT_EQ(result, expected);
}

// Tests replacement of namespace prefixes
TEST_F(NamespaceReplacerTest, ReplacesNamespacePrefix)
{
    const ReplacerParams params = { "cppmicroservices::Bundle bundle;", "newnamespace" };
    const std::string expected = "newnamespace::Bundle bundle;";
    const std::string result = runReplacer(params);
    EXPECT_EQ(result, expected);
}

// Tests error handling for invalid inputs
TEST_F(NamespaceReplacerTest, ThrowsExceptionOnInvalidInput)
{
    // Test missing --cppms flag
    EXPECT_THROW({ runExecutable("", "newns", tempDir.string()); }, std::runtime_error);

    // Test missing --namespace flag
    EXPECT_THROW({ runExecutable(tempDir.string(), "", tempDir.string()); }, std::runtime_error);

    // Test missing output directory
    EXPECT_THROW({ runExecutable(tempDir.string(), "newns", ""); }, std::runtime_error);

    // Test non-existent input directory
    EXPECT_THROW({ runExecutable("/non/existent/path", "newns", tempDir.string()); }, std::runtime_error);

    // Test non-existent output directory
    EXPECT_THROW({ runExecutable(tempDir.string(), "newns", "/non/existent/path"); }, std::runtime_error);
}

// Tests replacement of namespaces in manifest.json files
TEST_F(NamespaceReplacerTest, ReplacesNamespaceInManifestJson)
{
    const ReplacerParams params = { R"(
{
  "bundle.symbolic_name" : "cppmicroservices::example",
  "bundle.activator" : true,
  "bundle.name" : "Example Plugin",
  "bundle.description" : "A plugin using cppmicroservices framework"
}
)",
                                    "newnamespace",
                                    "json" };
    const std::string expected = R"(
{
  "bundle.symbolic_name" : "newnamespace::example",
  "bundle.activator" : true,
  "bundle.name" : "Example Plugin",
  "bundle.description" : "A plugin using cppmicroservices framework"
}
)";

    const std::string result = runReplacer(params);
    EXPECT_EQ(result, expected);
}

// Tests replacement of namespaces in config.h.in files
TEST_F(NamespaceReplacerTest, ReplacesNamespaceInConfigHIn)
{
    const ReplacerParams params = { R"(
namespace cppmicroservices {
  // Some configuration code here
}
)",
                                    "newnamespace",
                                    "config" };
    const std::string expected = R"(
namespace newnamespace {} namespace cppmicroservices   = newnamespace; namespace newnamespace {
  // Some configuration code here
}
)";

    const std::string result = runReplacer(params);
    EXPECT_EQ(result, expected);
}