#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/runtime_compat.h"

#include <optional>
#include <string>
#include <vector>

namespace
{
    namespace Keys
    {
        constexpr dragongod::BbKey<bool> HighSignal{ .name = "HighSignal", .slot = 1 };
        constexpr dragongod::BbKey<bool> ChildSawHighSignal{ .name = "ChildSawHighSignal", .slot = 2 };
        constexpr dragongod::BbKey<int> Counter{ .name = "Counter", .slot = 3 };
    }

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

    [[nodiscard]] bool ContainsSlot(const std::vector<std::uint32_t>& slots, std::uint32_t slot)
    {
        for (const std::uint32_t item : slots) {
            if (item == slot) {
                return true;
            }
        }

        return false;
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

FACT(M2b_Blackboard_SetMarksKeyDirty_WithinCurrentTickBoundary)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardSetReadComplete, 4);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "set/read scenario should complete");
    ASSERT_EQUAL(static_cast<std::size_t>(2), run.dirtySlotsByTick.size(), "scenario should record dirty slots for both executed ticks");
    ASSERT_TRUE(ContainsSlot(run.dirtySlotsByTick[0], Keys::HighSignal.slot), "tick 0 should mark HighSignal dirty after Set");
}

FACT(M2b_Blackboard_UnwrittenKeysStayClean)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardSetReadComplete, 4);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "set/read scenario should complete");
    ASSERT_FALSE(ContainsSlot(run.dirtySlotsByTick[0], Keys::Counter.slot), "Counter should stay clean when not written");
    ASSERT_FALSE(ContainsSlot(run.dirtySlotsByTick[0], Keys::ChildSawHighSignal.slot), "ChildSawHighSignal should stay clean when not written");
}

FACT(M2b_Blackboard_DirtyStateClearsAtStartOfEachTick)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardSetReadComplete, 4);

    ASSERT_EQUAL(static_cast<std::size_t>(2), run.dirtySlotsByTick.size(), "scenario should produce two executed ticks");
    ASSERT_TRUE(ContainsSlot(run.dirtySlotsByTick[0], Keys::HighSignal.slot), "first tick writes HighSignal");
    ASSERT_FALSE(ContainsSlot(run.dirtySlotsByTick[1], Keys::HighSignal.slot), "second tick reads only, so dirty should be cleared at tick start");
}

FACT(M2b_Blackboard_DirtyObservations_AreDeterministicAcrossRuns)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult firstRun = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardParentChildComplete, 16);
    const dragongod::FrameRunResult secondRun = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardParentChildComplete, 16);

    ASSERT_EQUAL(firstRun.dirtySlotsByTick.size(), secondRun.dirtySlotsByTick.size(), "dirty runs should have equal tick counts");
    for (std::size_t i = 0; i < firstRun.dirtySlotsByTick.size(); ++i) {
        ASSERT_SEQUENCE_EQUAL(firstRun.dirtySlotsByTick[i], secondRun.dirtySlotsByTick[i], "dirty slots should match exactly per tick");
    }
}

FACT(M2b_Blackboard_ParentChildWrites_AppearInDirtyTracking)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::BlackboardParentChildComplete, 16);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "parent-child scenario should complete");
    ASSERT_EQUAL(static_cast<std::size_t>(5), run.dirtySlotsByTick.size(), "parent-child scenario should execute five ticks");
    ASSERT_TRUE(ContainsSlot(run.dirtySlotsByTick[0], Keys::HighSignal.slot), "root tick should dirty HighSignal");
    ASSERT_TRUE(ContainsSlot(run.dirtySlotsByTick[0], Keys::Counter.slot), "root tick should dirty Counter");
    ASSERT_TRUE(ContainsSlot(run.dirtySlotsByTick[1], Keys::ChildSawHighSignal.slot), "child read/write tick should dirty ChildSawHighSignal");
    ASSERT_TRUE(ContainsSlot(run.dirtySlotsByTick[3], Keys::Counter.slot), "child counter write tick should dirty Counter");
}

FACT(M2b_Blackboard_ValueReadsRemainCorrect_WithDirtyTrackingEnabled)
{
    dragongod::Blackboard blackboard;
    blackboard.Set(Keys::HighSignal, true);
    blackboard.Set(Keys::Counter, 7);

    ASSERT_TRUE(blackboard.GetOr(Keys::HighSignal, false), "dirty tracking must not alter bool storage/read semantics");
    ASSERT_EQUAL(7, blackboard.GetOr(Keys::Counter, 0), "dirty tracking must not alter int storage/read semantics");
    ASSERT_TRUE(blackboard.IsDirty(Keys::HighSignal), "Set should mark key dirty in direct blackboard usage");
    ASSERT_TRUE(blackboard.IsDirty(Keys::Counter), "Set should mark key dirty in direct blackboard usage");
}

FACT(M17a_Blackboard_SameSlotAndSameKey_DoesNotFlagCollision)
{
    dragongod::Blackboard blackboard;
    constexpr dragongod::BbKey<int> counterA{ .name = "Counter", .slot = 99 };

    blackboard.Set(counterA, 10);
    blackboard.Set(counterA, 42);

    ASSERT_FALSE(blackboard.HasSlotCollision(), "same key metadata reused on same slot should not be flagged as collision");
}

FACT(M17a_Blackboard_SameSlotDifferentName_IsDiagnosed)
{
    dragongod::Blackboard blackboard;
    constexpr dragongod::BbKey<int> counterA{ .name = "CounterA", .slot = 77 };
    constexpr dragongod::BbKey<int> counterB{ .name = "CounterB", .slot = 77 };

    blackboard.Set(counterA, 1);
    blackboard.Set(counterB, 2);

    ASSERT_TRUE(blackboard.HasSlotCollision(), "same slot with conflicting names should be diagnosed");
    const std::optional<dragongod::Blackboard::SlotCollision> collision = blackboard.LastSlotCollision();
    ASSERT_TRUE(collision.has_value(), "collision metadata should be preserved");
    if (!collision.has_value()) {
        return;
    }

    ASSERT_EQUAL(static_cast<std::uint32_t>(77), collision->slot, "collision should report reused slot");
    ASSERT_EQUAL(std::string("CounterA"), std::string(collision->firstName), "collision should report first seen key name");
    ASSERT_EQUAL(std::string("CounterB"), std::string(collision->secondName), "collision should report conflicting key name");
}

FACT(M17a_Blackboard_SameSlotDifferentType_IsDiagnosed)
{
    dragongod::Blackboard blackboard;
    constexpr dragongod::BbKey<bool> boolKey{ .name = "Flag", .slot = 88 };
    constexpr dragongod::BbKey<int> intKey{ .name = "Flag", .slot = 88 };

    blackboard.Set(boolKey, true);
    blackboard.Set(intKey, 9);

    ASSERT_TRUE(blackboard.HasSlotCollision(), "same slot with different value kinds should be diagnosed");
    const std::optional<dragongod::Blackboard::SlotCollision> collision = blackboard.LastSlotCollision();
    ASSERT_TRUE(collision.has_value(), "type-collision metadata should be available");
    if (!collision.has_value()) {
        return;
    }

    ASSERT_TRUE(collision->firstWasBool, "first key kind should identify bool");
    ASSERT_FALSE(collision->secondWasBool, "second key kind should identify int");
}
