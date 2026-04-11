#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/m1_single_frame.h"

#include <string>
#include <vector>

namespace
{
    [[nodiscard]] std::string TraceKindToString(dragongod::FrameTraceKind kind)
    {
        if (kind == dragongod::FrameTraceKind::Tick) {
            return "tick";
        }

        if (kind == dragongod::FrameTraceKind::Enter) {
            return "enter";
        }

        if (kind == dragongod::FrameTraceKind::Step) {
            return "step";
        }

        if (kind == dragongod::FrameTraceKind::Push) {
            return "push";
        }

        if (kind == dragongod::FrameTraceKind::Pop) {
            return "pop";
        }

        if (kind == dragongod::FrameTraceKind::Replace) {
            return "replace";
        }

        if (kind == dragongod::FrameTraceKind::ExitCompleted) {
            return "exit_completed";
        }

        if (kind == dragongod::FrameTraceKind::ExitFailed) {
            return "exit_failed";
        }

        if (kind == dragongod::FrameTraceKind::TerminalCompleted) {
            return "terminal_completed";
        }

        return "terminal_failed";
    }

    [[nodiscard]] std::string ControlToString(dragongod::FrameControlKind control)
    {
        if (control == dragongod::FrameControlKind::Continue) {
            return "continue";
        }

        if (control == dragongod::FrameControlKind::Wait) {
            return "wait";
        }

        if (control == dragongod::FrameControlKind::Push) {
            return "push";
        }

        if (control == dragongod::FrameControlKind::Pop) {
            return "pop";
        }

        if (control == dragongod::FrameControlKind::Replace) {
            return "replace";
        }

        if (control == dragongod::FrameControlKind::Complete) {
            return "complete";
        }

        return "fail";
    }

    [[nodiscard]] std::string FrameIdToString(dragongod::FrameId id)
    {
        if (id == dragongod::FrameId::RootPushChild) {
            return "root_push_child";
        }

        if (id == dragongod::FrameId::RootReplace) {
            return "root_replace";
        }

        if (id == dragongod::FrameId::RootWaitThenPush) {
            return "root_wait_then_push";
        }

        if (id == dragongod::FrameId::RootPushFailingChild) {
            return "root_push_failing_child";
        }

        if (id == dragongod::FrameId::RootContinueThenComplete) {
            return "root_continue_then_complete";
        }

        if (id == dragongod::FrameId::RootSetThenReadBlackboard) {
            return "root_set_then_read_blackboard";
        }

        if (id == dragongod::FrameId::RootFallbackBranch) {
            return "root_fallback_branch";
        }

        if (id == dragongod::FrameId::RootParentChildBlackboard) {
            return "root_parent_child_blackboard";
        }

        if (id == dragongod::FrameId::ChildPop) {
            return "child_pop";
        }

        if (id == dragongod::FrameId::ChildFail) {
            return "child_fail";
        }

        if (id == dragongod::FrameId::ChildReadParentBool) {
            return "child_read_parent_bool";
        }

        if (id == dragongod::FrameId::ChildWriteParentCounter) {
            return "child_write_parent_counter";
        }

        return "recovery_complete";
    }

    [[nodiscard]] std::vector<std::string> SerializeTrace(const std::vector<dragongod::FrameTraceEvent>& trace)
    {
        std::vector<std::string> serialized;
        serialized.reserve(trace.size());

        for (const dragongod::FrameTraceEvent& event : trace) {
            serialized.push_back(
                "tick=" + std::to_string(event.tick) +
                ",kind=" + TraceKindToString(event.kind) +
                ",active=" + FrameIdToString(event.activeFrame) +
                ",pc=" + std::to_string(event.framePc) +
                ",control=" + ControlToString(event.control) +
                ",target=" + FrameIdToString(event.targetFrame) +
                ",depth=" + std::to_string(event.stackDepth));
        }

        return serialized;
    }
}

