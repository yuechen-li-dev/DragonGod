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

    [[nodiscard]] std::string ControlToString(dragongod::FrameControl control)
    {
        if (control == dragongod::FrameControl::Continue) {
            return "continue";
        }

        if (control == dragongod::FrameControl::Wait) {
            return "wait";
        }

        if (control == dragongod::FrameControl::Push) {
            return "push";
        }

        if (control == dragongod::FrameControl::Pop) {
            return "pop";
        }

        if (control == dragongod::FrameControl::Replace) {
            return "replace";
        }

        if (control == dragongod::FrameControl::Complete) {
            return "complete";
        }

        return "fail";
    }

    [[nodiscard]] std::string FrameKindToString(dragongod::FrameKind kind)
    {
        if (kind == dragongod::FrameKind::RootPushChild) {
            return "root_push_child";
        }

        if (kind == dragongod::FrameKind::ChildComplete) {
            return "child_complete";
        }

        if (kind == dragongod::FrameKind::RootReplace) {
            return "root_replace";
        }

        if (kind == dragongod::FrameKind::ReplacementComplete) {
            return "replacement_complete";
        }

        if (kind == dragongod::FrameKind::RootWaitThenPush) {
            return "root_wait_then_push";
        }

        if (kind == dragongod::FrameKind::RootPushFailingChild) {
            return "root_push_failing_child";
        }

        return "child_fail";
    }

    [[nodiscard]] std::vector<std::string> SerializeTrace(const std::vector<dragongod::FrameTraceEvent>& trace)
    {
        std::vector<std::string> serialized;
        serialized.reserve(trace.size());

        for (const dragongod::FrameTraceEvent& event : trace) {
            serialized.push_back(
                "tick=" + std::to_string(event.tick) +
                ",kind=" + TraceKindToString(event.kind) +
                ",active=" + FrameKindToString(event.activeFrame) +
                ",frame_step=" + std::to_string(event.frameStep) +
                ",control=" + ControlToString(event.control) +
                ",target=" + FrameKindToString(event.targetFrame) +
                ",depth=" + std::to_string(event.stackDepth));
        }

        return serialized;
    }
}

FACT(M1b_PushAndPop_AreDeterministic_AndRestoreParent)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::RuntimeState initialState{
        .scenario = dragongod::StackScriptScenario::PushPopComplete,
        .stack = {}
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(initialState, 10);

    const std::vector<std::string> expectedTrace{
        "tick=0,kind=tick,active=root_push_child,frame_step=0,control=continue,target=root_push_child,depth=1",
        "tick=0,kind=enter,active=root_push_child,frame_step=0,control=continue,target=root_push_child,depth=1",
        "tick=0,kind=step,active=root_push_child,frame_step=1,control=push,target=child_complete,depth=1",
        "tick=0,kind=push,active=root_push_child,frame_step=1,control=push,target=child_complete,depth=1",
        "tick=1,kind=tick,active=child_complete,frame_step=0,control=continue,target=child_complete,depth=2",
        "tick=1,kind=enter,active=child_complete,frame_step=0,control=continue,target=child_complete,depth=2",
        "tick=1,kind=step,active=child_complete,frame_step=1,control=pop,target=child_complete,depth=2",
        "tick=1,kind=exit_completed,active=child_complete,frame_step=1,control=pop,target=child_complete,depth=2",
        "tick=1,kind=pop,active=child_complete,frame_step=1,control=pop,target=child_complete,depth=2",
        "tick=2,kind=tick,active=root_push_child,frame_step=1,control=continue,target=root_push_child,depth=1",
        "tick=2,kind=step,active=root_push_child,frame_step=2,control=complete,target=root_push_child,depth=1",
        "tick=2,kind=exit_completed,active=root_push_child,frame_step=2,control=complete,target=root_push_child,depth=1",
        "tick=2,kind=pop,active=root_push_child,frame_step=2,control=complete,target=root_push_child,depth=1",
        "tick=2,kind=terminal_completed,active=root_push_child,frame_step=2,control=complete,target=root_push_child,depth=0"
    };

    ASSERT_SEQUENCE_EQUAL(expectedTrace, SerializeTrace(run.trace), "push then child pop should return control to root before root completion");
    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "push-pop scenario should terminate with completion");
}

