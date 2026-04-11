#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/m0_tick_loop.h"

#include <string>
#include <vector>

namespace
{
    [[nodiscard]] std::string OutcomeToString(dragongod::StepOutcome outcome)
    {
        if (outcome == dragongod::StepOutcome::Continue) {
            return "continue";
        }

        if (outcome == dragongod::StepOutcome::Wait) {
            return "wait";
        }

        if (outcome == dragongod::StepOutcome::Completed) {
            return "completed";
        }

        return "failed";
    }

    [[nodiscard]] std::vector<std::string> SerializeTrace(const std::vector<dragongod::TraceEvent>& trace)
    {
        std::vector<std::string> serialized;
        serialized.reserve(trace.size());

        for (const dragongod::TraceEvent& event : trace) {
            serialized.push_back(
                "tick=" + std::to_string(event.tick) +
                ",counter=" + std::to_string(event.counter) +
                ",outcome=" + OutcomeToString(event.outcome));
        }

        return serialized;
    }
}

FACT(M0b_ContinueWaitAndTerminalOutcomes_AreDistinctAndDeterministic)
{
    const dragongod::Runtime runtime;
    const dragongod::AgentRuntimeState initialState{
        .scenario = dragongod::ScriptScenario::ContinueWaitComplete,
        .counter = 0,
        .completionCounter = 4,
        .waitCounter = 2,
        .waitConsumed = false,
        .failureCounter = 3
    };

    const dragongod::RunResult run = runtime.RunForTicks(initialState, 10);

    const std::vector<std::string> expectedTrace{
        "tick=0,counter=1,outcome=continue",
        "tick=1,counter=2,outcome=continue",
        "tick=2,counter=2,outcome=wait",
        "tick=3,counter=3,outcome=continue",
        "tick=4,counter=4,outcome=completed"
    };

    ASSERT_SEQUENCE_EQUAL(expectedTrace, SerializeTrace(run.trace), "continue, wait, and completed outcomes should be explicit in ordered trace");
    ASSERT_TRUE(run.finalOutcome == dragongod::StepOutcome::Completed, "final outcome should be completed for continue-wait-complete scenario");
}

FACT(M0b_WaitIsNonTerminal_AndDoesNotEndExecution)
{
    const dragongod::Runtime runtime;
    const dragongod::AgentRuntimeState initialState{
        .scenario = dragongod::ScriptScenario::ContinueWaitComplete,
        .counter = 0,
        .completionCounter = 4,
        .waitCounter = 2,
        .waitConsumed = false,
        .failureCounter = 3
    };

    const dragongod::RunResult run = runtime.RunForTicks(initialState, 3);

    ASSERT_EQUAL(static_cast<std::size_t>(3), run.trace.size(), "three ticks should be processed when budget is three");
    ASSERT_TRUE(run.trace[2].outcome == dragongod::StepOutcome::Wait, "third tick should be the configured wait");
    ASSERT_TRUE(run.finalOutcome == dragongod::StepOutcome::Wait, "wait should be observable as a non-terminal final outcome when tick budget ends");
}

FACT(M0b_CompletedIsTerminal_AndPreventsFurtherTicks)
{
    const dragongod::Runtime runtime;
    const dragongod::AgentRuntimeState initialState{
        .scenario = dragongod::ScriptScenario::ContinueWaitComplete,
        .counter = 0,
        .completionCounter = 4,
        .waitCounter = 2,
        .waitConsumed = false,
        .failureCounter = 3
    };

    const dragongod::RunResult run = runtime.RunForTicks(initialState, 100);

    ASSERT_EQUAL(static_cast<std::size_t>(5), run.trace.size(), "completed should stop progression after terminal tick");
    ASSERT_TRUE(run.trace.back().outcome == dragongod::StepOutcome::Completed, "last trace outcome should be completed");
}

FACT(M0b_FailedIsTerminal_AndPreventsFurtherTicks)
{
    const dragongod::Runtime runtime;
    const dragongod::AgentRuntimeState initialState{
        .scenario = dragongod::ScriptScenario::ContinueThenFail,
        .counter = 0,
        .completionCounter = 4,
        .waitCounter = 2,
        .waitConsumed = false,
        .failureCounter = 3
    };

    const dragongod::RunResult run = runtime.RunForTicks(initialState, 100);

    const std::vector<std::string> expectedTrace{
        "tick=0,counter=1,outcome=continue",
        "tick=1,counter=2,outcome=continue",
        "tick=2,counter=3,outcome=failed"
    };

    ASSERT_SEQUENCE_EQUAL(expectedTrace, SerializeTrace(run.trace), "failed should be explicit and terminal in trace");
    ASSERT_TRUE(run.finalOutcome == dragongod::StepOutcome::Failed, "final outcome should be failed for continue-then-fail scenario");
}

FACT(M0b_RepeatedRuns_WithSameInputs_ProduceSameTraceAndOutcome)
{
    const dragongod::Runtime runtime;
    const dragongod::AgentRuntimeState initialState{
        .scenario = dragongod::ScriptScenario::ContinueWaitComplete,
        .counter = 0,
        .completionCounter = 4,
        .waitCounter = 2,
        .waitConsumed = false,
        .failureCounter = 3
    };

    const dragongod::RunResult firstRun = runtime.RunForTicks(initialState, 10);
    const dragongod::RunResult secondRun = runtime.RunForTicks(initialState, 10);

    ASSERT_TRUE(firstRun.finalOutcome == secondRun.finalOutcome, "final outcomes must match between repeated runs");
    ASSERT_EQUAL(firstRun.trace.size(), secondRun.trace.size(), "trace lengths must match between repeated runs");
    ASSERT_SEQUENCE_EQUAL(
        SerializeTrace(firstRun.trace),
        SerializeTrace(secondRun.trace),
        "ordered structural traces must match between repeated runs");
}
