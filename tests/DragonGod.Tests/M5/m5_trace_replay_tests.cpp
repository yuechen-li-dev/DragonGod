#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/m1_single_frame.h"

#include <string>
#include <vector>

namespace
{
    void AppendTickTrace(
        std::vector<dragongod::TickTraceEntry>& into,
        const std::vector<dragongod::TickTraceEntry>& from)
    {
        for (const dragongod::TickTraceEntry& entry : from) {
            into.push_back(entry);
        }
    }

    [[nodiscard]] dragongod::TraceComparisonResult CompareUninterruptedAndRestored(
        dragongod::StackScriptScenario scenario,
        const dragongod::RuntimeMailboxInput& mailboxInput,
        dragongod::TickIndex firstLegTicks,
        dragongod::TickIndex remainingTicks)
    {
        dragongod::StackFrameRuntimeSession uninterrupted(scenario, mailboxInput);
        const dragongod::FrameRunResult uninterruptedResult = uninterrupted.RunForTicks(firstLegTicks + remainingTicks);

        dragongod::StackFrameRuntimeSession split(scenario, mailboxInput);
        const dragongod::FrameRunResult firstLeg = split.RunForTicks(firstLegTicks);
        const dragongod::RuntimeChunk snapshot = split.Save();
        dragongod::StackFrameRuntimeSession restored(snapshot);
        const dragongod::FrameRunResult secondLeg = restored.RunForTicks(remainingTicks);

        std::vector<dragongod::TickTraceEntry> splitTrace;
        AppendTickTrace(splitTrace, firstLeg.tickTrace);
        AppendTickTrace(splitTrace, secondLeg.tickTrace);

        return dragongod::CompareTickTraces(uninterruptedResult.tickTrace, splitTrace);
    }
}

FACT(M5_IdenticalDeterministicRuns_CompareEqual)
{
    const dragongod::StackFrameRuntime runtime;
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 },
        dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 22 }
    };

    const dragongod::FrameRunResult firstRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        8,
        mailboxInput);
    const dragongod::FrameRunResult secondRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        8,
        mailboxInput);

    const dragongod::TraceComparisonResult comparison = dragongod::CompareTickTraces(firstRun.tickTrace, secondRun.tickTrace);
    ASSERT_TRUE(comparison.matches, "identical runs should match on bounded tick trace entries");
    ASSERT_EQUAL(static_cast<std::size_t>(0), comparison.firstMismatchIndex, "matching traces keep mismatch index at default");
}

FACT(M5_UninterruptedAndRestoredRuns_CompareEqual_ThroughChunkPath)
{
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 }
    };

    const dragongod::TraceComparisonResult comparison = CompareUninterruptedAndRestored(
        dragongod::StackScriptScenario::MailboxEnqueueDuringTickComplete,
        mailboxInput,
        1,
        7);

    ASSERT_TRUE(comparison.matches, "uninterrupted vs restored runs should match via chunk-based save/restore");
}

FACT(M5_FirstDivergence_IsReportedWithExpectedAndActualPayload)
{
    const dragongod::StackFrameRuntime runtime;

    dragongod::RuntimeMailboxInput leftInput;
    leftInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 },
        dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 22 }
    };

    dragongod::RuntimeMailboxInput rightInput;
    rightInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 },
        dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 99 }
    };

    const dragongod::FrameRunResult expectedRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        8,
        leftInput);
    const dragongod::FrameRunResult actualRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        8,
        rightInput);

    const dragongod::TraceComparisonResult comparison = dragongod::CompareTickTraces(expectedRun.tickTrace, actualRun.tickTrace);
    ASSERT_FALSE(comparison.matches, "different inputs should produce a structural mismatch");
    ASSERT_TRUE(comparison.firstMismatchIndex < expectedRun.tickTrace.size(), "mismatch index should point into compared trace");
    ASSERT_TRUE(!comparison.mismatchReason.empty(), "mismatch should include a reason");
    ASSERT_TRUE(!(comparison.expected == comparison.actual), "comparison should expose both expected and actual mismatched entries");
}

FACT(M5_ComparisonArtifacts_AreBoundedAndUseful)
{
    const dragongod::StackFrameRuntime runtime;
    dragongod::RuntimeMailboxInput leftInput;
    leftInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 },
        dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 22 }
    };

    dragongod::RuntimeMailboxInput rightInput;
    rightInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 },
        dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 99 }
    };

    const dragongod::FrameRunResult expectedRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        8,
        leftInput);
    const dragongod::FrameRunResult actualRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        8,
        rightInput);

    const dragongod::TraceComparisonResult comparison = dragongod::CompareTickTraces(expectedRun.tickTrace, actualRun.tickTrace);
    const std::string artifact = dragongod::FormatTraceComparison(comparison);

    ASSERT_TRUE(!artifact.empty(), "formatted artifact should not be empty");
    ASSERT_TRUE(artifact.find("firstMismatchIndex=") != std::string::npos, "artifact should include first mismatch index");
    ASSERT_TRUE(artifact.size() < 2048, "artifact should remain bounded for loop-friendly diagnostics");
    ASSERT_TRUE(context.WriteTextArtifact("m5_trace_comparison.txt", artifact), "artifact should be materialized for inspection");
}

FACT(M5_RepeatedComparisons_AreDeterministic)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult runA = runtime.RunForTicks(
        dragongod::StackScriptScenario::BlackboardParentChildComplete,
        8,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult runB = runtime.RunForTicks(
        dragongod::StackScriptScenario::BlackboardParentChildComplete,
        8,
        dragongod::RuntimeMailboxInput{});

    const dragongod::TraceComparisonResult firstComparison = dragongod::CompareTickTraces(runA.tickTrace, runB.tickTrace);
    const dragongod::TraceComparisonResult secondComparison = dragongod::CompareTickTraces(runA.tickTrace, runB.tickTrace);

    ASSERT_TRUE(firstComparison.matches, "first deterministic comparison should match");
    ASSERT_TRUE(secondComparison.matches, "second deterministic comparison should match");
    ASSERT_EQUAL(firstComparison.firstMismatchIndex, secondComparison.firstMismatchIndex, "repeated comparisons should agree on mismatch index");
    ASSERT_EQUAL(firstComparison.expectedEntryCount, secondComparison.expectedEntryCount, "repeated comparisons should agree on expected entry count");
    ASSERT_EQUAL(firstComparison.actualEntryCount, secondComparison.actualEntryCount, "repeated comparisons should agree on actual entry count");
}
