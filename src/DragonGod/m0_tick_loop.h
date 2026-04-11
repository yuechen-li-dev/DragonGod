#pragma once

#include <cstddef>
#include <vector>

namespace dragongod
{
    using TickIndex = std::size_t;

    enum class StepOutcome
    {
        Continue,
        Completed
    };

    struct TraceEvent
    {
        TickIndex tick = 0;
        int counter = 0;
        StepOutcome outcome = StepOutcome::Continue;

        [[nodiscard]] bool operator==(const TraceEvent& other) const = default;
    };

    struct AgentRuntimeState
    {
        int counter = 0;
        int completionCounter = 4;
    };

    struct [[nodiscard]] RunResult
    {
        StepOutcome finalOutcome = StepOutcome::Continue;
        std::vector<TraceEvent> trace;
    };

    class Runtime
    {
    public:
        [[nodiscard]] RunResult RunForTicks(AgentRuntimeState initialState, TickIndex tickCount) const;

    private:
        [[nodiscard]] static StepOutcome StepAgent(AgentRuntimeState& state);
    };
}
