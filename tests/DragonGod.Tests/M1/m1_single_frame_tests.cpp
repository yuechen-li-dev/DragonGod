#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/runtime_compat.h"

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

        if (id == dragongod::FrameId::ChildPop) {
            return "child_pop";
        }

        if (id == dragongod::FrameId::ChildFail) {
            return "child_fail";
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

FACT(M1c_CanonicalFrames_ExecutePushPopAndRestoreParent)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::PushPopComplete, 10);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "push-pop scenario should terminate with completion");

    bool sawPush = false;
    bool sawChildPop = false;
    bool sawRootComplete = false;
    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind == dragongod::FrameTraceKind::Push &&
            event.activeFrame == dragongod::FrameId::RootPushChild &&
            event.targetFrame == dragongod::FrameId::ChildPop) {
            sawPush = true;
        }

        if (event.kind == dragongod::FrameTraceKind::Pop &&
            event.activeFrame == dragongod::FrameId::ChildPop) {
            sawChildPop = true;
        }

        if (event.kind == dragongod::FrameTraceKind::TerminalCompleted &&
            event.activeFrame == dragongod::FrameId::RootPushChild) {
            sawRootComplete = true;
        }
    }

    ASSERT_TRUE(sawPush, "canonical root frame should push child frame");
    ASSERT_TRUE(sawChildPop, "child frame should pop and return control");
    ASSERT_TRUE(sawRootComplete, "root frame should complete after child returns");
}

FACT(M1c_Replace_SwapsTopFrameWithoutGhostState)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::ReplaceComplete, 10);

    int rootEnterCount = 0;
    int replacementEnterCount = 0;
    bool sawReplace = false;

    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind == dragongod::FrameTraceKind::Enter &&
            event.activeFrame == dragongod::FrameId::RootReplace) {
            ++rootEnterCount;
        }

        if (event.kind == dragongod::FrameTraceKind::Enter &&
            event.activeFrame == dragongod::FrameId::RecoveryComplete) {
            ++replacementEnterCount;
        }

        if (event.kind == dragongod::FrameTraceKind::Replace &&
            event.activeFrame == dragongod::FrameId::RootReplace &&
            event.targetFrame == dragongod::FrameId::RecoveryComplete) {
            sawReplace = true;
        }
    }

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "replace scenario should complete");
    ASSERT_EQUAL(1, rootEnterCount, "replaced frame should enter exactly once");
    ASSERT_EQUAL(1, replacementEnterCount, "replacement frame should enter exactly once");
    ASSERT_TRUE(sawReplace, "replace control should swap active frame to recovery");
}

FACT(M1c_ProgramCounterResumption_IsRealAfterWaitAndReturn)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::WaitPushPopComplete, 10);

    bool sawWaitOnPc0 = false;
    bool sawPushOnPc1 = false;
    bool sawCompleteOnPc2 = false;

    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind != dragongod::FrameTraceKind::Step ||
            event.activeFrame != dragongod::FrameId::RootWaitThenPush) {
            continue;
        }

        if (event.control == dragongod::FrameControlKind::Wait && event.framePc == 0) {
            sawWaitOnPc0 = true;
        }

        if (event.control == dragongod::FrameControlKind::Push && event.framePc == 1) {
            sawPushOnPc1 = true;
        }

        if (event.control == dragongod::FrameControlKind::Complete && event.framePc == 2) {
            sawCompleteOnPc2 = true;
        }
    }

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "wait-push scenario should eventually complete");
    ASSERT_TRUE(sawWaitOnPc0, "frame should wait while at pc=0");
    ASSERT_TRUE(sawPushOnPc1, "frame should resume at pc=1 and push child");
    ASSERT_TRUE(sawCompleteOnPc2, "frame should resume at pc=2 after child pop and complete");
}

FACT(M1c_EnterSemantics_AreBoundedPerActivation)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::WaitPushPopComplete, 10);

    int rootEnterCount = 0;
    int childEnterCount = 0;

    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind != dragongod::FrameTraceKind::Enter) {
            continue;
        }

        if (event.activeFrame == dragongod::FrameId::RootWaitThenPush) {
            ++rootEnterCount;
        }

        if (event.activeFrame == dragongod::FrameId::ChildPop) {
            ++childEnterCount;
        }
    }

    ASSERT_EQUAL(1, rootEnterCount, "root frame should enter once and then resume via pc");
    ASSERT_EQUAL(1, childEnterCount, "child frame should enter once when pushed");
}

