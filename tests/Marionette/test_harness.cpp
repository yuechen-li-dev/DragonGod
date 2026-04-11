#include "test_harness.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

namespace marionette::tests
{
    namespace
    {
        [[nodiscard]] std::filesystem::path GetArtifactRoot()
        {
            return std::filesystem::path{ MARIONETTE_TEST_REPO_ROOT } / "out" / "test-artifacts";
        }

        [[nodiscard]] std::string SanitizePathComponent(std::string_view value)
        {
            std::string sanitized;
            sanitized.reserve(value.size());

            for (const char character : value) {
                const bool isLetter =
                    (character >= 'a' && character <= 'z') ||
                    (character >= 'A' && character <= 'Z');
                const bool isDigit = character >= '0' && character <= '9';
                const bool isAllowedPunctuation = character == '-' || character == '_';

                if (isLetter || isDigit || isAllowedPunctuation) {
                    sanitized.push_back(character);
                    continue;
                }

                sanitized.push_back('_');
            }

            if (sanitized.empty()) {
                return "unnamed";
            }

            return sanitized;
        }

        [[nodiscard]] bool MatchesFilter(std::string_view testName, std::string_view filter)
        {
            if (filter.empty()) {
                return true;
            }

            return testName.find(filter) != std::string_view::npos;
        }

        void PrintFailure(const TestContext&, const Failure& failure)
        {
            std::cout
                << "  FAIL " << failure.testName
                << " at " << failure.file << ":" << failure.line
                << " [" << failure.assertion << "]\n"
                << "    message: " << failure.message << "\n";

            if (!failure.expected.empty() || !failure.actual.empty()) {
                std::cout << "    expected: " << failure.expected << "\n";
                std::cout << "    actual:   " << failure.actual << "\n";
            }
        }

        void PrintSkip(const TestContext&, const Skip& skip)
        {
            std::cout
                << "  SKIP " << skip.testName
                << " at " << skip.file << ":" << skip.line << "\n"
                << "    reason: " << skip.reason << "\n";
        }

        void PrintArtifacts(const TestContext& context)
        {
            for (const std::filesystem::path& artifactPath : context.ArtifactPaths()) {
                std::cout << "    artifact: " << artifactPath.lexically_normal().string() << "\n";
            }
        }
    }

    TestContext::TestContext(std::string_view testName)
        : testName_(testName)
    {
    }

    [[nodiscard]] std::string_view TestContext::TestName() const
    {
        return testName_;
    }

    [[nodiscard]] std::string TestContext::DisplayName() const
    {
        if (theoryCaseName_.empty()) {
            return testName_;
        }

        return testName_ + "[" + theoryCaseName_ + "]";
    }

    [[nodiscard]] const std::vector<Failure>& TestContext::Failures() const
    {
        return failures_;
    }

    [[nodiscard]] const std::vector<std::filesystem::path>& TestContext::ArtifactPaths() const
    {
        return artifactPaths_;
    }

    [[nodiscard]] const Skip* TestContext::SkipState() const
    {
        if (!skipped_) {
            return nullptr;
        }

        return &skip_;
    }

    [[nodiscard]] bool TestContext::HasFailures() const
    {
        return !failures_.empty();
    }

    [[nodiscard]] bool TestContext::IsSkipped() const
    {
        return skipped_;
    }

    [[nodiscard]] std::filesystem::path TestContext::ArtifactDirectory() const
    {
        return GetArtifactRoot() / SanitizePathComponent(DisplayName());
    }

    void TestContext::RecordFailure(
        const char* file,
        int line,
        std::string_view assertion,
        std::string_view message,
        std::string_view expected,
        std::string_view actual)
    {
        failures_.push_back(Failure{
            .testName = DisplayName(),
            .file = file,
            .line = line,
            .assertion = std::string(assertion),
            .message = std::string(message),
            .expected = std::string(expected),
            .actual = std::string(actual)
        });
    }

    void TestContext::SkipTest(const char* file, int line, std::string_view reason)
    {
        skipped_ = true;
        skip_ = Skip{
            .testName = DisplayName(),
            .file = file,
            .line = line,
            .reason = std::string(reason)
        };
    }

