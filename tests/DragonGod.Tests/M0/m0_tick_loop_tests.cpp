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

        return "completed";
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

FACT(M0a_RepeatedRuns_WithSameInputs_ProduceSameTraceAndOutcome)
{
    const dragongod::Runtime runtime;
    const dragongod::AgentRuntimeState initialState{
        .counter = 0,
        .completionCounter = 4
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

FACT(M0a_TickProgression_IsExplicit_AndTerminalOutcomeIsBounded)
{
    const dragongod::Runtime runtime;
    const dragongod::AgentRuntimeState initialState{
        .counter = 0,
        .completionCounter = 4
    };

    const dragongod::RunResult run = runtime.RunForTicks(initialState, 10);

    const std::vector<std::string> expectedTrace{
        "tick=0,counter=1,outcome=continue",
        "tick=1,counter=2,outcome=continue",
        "tick=2,counter=3,outcome=continue",
        "tick=3,counter=4,outcome=completed"
    };

    const std::vector<std::string> actualTrace = SerializeTrace(run.trace);

    ASSERT_SEQUENCE_EQUAL(expectedTrace, actualTrace, "trace should expose discrete tick progression and bounded terminal transition");
    ASSERT_TRUE(run.finalOutcome == dragongod::StepOutcome::Completed, "final outcome should be completed for this deterministic scenario");
}
