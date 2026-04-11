#pragma once

#include <cstddef>
#include <vector>

namespace dragongod
{
    using TickIndex = std::size_t;

    enum class StackRunOutcome
    {
        Continue,
        Wait,
        Completed,
        Failed
    };

    enum class FrameKind
    {
        RootPushChild,
        ChildComplete,
        RootReplace,
        ReplacementComplete,
        RootWaitThenPush,
        RootPushFailingChild,
        ChildFail
    };

    enum class FrameControl
    {
        Continue,
        Wait,
        Push,
        Pop,
        Replace,
        Complete,
        Fail
    };

    enum class FrameTraceKind
    {
        Tick,
        Enter,
        Step,
        Push,
        Pop,
        Replace,
        ExitCompleted,
        ExitFailed,
        TerminalCompleted,
        TerminalFailed
    };

    enum class StackScriptScenario
    {
        PushPopComplete,
        ReplaceComplete,
        WaitPushPopComplete,
        PushChildFail
    };

    struct FrameControlAction
    {
        FrameControl control = FrameControl::Continue;
        FrameKind target = FrameKind::RootPushChild;
    };

    struct FrameState
    {
        FrameKind kind = FrameKind::RootPushChild;
        bool entered = false;
        int step = 0;
    };

    struct FrameTraceEvent
    {
        TickIndex tick = 0;
        FrameTraceKind kind = FrameTraceKind::Tick;
        FrameKind activeFrame = FrameKind::RootPushChild;
        int frameStep = 0;
        FrameControl control = FrameControl::Continue;
        FrameKind targetFrame = FrameKind::RootPushChild;
        std::size_t stackDepth = 0;

        [[nodiscard]] bool operator==(const FrameTraceEvent& other) const = default;
    };

    struct RuntimeState
    {
        StackScriptScenario scenario = StackScriptScenario::PushPopComplete;
        std::vector<FrameState> stack;
    };

    struct [[nodiscard]] FrameRunResult
    {
        StackRunOutcome finalOutcome = StackRunOutcome::Continue;
        std::vector<FrameTraceEvent> trace;
    };

    class StackFrameRuntime
    {
    public:
        [[nodiscard]] FrameRunResult RunForTicks(RuntimeState initialState, TickIndex tickCount) const;

    private:
        [[nodiscard]] static FrameControlAction StepTopFrame(const RuntimeState& state, FrameState& frame);
        [[nodiscard]] static bool IsTerminal(StackRunOutcome outcome);
    };
}