FACT(M1c_ChildFailure_IsTerminal_AndStopsStackProgression)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::PushChildFail, 10);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Failed, "failure scenario should terminate as failed");

    bool sawChildFailStep = false;
    bool sawTerminalFailed = false;
    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind == dragongod::FrameTraceKind::Step &&
            event.activeFrame == dragongod::FrameId::ChildFail &&
            event.control == dragongod::FrameControlKind::Fail) {
            sawChildFailStep = true;
        }

        if (event.kind == dragongod::FrameTraceKind::TerminalFailed &&
            event.activeFrame == dragongod::FrameId::ChildFail) {
            sawTerminalFailed = true;
        }
    }

    ASSERT_TRUE(sawChildFailStep, "child fail frame should issue fail control");
    ASSERT_TRUE(sawTerminalFailed, "runtime should emit terminal failed when child fails");
}

FACT(M1c_CanonicalVerbs_IncludeContinue)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(dragongod::StackScriptScenario::ContinueThenComplete, 10);

    bool sawContinueOnPc0 = false;
    bool sawCompleteOnPc1 = false;
    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind != dragongod::FrameTraceKind::Step ||
            event.activeFrame != dragongod::FrameId::RootContinueThenComplete) {
            continue;
        }

        if (event.control == dragongod::FrameControlKind::Continue && event.framePc == 0) {
            sawContinueOnPc0 = true;
        }

        if (event.control == dragongod::FrameControlKind::Complete && event.framePc == 1) {
            sawCompleteOnPc1 = true;
        }
    }

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "continue scenario should complete");
    ASSERT_TRUE(sawContinueOnPc0, "frame should use continue control from pc=0");
    ASSERT_TRUE(sawCompleteOnPc1, "frame should resume on pc=1 and complete");
}

FACT(M1c_RepeatedRuns_WithSameInputs_HaveNoTraceDrift)
{
    const dragongod::StackFrameRuntime runtime;

    const dragongod::FrameRunResult firstRun = runtime.RunForTicks(dragongod::StackScriptScenario::WaitPushPopComplete, 10);
    const dragongod::FrameRunResult secondRun = runtime.RunForTicks(dragongod::StackScriptScenario::WaitPushPopComplete, 10);

    ASSERT_TRUE(firstRun.finalOutcome == secondRun.finalOutcome, "deterministic runs must match final outcome");
    ASSERT_EQUAL(firstRun.trace.size(), secondRun.trace.size(), "deterministic runs must match trace length");
    ASSERT_SEQUENCE_EQUAL(SerializeTrace(firstRun.trace), SerializeTrace(secondRun.trace), "deterministic runs must match ordered trace exactly");
}

namespace
{
    [[nodiscard]] dragongod::FrameControl AuthorRootContinueThenComplete(dragongod::FrameCtx& ctx)
    {
        switch (ctx.Pc()) {
        case 0:
            return dragongod::Dg::Continue(1);
        case 1:
            return dragongod::Dg::Complete();
        default:
            return dragongod::Dg::Fail(9500);
        }
    }
}

FACT(M16a_PublicRegistrySurface_AllowsAuthorOwnedRegistryAtSessionConstruction)
{
    dragongod::FrameRegistry registry;
    registry.Add(dragongod::FrameId::UtilityActionFallback, &AuthorRootContinueThenComplete);

    dragongod::StackFrameRuntimeSession session(dragongod::StackFrameSessionInit{
        .registry = registry,
        .rootFrame = dragongod::FrameId::UtilityActionFallback,
        .mailboxInput = {}
    });

    const dragongod::FrameRunResult run = session.RunForTicks(4);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "author-owned registry/root should execute to completion");
}

FACT(M16a_PublicRegistrySurface_AllowsAuthorOwnedRegistryOnRestore)
{
    dragongod::FrameRegistry registry;
    registry.Add(dragongod::FrameId::UtilityActionFallback, &AuthorRootContinueThenComplete);

    dragongod::StackFrameRuntimeSession initial(dragongod::StackFrameSessionInit{
        .registry = registry,
        .rootFrame = dragongod::FrameId::UtilityActionFallback,
        .mailboxInput = {}
    });

    const dragongod::FrameRunResult legA = initial.RunForTicks(1);
    const dragongod::RuntimeChunk snapshot = initial.Save();

    dragongod::StackFrameRuntimeSession restored(snapshot, registry);
    const dragongod::FrameRunResult legB = restored.RunForTicks(4);

    ASSERT_TRUE(legA.finalOutcome == dragongod::StackRunOutcome::Continue, "first leg should stop after continue phase");
    ASSERT_TRUE(legB.finalOutcome == dragongod::StackRunOutcome::Completed, "restored session should complete with caller-provided registry");
}
