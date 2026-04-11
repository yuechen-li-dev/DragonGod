#include "m0_tick_loop.h"

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

            if (outcome == StepOutcome::Completed) {
                break;
            }
        }

        return result;
    }

    [[nodiscard]] StepOutcome Runtime::StepAgent(AgentRuntimeState& state)
    {
        ++state.counter;

        if (state.counter >= state.completionCounter) {
            return StepOutcome::Completed;
        }

        return StepOutcome::Continue;
    }
}
