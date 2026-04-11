#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/runtime_compat.h"

#include <string>
#include <vector>

namespace
{
    namespace Keys
    {
        constexpr dragongod::BbKey<int> UtilityChoice{ .name = "UtilityChoice", .slot = 10 };
        constexpr dragongod::BbKey<int> UtilityDecisionsMade{ .name = "UtilityDecisionsMade", .slot = 11 };
    }

    void AppendTickTrace(
        std::vector<dragongod::TickTraceEntry>& into,
        const std::vector<dragongod::TickTraceEntry>& from)
    {
        for (const dragongod::TickTraceEntry& entry : from) {
            into.push_back(entry);
        }
    }
}

FACT(M6_CanonicalUtilityAuthorShape_HighestScoreWinsDeterministically)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::UtilityHighestScoreComplete,
        8,
        dragongod::RuntimeMailboxInput{});

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "utility highest score scenario should complete");
    ASSERT_EQUAL(2, run.finalBlackboard.GetOr(Keys::UtilityChoice, -1), "highest score candidate should win deterministically");
    ASSERT_EQUAL(1, run.finalBlackboard.GetOr(Keys::UtilityDecisionsMade, -1), "canonical utility scenario should make exactly one decision");
}

FACT(M6_Hysteresis_BlocksSmallChallengerFlip)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::UtilityHysteresisComplete,
        12,
        dragongod::RuntimeMailboxInput{});

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "hysteresis scenario should complete");
    ASSERT_EQUAL(1, run.finalBlackboard.GetOr(Keys::UtilityChoice, -1), "challenger below hysteresis threshold should be blocked");

    bool sawHysteresisBlock = false;
    for (const dragongod::TickTraceEntry& tick : run.tickTrace) {
        for (const dragongod::UtilityDecisionTraceEntry& decision : tick.utilityDecisions) {
            if (decision.hysteresisBlocked) {
                sawHysteresisBlock = true;
            }
        }
    }

    ASSERT_TRUE(sawHysteresisBlock, "trace should expose hysteresis block evidence");
}

FACT(M6_MinCommit_SticksUntilWindowExpires_ThenSwitches)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::UtilityMinCommitComplete,
        16,
        dragongod::RuntimeMailboxInput{});

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "min commit scenario should complete");
    ASSERT_EQUAL(2, run.finalBlackboard.GetOr(Keys::UtilityChoice, -1), "challenger should win after commit window expires");

    bool sawMinCommitBlock = false;
    bool sawSwitchAfterWindow = false;
    for (const dragongod::TickTraceEntry& tick : run.tickTrace) {
        for (const dragongod::UtilityDecisionTraceEntry& decision : tick.utilityDecisions) {
            if (decision.minCommitBlocked) {
                sawMinCommitBlock = true;
            }

            if (!decision.minCommitBlocked && decision.chosen == dragongod::FrameId::UtilityActionReload) {
                sawSwitchAfterWindow = true;
            }
        }
    }

    ASSERT_TRUE(sawMinCommitBlock, "trace should expose min-commit blocking evidence");
    ASSERT_TRUE(sawSwitchAfterWindow, "trace should expose eventual switch after commit window");
}

FACT(M6_TieBreakPolicies_AreDeterministicAndExplicit)
{
    const dragongod::StackFrameRuntime runtime;

    const dragongod::FrameRunResult keepCurrentRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::UtilityTieBreakKeepCurrentComplete,
        12,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult firstListedRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::UtilityTieBreakFirstListedComplete,
        8,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult lastListedRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::UtilityTieBreakLastListedComplete,
        8,
        dragongod::RuntimeMailboxInput{});

    ASSERT_EQUAL(1, keepCurrentRun.finalBlackboard.GetOr(Keys::UtilityChoice, -1), "KeepCurrent should hold prior committed winner on tie");
    ASSERT_EQUAL(1, firstListedRun.finalBlackboard.GetOr(Keys::UtilityChoice, -1), "FirstListed should resolve tie to first candidate");
    ASSERT_EQUAL(2, lastListedRun.finalBlackboard.GetOr(Keys::UtilityChoice, -1), "LastListed should resolve tie to last candidate");
}

FACT(M6_RepeatedRuns_And_SaveRestore_NoUtilityDrift)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult runA = runtime.RunForTicks(
        dragongod::StackScriptScenario::UtilityMinCommitComplete,
        16,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult runB = runtime.RunForTicks(
        dragongod::StackScriptScenario::UtilityMinCommitComplete,
        16,
        dragongod::RuntimeMailboxInput{});

    ASSERT_SEQUENCE_EQUAL(
        dragongod::SerializeTickTrace(runA.tickTrace),
        dragongod::SerializeTickTrace(runB.tickTrace),
        "repeated utility runs should be deterministic on structural+utility tick trace");

    dragongod::StackFrameRuntimeSession split(
        dragongod::StackScriptScenario::UtilityMinCommitComplete,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult leg1 = split.RunForTicks(4);
    const dragongod::RuntimeChunk snapshot = split.Save();
    dragongod::StackFrameRuntimeSession restored(snapshot);
    const dragongod::FrameRunResult leg2 = restored.RunForTicks(12);

    std::vector<dragongod::TickTraceEntry> splitTrace;
    AppendTickTrace(splitTrace, leg1.tickTrace);
    AppendTickTrace(splitTrace, leg2.tickTrace);

    const dragongod::TraceComparisonResult comparison = dragongod::CompareTickTraces(runA.tickTrace, splitTrace);
    ASSERT_TRUE(comparison.matches, "save/restore utility memory should match uninterrupted execution");
}

FACT(M6_UtilityDecisions_CoexistWithStackDirtyAndMailboxSemantics)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::UtilityHighestScoreComplete,
        8,
        dragongod::RuntimeMailboxInput{});

    bool sawUtilityPush = false;
    bool sawUtilityPop = false;
    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind == dragongod::FrameTraceKind::Push && event.targetFrame == dragongod::FrameId::UtilityActionReload) {
            sawUtilityPush = true;
        }

        if (event.kind == dragongod::FrameTraceKind::Pop && event.activeFrame == dragongod::FrameId::UtilityActionReload) {
            sawUtilityPop = true;
        }
    }

    ASSERT_TRUE(sawUtilityPush, "utility winner should fold into normal push structural consequence");
    ASSERT_TRUE(sawUtilityPop, "utility action frame should return through normal pop semantics");
    ASSERT_TRUE(!run.dirtySlotsByTick.empty(), "utility actions should still produce normal dirty slot evidence");
    ASSERT_TRUE(!run.visibleMailboxByTick.empty(), "utility run should still produce bounded mailbox snapshots each tick");
    ASSERT_EQUAL(0, static_cast<int>(run.visibleMailboxByTick[0].size()), "utility without mailbox input should preserve mailbox emptiness");
}