FACT(M2a_Blackboard_TypedSetAndRead_WorksInsideCanonicalFrames)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardSetReadComplete, 8);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "set-then-read blackboard scenario should complete");

    bool sawContinueAtPc0 = false;
    bool sawCompleteAtPc1 = false;
    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind != dragongod::FrameTraceKind::Step ||
            event.activeFrame != dragongod::FrameId::RootSetThenReadBlackboard) {
            continue;
        }

        if (event.control == dragongod::FrameControlKind::Continue && event.framePc == 0) {
            sawContinueAtPc0 = true;
        }

        if (event.control == dragongod::FrameControlKind::Complete && event.framePc == 1) {
            sawCompleteAtPc1 = true;
        }
    }

    ASSERT_TRUE(sawContinueAtPc0, "pc=0 should set blackboard state and continue");
    ASSERT_TRUE(sawCompleteAtPc1, "pc=1 should read blackboard state and complete");
}

FACT(M2a_Blackboard_GetOrFallback_IsDeterministicForMissingKey)
{
    const dragongod::StackFrameRuntime runtime;

    const dragongod::FrameRunResult firstRun = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardFallbackComplete, 8);
    const dragongod::FrameRunResult secondRun = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardFallbackComplete, 8);

    ASSERT_TRUE(firstRun.finalOutcome == dragongod::StackRunOutcome::Completed, "fallback scenario should complete");
    ASSERT_TRUE(secondRun.finalOutcome == dragongod::StackRunOutcome::Completed, "fallback scenario should complete on repeated run");
    ASSERT_SEQUENCE_EQUAL(SerializeTrace(firstRun.trace), SerializeTrace(secondRun.trace), "missing-key fallback path should have deterministic ordered trace");
}

FACT(M2a_Blackboard_StateInfluencesControlFlowAcrossParentAndChild)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardParentChildComplete, 16);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "parent-child blackboard scenario should complete");

    bool sawChildReadPop = false;
    bool sawChildWritePop = false;
    bool sawRootCompleteAtPc2 = false;

    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind == dragongod::FrameTraceKind::Step &&
            event.activeFrame == dragongod::FrameId::ChildReadParentBool &&
            event.control == dragongod::FrameControlKind::Pop) {
            sawChildReadPop = true;
        }

        if (event.kind == dragongod::FrameTraceKind::Step &&
            event.activeFrame == dragongod::FrameId::ChildWriteParentCounter &&
            event.control == dragongod::FrameControlKind::Pop) {
            sawChildWritePop = true;
        }

        if (event.kind == dragongod::FrameTraceKind::Step &&
            event.activeFrame == dragongod::FrameId::RootParentChildBlackboard &&
            event.framePc == 2 &&
            event.control == dragongod::FrameControlKind::Complete) {
            sawRootCompleteAtPc2 = true;
        }
    }

    ASSERT_TRUE(sawChildReadPop, "first child should read parent blackboard value and pop");
    ASSERT_TRUE(sawChildWritePop, "second child should write blackboard value and pop");
    ASSERT_TRUE(sawRootCompleteAtPc2, "parent should branch on blackboard value and complete at pc=2");
}

FACT(M2a_Blackboard_RepeatedRuns_WithSameInputs_HaveNoTraceDrift)
{
    const dragongod::StackFrameRuntime runtime;

    const dragongod::FrameRunResult firstRun = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardParentChildComplete, 16);
    const dragongod::FrameRunResult secondRun = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardParentChildComplete, 16);

    ASSERT_TRUE(firstRun.finalOutcome == secondRun.finalOutcome, "deterministic blackboard runs must match final outcome");
    ASSERT_EQUAL(firstRun.trace.size(), secondRun.trace.size(), "deterministic blackboard runs must match trace length");
    ASSERT_SEQUENCE_EQUAL(SerializeTrace(firstRun.trace), SerializeTrace(secondRun.trace), "deterministic blackboard runs must match ordered trace exactly");
}