    void TestContext::EnterTheoryCase(std::string_view caseName)
    {
        theoryCaseName_ = caseName;
    }

    void TestContext::LeaveTheoryCase()
    {
        theoryCaseName_.clear();
    }

    [[nodiscard]] bool TestContext::WriteTextArtifact(std::string_view artifactName, std::string_view content)
    {
        const std::filesystem::path directory = ArtifactDirectory();
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            RecordFailure(
                __FILE__,
                __LINE__,
                "WRITE_TEXT_ARTIFACT",
                "failed to create artifact directory",
                directory.string(),
                error.message());
            return false;
        }

        const std::filesystem::path artifactPath =
            directory / (SanitizePathComponent(artifactName) + ".txt");
        std::ofstream artifactFile(artifactPath, std::ios::binary | std::ios::trunc);
        if (!artifactFile.is_open()) {
            RecordFailure(
                __FILE__,
                __LINE__,
                "WRITE_TEXT_ARTIFACT",
                "failed to open artifact file",
                artifactPath.string(),
                "could not open file");
            return false;
        }

        artifactFile << std::string(content);
        artifactFile.close();
        if (!artifactFile) {
            RecordFailure(
                __FILE__,
                __LINE__,
                "WRITE_TEXT_ARTIFACT",
                "failed to write artifact file",
                artifactPath.string(),
                "write failed");
            return false;
        }

        artifactPaths_.push_back(artifactPath);
        return true;
    }

    TestRegistrar::TestRegistrar(const char* testName, TestFunction function)
    {
        Registry().push_back(TestCase{
            .name = testName,
            .function = function
        });
    }

    [[nodiscard]] std::vector<TestCase>& Registry()
    {
        static std::vector<TestCase> registry;
        return registry;
    }

    [[nodiscard]] int RunAllTests(std::string_view filter)
    {
        std::vector<TestCase>& tests = Registry();
        std::sort(
            tests.begin(),
            tests.end(),
            [](const TestCase& left, const TestCase& right)
            {
                return left.name < right.name;
            });

        int executedCount = 0;
        int passedCount = 0;
        int failedCount = 0;
        int skippedCount = 0;
        int totalFailureCount = 0;

        for (const TestCase& test : tests) {
            if (!MatchesFilter(test.name, filter)) {
                continue;
            }

            ++executedCount;

            TestContext context(test.name);
            test.function(context);

            if (context.IsSkipped()) {
                ++skippedCount;
                std::cout << "[SKIP] " << test.name << "\n";
                PrintSkip(context, *context.SkipState());
                PrintArtifacts(context);
                continue;
            }

            if (context.HasFailures()) {
                ++failedCount;
                totalFailureCount += static_cast<int>(context.Failures().size());
                std::cout << "[FAIL] " << test.name << "\n";
                for (const Failure& failure : context.Failures()) {
                    PrintFailure(context, failure);
                }
                PrintArtifacts(context);
                continue;
            }

            ++passedCount;
            std::cout << "[PASS] " << test.name << "\n";
            PrintArtifacts(context);
        }

        std::cout
            << "\nSummary: "
            << executedCount << " test(s), "
            << passedCount << " passed, "
            << skippedCount << " skipped, "
            << failedCount << " failed, "
            << totalFailureCount << " assertion failure(s)\n";

        return failedCount == 0 ? 0 : 1;
    }

    [[nodiscard]] std::string FormatValue(bool value)
    {
        return value ? "true" : "false";
    }

    [[nodiscard]] std::string FormatValue(int value)
    {
        return std::to_string(value);
    }

    [[nodiscard]] std::string FormatValue(std::size_t value)
    {
        return std::to_string(value);
    }

    [[nodiscard]] std::string FormatValue(unsigned int value)
    {
        return std::to_string(value);
    }

    [[nodiscard]] std::string FormatValue(const char* value)
    {
        if (value == nullptr) {
            return "null";
        }

        return std::string(value);
    }

    [[nodiscard]] std::string FormatValue(const std::string& value)
    {
        return value;
    }

    [[nodiscard]] std::string FormatValue(std::string_view value)
    {
        return std::string(value);
    }
}
