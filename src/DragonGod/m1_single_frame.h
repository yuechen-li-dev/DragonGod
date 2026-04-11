#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
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
        RootSetThenReadBlackboard,
        RootFallbackBranch,
        RootParentChildBlackboard,
        ChildPop,
        ChildFail,
        ChildReadParentBool,
        ChildWriteParentCounter,
        RecoveryComplete,
        RootMailboxConsumeFifo,
        RootMailboxPeekThenConsume,
        RootMailboxParentPushChildConsume,
        RootMailboxEnqueueDuringTick,
        ChildMailboxConsumeAndPop
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
        ContinueThenComplete,
        BlackboardSetReadComplete,
        BlackboardFallbackComplete,
        BlackboardParentChildComplete,
        MailboxConsumeFifoComplete,
        MailboxPeekThenConsumeComplete,
        MailboxParentChildConsumeComplete,
        MailboxEnqueueDuringTickComplete
    };

    enum class MessageKind
    {
        Signal,
        Alert
    };

    struct Message
    {
        MessageKind kind = MessageKind::Signal;
        int value = 0;

        [[nodiscard]] bool operator==(const Message& other) const = default;
    };

    struct ScheduledMessage
    {
        TickIndex tick = 0;
        Message message{};
    };

    struct RuntimeMailboxInput
    {
        std::vector<Message> initialMessages;
        std::vector<ScheduledMessage> scheduledMessages;
    };

    class Mailbox
    {
    public:
        void Enqueue(const Message& message);
        void BeginTick();
        [[nodiscard]] bool HasMessage() const;
        [[nodiscard]] bool PeekFront(Message& message) const;
        [[nodiscard]] bool ConsumeFront(Message& message);
        [[nodiscard]] const std::vector<Message>& VisibleMessages() const;

    private:
        std::vector<Message> visible_;
        std::vector<Message> staged_;
    };

    template <typename T>
    struct BbKey
    {
        std::string_view name{};
        std::uint32_t slot = 0;
    };

    class Blackboard
    {
    public:
        template <typename T>
        void Set(BbKey<T> key, const T& value);

        template <typename T>
        [[nodiscard]] bool TryGet(BbKey<T> key, T& value) const;

        template <typename T>
        [[nodiscard]] T GetOr(BbKey<T> key, const T& fallback) const;

        template <typename T>
        [[nodiscard]] bool IsDirty(BbKey<T> key) const;

        [[nodiscard]] const std::vector<std::uint32_t>& DirtySlots() const;
        void ClearDirty();

    private:
        struct BoolEntry
        {
            std::uint32_t slot = 0;
            bool value = false;
        };

        struct IntEntry
        {
            std::uint32_t slot = 0;
            int value = 0;
        };

        template <typename T>
        [[nodiscard]] const T* FindValue(std::uint32_t slot) const;

        template <typename T>
        void UpsertValue(std::uint32_t slot, const T& value);

        void MarkDirty(std::uint32_t slot);
        [[nodiscard]] bool HasDirtySlot(std::uint32_t slot) const;

        std::vector<BoolEntry> boolEntries_;
        std::vector<IntEntry> intEntries_;
        std::vector<std::uint32_t> dirtySlots_;
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
        FrameCtx(FrameId frameId, TickIndex tick, std::uint32_t pc, bool entered, Blackboard& blackboard);
        FrameCtx(FrameId frameId, TickIndex tick, std::uint32_t pc, bool entered, Blackboard& blackboard, Mailbox& mailbox);

        [[nodiscard]] FrameId Id() const;
        [[nodiscard]] TickIndex Tick() const;
        [[nodiscard]] std::uint32_t Pc() const;
        [[nodiscard]] bool Entered() const;
        [[nodiscard]] Blackboard& Bb();
        [[nodiscard]] Mailbox& Mb();

    private:
        FrameId frameId_;
        TickIndex tick_;
        std::uint32_t pc_;
        bool entered_;
        Blackboard* blackboard_ = nullptr;
        Mailbox* mailbox_ = nullptr;
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
        std::vector<std::vector<std::uint32_t>> dirtySlotsByTick;
        std::vector<std::vector<Message>> visibleMailboxByTick;
        Blackboard finalBlackboard;
    };

    class StackFrameRuntime
    {
    public:
        [[nodiscard]] FrameRunResult RunForTicks(StackScriptScenario scenario, TickIndex tickCount) const;
        [[nodiscard]] FrameRunResult RunForTicks(
            StackScriptScenario scenario,
            TickIndex tickCount,
            const RuntimeMailboxInput& mailboxInput) const;

    private:
        [[nodiscard]] static FrameRegistry BuildRegistry();
    };

    template <typename T>
    void Blackboard::Set(BbKey<T> key, const T& value)
    {
        static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int>, "Blackboard key type not supported in M2a");
        UpsertValue<T>(key.slot, value);
        MarkDirty(key.slot);
    }

    template <typename T>
    [[nodiscard]] bool Blackboard::TryGet(BbKey<T> key, T& value) const
    {
        static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int>, "Blackboard key type not supported in M2a");
        const T* found = FindValue<T>(key.slot);
        if (found == nullptr) {
            return false;
        }

        value = *found;
        return true;
    }

    template <typename T>
    [[nodiscard]] T Blackboard::GetOr(BbKey<T> key, const T& fallback) const
    {
        static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int>, "Blackboard key type not supported in M2a");
        const T* found = FindValue<T>(key.slot);
        if (found == nullptr) {
            return fallback;
        }

        return *found;
    }

    template <typename T>
    [[nodiscard]] bool Blackboard::IsDirty(BbKey<T> key) const
    {
        static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int>, "Blackboard key type not supported in M2a");
        return HasDirtySlot(key.slot);
    }
}
