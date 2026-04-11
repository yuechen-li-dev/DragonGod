#include "tick_loop.h"

namespace dragongod
{
    [[nodiscard]] RunResult Runtime::RunForTicks(AgentRuntimeState initialState, TickIndex tickCount) const
    {
        RunResult result;

        for (TickIndex tick = 0; tick < tickCount; ++tick) {
            const StepOutcome outcome = StepAgent(initialState);
            result.trace.push_back(TraceEvent{
                .tick = tick,
                .counter = initialState.counter,
                .outcome = outcome
            });
            result.finalOutcome = outcome;

            if (IsTerminal(outcome)) {
                break;
            }
        }

        return result;
    }

    [[nodiscard]] StepOutcome Runtime::StepAgent(AgentRuntimeState& state)
    {
        if (state.scenario == ScriptScenario::ContinueWaitComplete) {
            if (!state.waitConsumed && state.counter == state.waitCounter) {
                state.waitConsumed = true;
                return StepOutcome::Wait;
            }

            ++state.counter;
            if (state.counter >= state.completionCounter) {
                return StepOutcome::Completed;
            }

            return StepOutcome::Continue;
        }

        ++state.counter;
        if (state.counter >= state.failureCounter) {
            return StepOutcome::Failed;
        }

        return StepOutcome::Continue;
    }

    [[nodiscard]] bool Runtime::IsTerminal(StepOutcome outcome)
    {
        return outcome == StepOutcome::Completed || outcome == StepOutcome::Failed;
    }
}