FACT(M1b_Replace_SwapsTopFrame_WithoutGhostState)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::RuntimeState initialState{
        .scenario = dragongod::StackScriptScenario::ReplaceComplete,
        .stack = {}
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(initialState, 10);
    const std::vector<std::string> serializedTrace = SerializeTrace(run.trace);

    int rootEnterCount = 0;
    int replacementEnterCount = 0;
    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind == dragongod::FrameTraceKind::Enter && event.activeFrame == dragongod::FrameKind::RootReplace) {
            ++rootEnterCount;
        }

        if (event.kind == dragongod::FrameTraceKind::Enter && event.activeFrame == dragongod::FrameKind::ReplacementComplete) {
            ++replacementEnterCount;
        }
    }

    const std::vector<std::string> expectedTrace{
        "tick=0,kind=tick,active=root_replace,frame_step=0,control=continue,target=root_replace,depth=1",
        "tick=0,kind=enter,active=root_replace,frame_step=0,control=continue,target=root_replace,depth=1",
        "tick=0,kind=step,active=root_replace,frame_step=1,control=replace,target=replacement_complete,depth=1",
        "tick=0,kind=replace,active=root_replace,frame_step=1,control=replace,target=replacement_complete,depth=1",
        "tick=1,kind=tick,active=replacement_complete,frame_step=0,control=continue,target=replacement_complete,depth=1",
        "tick=1,kind=enter,active=replacement_complete,frame_step=0,control=continue,target=replacement_complete,depth=1",
        "tick=1,kind=step,active=replacement_complete,frame_step=1,control=complete,target=replacement_complete,depth=1",
        "tick=1,kind=exit_completed,active=replacement_complete,frame_step=1,control=complete,target=replacement_complete,depth=1",
        "tick=1,kind=pop,active=replacement_complete,frame_step=1,control=complete,target=replacement_complete,depth=1",
        "tick=1,kind=terminal_completed,active=replacement_complete,frame_step=1,control=complete,target=replacement_complete,depth=0"
    };

    ASSERT_EQUAL(1, rootEnterCount, "replaced root frame should enter exactly once");
    ASSERT_EQUAL(1, replacementEnterCount, "replacement frame should enter exactly once");
    ASSERT_SEQUENCE_EQUAL(expectedTrace, serializedTrace, "replace should remove prior active frame and activate replacement cleanly");
    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "replace scenario should complete");
}

FACT(M1b_Wait_IsNonTerminal_AndDeterministic)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::RuntimeState initialState{
        .scenario = dragongod::StackScriptScenario::WaitPushPopComplete,
        .stack = {}
    };

    const dragongod::FrameRunResult shortRun = runtime.RunForTicks(initialState, 1);
    const dragongod::FrameRunResult fullRun = runtime.RunForTicks(initialState, 10);

    ASSERT_TRUE(shortRun.finalOutcome == dragongod::StackRunOutcome::Wait, "first tick should wait without becoming terminal");
    ASSERT_EQUAL(static_cast<std::size_t>(3), shortRun.trace.size(), "single wait tick should emit tick, enter, step");
    ASSERT_TRUE(fullRun.finalOutcome == dragongod::StackRunOutcome::Completed, "wait-then-push scenario should eventually complete");
}

FACT(M1b_ChildFailure_IsTerminal_AndStopsStackProgression)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::RuntimeState initialState{
        .scenario = dragongod::StackScriptScenario::PushChildFail,
        .stack = {}
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(initialState, 10);

    const std::vector<std::string> expectedTrace{
        "tick=0,kind=tick,active=root_push_failing_child,frame_step=0,control=continue,target=root_push_failing_child,depth=1",
        "tick=0,kind=enter,active=root_push_failing_child,frame_step=0,control=continue,target=root_push_failing_child,depth=1",
        "tick=0,kind=step,active=root_push_failing_child,frame_step=1,control=push,target=child_fail,depth=1",
        "tick=0,kind=push,active=root_push_failing_child,frame_step=1,control=push,target=child_fail,depth=1",
        "tick=1,kind=tick,active=child_fail,frame_step=0,control=continue,target=child_fail,depth=2",
        "tick=1,kind=enter,active=child_fail,frame_step=0,control=continue,target=child_fail,depth=2",
        "tick=1,kind=step,active=child_fail,frame_step=1,control=fail,target=child_fail,depth=2",
        "tick=1,kind=exit_failed,active=child_fail,frame_step=1,control=fail,target=child_fail,depth=2",
        "tick=1,kind=terminal_failed,active=child_fail,frame_step=1,control=fail,target=child_fail,depth=2"
    };

    ASSERT_SEQUENCE_EQUAL(expectedTrace, SerializeTrace(run.trace), "child fail should terminate stack immediately without ghost parent resume");
    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Failed, "failure scenario should terminate as failed");
}

FACT(M1b_RepeatedRuns_WithSameInputs_HaveNoTraceDrift)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::RuntimeState initialState{
        .scenario = dragongod::StackScriptScenario::WaitPushPopComplete,
        .stack = {}
    };

    const dragongod::FrameRunResult firstRun = runtime.RunForTicks(initialState, 10);
    const dragongod::FrameRunResult secondRun = runtime.RunForTicks(initialState, 10);

    ASSERT_TRUE(firstRun.finalOutcome == secondRun.finalOutcome, "deterministic runs must match final outcome");
    ASSERT_EQUAL(firstRun.trace.size(), secondRun.trace.size(), "deterministic runs must match trace length");
    ASSERT_SEQUENCE_EQUAL(SerializeTrace(firstRun.trace), SerializeTrace(secondRun.trace), "deterministic runs must match ordered trace exactly");
}
