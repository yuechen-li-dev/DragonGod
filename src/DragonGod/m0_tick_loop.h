#pragma once

#include <cstddef>
#include <vector>

namespace dragongod
{
    using TickIndex = std::size_t;

    enum class StepOutcome
    {
        Continue,
        Wait,
        Completed,
        Failed
    };

    enum class ScriptScenario
    {
        ContinueWaitComplete,
        ContinueThenFail
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
        ScriptScenario scenario = ScriptScenario::ContinueWaitComplete;
        int counter = 0;
        int completionCounter = 4;
        int waitCounter = 2;
        bool waitConsumed = false;
        int failureCounter = 3;
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
        [[nodiscard]] static bool IsTerminal(StepOutcome outcome);
    };
}
