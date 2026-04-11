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

        if (kind == dragongod::FrameTraceKind::ExitCompleted) {
            return "exit_completed";
        }

        return "exit_failed";
    }

    [[nodiscard]] std::string OutcomeToString(dragongod::FrameStepOutcome outcome)
    {
        if (outcome == dragongod::FrameStepOutcome::Continue) {
            return "continue";
        }

        if (outcome == dragongod::FrameStepOutcome::Wait) {
            return "wait";
        }

        if (outcome == dragongod::FrameStepOutcome::Completed) {
            return "completed";
        }

        return "failed";
    }

    [[nodiscard]] std::vector<std::string> SerializeTrace(const std::vector<dragongod::FrameTraceEvent>& trace)
    {
        std::vector<std::string> serialized;
        serialized.reserve(trace.size());

        for (const dragongod::FrameTraceEvent& event : trace) {
            serialized.push_back(
                "tick=" + std::to_string(event.tick) +
                ",kind=" + TraceKindToString(event.kind) +
                ",frame_step=" + std::to_string(event.frameStep) +
                ",outcome=" + OutcomeToString(event.outcome));
        }

        return serialized;
    }
}

FACT(M1a_FrameEnter_HappensExactlyOnce_AndProgressesToCompleted)
{
    const dragongod::SingleFrameRuntime runtime;
    const dragongod::RuntimeState initialState{
        .scenario = dragongod::FrameScriptScenario::EnterWaitThenComplete,
        .waitAtStep = 1,
        .waitConsumed = false,
        .completeAtStep = 3,
        .failAtStep = 2,
        .frame = dragongod::SingleFrameState{}
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(initialState, 10);
    const std::vector<std::string> serializedTrace = SerializeTrace(run.trace);

    int enterCount = 0;
    for (const dragongod::FrameTraceEvent& event : run.trace) {
        if (event.kind == dragongod::FrameTraceKind::Enter) {
            ++enterCount;
        }
    }

    const std::vector<std::string> expectedTrace{
        "tick=0,kind=tick,frame_step=0,outcome=continue",
        "tick=0,kind=enter,frame_step=0,outcome=continue",
        "tick=0,kind=step,frame_step=1,outcome=continue",
        "tick=1,kind=tick,frame_step=1,outcome=continue",
        "tick=1,kind=step,frame_step=1,outcome=wait",
        "tick=2,kind=tick,frame_step=1,outcome=continue",
        "tick=2,kind=step,frame_step=2,outcome=continue",
        "tick=3,kind=tick,frame_step=2,outcome=continue",
        "tick=3,kind=step,frame_step=3,outcome=completed",
        "tick=3,kind=exit_completed,frame_step=3,outcome=completed"
    };

    ASSERT_EQUAL(1, enterCount, "enter should be emitted exactly once for one active frame");
    ASSERT_SEQUENCE_EQUAL(expectedTrace, serializedTrace, "single-frame lifecycle should be explicit and ordered");
    ASSERT_TRUE(run.finalOutcome == dragongod::FrameStepOutcome::Completed, "scenario should exit with completed");
}

FACT(M1a_FrameWait_IsNonTerminal_WithinTickBudget)
{
    const dragongod::SingleFrameRuntime runtime;
    const dragongod::RuntimeState initialState{
        .scenario = dragongod::FrameScriptScenario::EnterWaitThenComplete,
        .waitAtStep = 1,
        .waitConsumed = false,
        .completeAtStep = 3,
        .failAtStep = 2,
        .frame = dragongod::SingleFrameState{}
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(initialState, 2);

    ASSERT_EQUAL(static_cast<std::size_t>(5), run.trace.size(), "two ticks should emit tick/enter/step then tick/step");
    ASSERT_TRUE(run.trace[4].kind == dragongod::FrameTraceKind::Step, "second tick should emit step event");
    ASSERT_TRUE(run.trace[4].outcome == dragongod::FrameStepOutcome::Wait, "second tick should be deterministic wait");
    ASSERT_TRUE(run.finalOutcome == dragongod::FrameStepOutcome::Wait, "wait should remain non-terminal when tick budget ends");
}

FACT(M1a_FailedExit_IsTerminal_AndStopsFurtherProgression)
{
    const dragongod::SingleFrameRuntime runtime;
    const dragongod::RuntimeState initialState{
        .scenario = dragongod::FrameScriptScenario::EnterThenFail,
        .waitAtStep = 1,
        .waitConsumed = false,
        .completeAtStep = 3,
        .failAtStep = 2,
        .frame = dragongod::SingleFrameState{}
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(initialState, 100);

    const std::vector<std::string> expectedTrace{
        "tick=0,kind=tick,frame_step=0,outcome=continue",
        "tick=0,kind=enter,frame_step=0,outcome=continue",
        "tick=0,kind=step,frame_step=1,outcome=continue",
        "tick=1,kind=tick,frame_step=1,outcome=continue",
        "tick=1,kind=step,frame_step=2,outcome=failed",
        "tick=1,kind=exit_failed,frame_step=2,outcome=failed"
    };

    ASSERT_SEQUENCE_EQUAL(expectedTrace, SerializeTrace(run.trace), "failed lifecycle should emit explicit failed exit and stop");
    ASSERT_TRUE(run.finalOutcome == dragongod::FrameStepOutcome::Failed, "scenario should exit with failed");
}

FACT(M1a_RepeatedRuns_WithSameInputs_HaveNoHiddenDrift)
{
    const dragongod::SingleFrameRuntime runtime;
    const dragongod::RuntimeState initialState{
        .scenario = dragongod::FrameScriptScenario::EnterWaitThenComplete,
        .waitAtStep = 1,
        .waitConsumed = false,
        .completeAtStep = 3,
        .failAtStep = 2,
        .frame = dragongod::SingleFrameState{}
    };

    const dragongod::FrameRunResult firstRun = runtime.RunForTicks(initialState, 10);
    const dragongod::FrameRunResult secondRun = runtime.RunForTicks(initialState, 10);

    ASSERT_TRUE(firstRun.finalOutcome == secondRun.finalOutcome, "final outcomes should match across repeated deterministic runs");
    ASSERT_EQUAL(firstRun.trace.size(), secondRun.trace.size(), "trace lengths should match across repeated deterministic runs");
    ASSERT_SEQUENCE_EQUAL(
        SerializeTrace(firstRun.trace),
        SerializeTrace(secondRun.trace),
        "ordered lifecycle trace should match exactly across repeated deterministic runs");
}
