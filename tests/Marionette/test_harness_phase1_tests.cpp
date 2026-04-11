#include "test_harness.h"

#include <cstdint>
#include <string_view>

namespace
{
    BENCHMARK_WITH_ITERATIONS(MarionetteBenchmarkSmoke, 128)
    {
        volatile std::uint64_t sample = context.iteration + 1;
        (void)sample;
    }

    [[nodiscard]] bool ContainsText(std::string_view text, std::string_view token)
    {
        return text.find(token) != std::string_view::npos;
    }
}

FACT(AssertNearPassesWithinTolerance)
{
    ASSERT_NEAR(10.0, 10.2, 0.3, "difference inside tolerance should pass");
}

FACT(AssertNearPassesOnToleranceBoundary)
{
    ASSERT_NEAR(3.0f, 3.1f, 0.1f, "difference on boundary should pass");
}

FACT(AssertNearFailureReportsExpectedActualToleranceAndDifference)
{
    ::marionette::tests::TestContext nearContext("AssertNearFailure");
    ::marionette::tests::AssertNear(nearContext, 1.0, 1.4, 0.2, __FILE__, __LINE__, "outside tolerance");

    ASSERT_TRUE(nearContext.HasFailures(), "assert near failure should be recorded");
    ASSERT_EQUAL(static_cast<std::size_t>(1), nearContext.Failures().size(), "one failure is expected");

    const ::marionette::tests::Failure& failure = nearContext.Failures().front();
    ASSERT_EQUAL(std::string("ASSERT_NEAR"), failure.assertion, "assertion label should match");
    ASSERT_EQUAL(std::string("1"), failure.expected, "expected value should be present");
    ASSERT_EQUAL(std::string("1.3999999999999999"), failure.actual, "actual value should be present");
    ASSERT_TRUE(ContainsText(failure.message, "outside tolerance"), "message should include user message");
    ASSERT_TRUE(ContainsText(failure.message, "tolerance=0.20000000000000001"), "message should include tolerance");
    ASSERT_TRUE(ContainsText(failure.message, "difference=0.39999999999999991"), "message should include absolute difference");
}

FACT(BenchmarkRegistrationIncludesSmokeBenchmark)
{
    bool benchmarkFound = false;
    for (const auto& benchmark : ::marionette::tests::BenchmarkRegistry()) {
        if (benchmark.name == "MarionetteBenchmarkSmoke") {
            benchmarkFound = true;
            ASSERT_EQUAL(static_cast<std::uint64_t>(128), benchmark.iterations, "benchmark iterations should be preserved");
            break;
        }
    }

    ASSERT_TRUE(benchmarkFound, "benchmark should be discoverable in benchmark registry");
}

FACT(BenchmarkExecutionProducesStructuredResults)
{
    const std::vector<::marionette::tests::BenchmarkResult> results =
        ::marionette::tests::ExecuteBenchmarks("MarionetteBenchmarkSmoke");

    ASSERT_EQUAL(static_cast<std::size_t>(1), results.size(), "benchmark filter should select one benchmark");
    ASSERT_EQUAL(std::string("MarionetteBenchmarkSmoke"), results.front().name, "result name should match benchmark");
    ASSERT_EQUAL(static_cast<std::uint64_t>(128), results.front().iterations, "result should include iteration count");
}

FACT(BenchmarkCategoryIsDistinctFromTests)
{
    bool benchmarkAppearsInTests = false;
    for (const auto& testCase : ::marionette::tests::Registry()) {
        if (testCase.name == "MarionetteBenchmarkSmoke") {
            benchmarkAppearsInTests = true;
            break;
        }
    }

    ASSERT_FALSE(benchmarkAppearsInTests, "benchmark should not be registered as a test case");
}
