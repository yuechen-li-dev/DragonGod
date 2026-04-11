#pragma once

#include <cstddef>
#include <cstdint>
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

    enum class FrameId
    {
        RootPushChild,
        RootReplace,
        RootWaitThenPush,
        RootPushFailingChild,
        RootContinueThenComplete,
        ChildPop,
        ChildFail,
        RecoveryComplete
    };

    enum class FrameControlKind
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
        PushChildFail,
        ContinueThenComplete
    };

    struct FrameControl
    {
        FrameControlKind kind = FrameControlKind::Continue;
        std::uint32_t resumePc = 0;
        std::uint32_t waitTicks = 0;
        FrameId target = FrameId::RootPushChild;
        int failReason = 0;
    };

    class FrameCtx
    {
    public:
        FrameCtx(FrameId frameId, TickIndex tick, std::uint32_t pc, bool entered);

        [[nodiscard]] FrameId Id() const;
        [[nodiscard]] TickIndex Tick() const;
        [[nodiscard]] std::uint32_t Pc() const;
        [[nodiscard]] bool Entered() const;

    private:
        FrameId frameId_;
        TickIndex tick_;
        std::uint32_t pc_;
        bool entered_;
    };

    using FrameFn = FrameControl (*)(FrameCtx& ctx);

    struct FrameDef
    {
        FrameId id = FrameId::RootPushChild;
        FrameFn function = nullptr;
    };

    class FrameRegistry
    {
    public:
        void Add(FrameId id, FrameFn function);
        [[nodiscard]] FrameFn Find(FrameId id) const;

    private:
        std::vector<FrameDef> definitions_;
    };

    namespace Dg
    {
        [[nodiscard]] FrameControl Continue(std::uint32_t resumePc);
        [[nodiscard]] FrameControl WaitTicks(std::uint32_t ticks, std::uint32_t resumePc);
        [[nodiscard]] FrameControl Push(FrameId target, std::uint32_t resumePc);
        [[nodiscard]] FrameControl Pop();
        [[nodiscard]] FrameControl Replace(FrameId target);
        [[nodiscard]] FrameControl Complete();
        [[nodiscard]] FrameControl Fail(int reason);
    }

    struct FrameTraceEvent
    {
        TickIndex tick = 0;
        FrameTraceKind kind = FrameTraceKind::Tick;
        FrameId activeFrame = FrameId::RootPushChild;
        std::uint32_t framePc = 0;
        FrameControlKind control = FrameControlKind::Continue;
        FrameId targetFrame = FrameId::RootPushChild;
        std::size_t stackDepth = 0;

        [[nodiscard]] bool operator==(const FrameTraceEvent& other) const = default;
    };

    struct [[nodiscard]] FrameRunResult
    {
        StackRunOutcome finalOutcome = StackRunOutcome::Continue;
        std::vector<FrameTraceEvent> trace;
    };

    class StackFrameRuntime
    {
    public:
        [[nodiscard]] FrameRunResult RunForTicks(StackScriptScenario scenario, TickIndex tickCount) const;

    private:
        [[nodiscard]] static FrameRegistry BuildRegistry();
    };
}
