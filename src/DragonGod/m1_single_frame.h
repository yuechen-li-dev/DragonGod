#pragma once

#include <cstddef>
#include <vector>

namespace dragongod
{
    using TickIndex = std::size_t;

    enum class FrameStepOutcome
    {
        Continue,
        Wait,
        Completed,
        Failed
    };

    enum class FrameTraceKind
    {
        Tick,
        Enter,
        Step,
        ExitCompleted,
        ExitFailed
    };

    enum class FrameScriptScenario
    {
        EnterWaitThenComplete,
        EnterThenFail
    };

    struct FrameTraceEvent
    {
        TickIndex tick = 0;
        FrameTraceKind kind = FrameTraceKind::Tick;
        int frameStep = 0;
        FrameStepOutcome outcome = FrameStepOutcome::Continue;

        [[nodiscard]] bool operator==(const FrameTraceEvent& other) const = default;
    };

    struct SingleFrameState
    {
        bool entered = false;
        int frameStep = 0;
    };

    struct RuntimeState
    {
        FrameScriptScenario scenario = FrameScriptScenario::EnterWaitThenComplete;
        int waitAtStep = 1;
        bool waitConsumed = false;
        int completeAtStep = 3;
        int failAtStep = 2;
        SingleFrameState frame;
    };

    struct [[nodiscard]] FrameRunResult
    {
        FrameStepOutcome finalOutcome = FrameStepOutcome::Continue;
        std::vector<FrameTraceEvent> trace;
    };

    class SingleFrameRuntime
    {
    public:
        [[nodiscard]] FrameRunResult RunForTicks(RuntimeState initialState, TickIndex tickCount) const;

    private:
        [[nodiscard]] static FrameStepOutcome StepFrame(RuntimeState& state);
        [[nodiscard]] static bool IsTerminal(FrameStepOutcome outcome);
    };
}
