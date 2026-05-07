#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
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

        struct FrameId
    {
        std::uint64_t domain = 0;
        std::uint32_t local = 0;

        [[nodiscard]] bool operator==(const FrameId& other) const = default;
    };

    inline constexpr std::uint64_t CanonicalFrameDomain = 0;

    enum class CanonicalFrameLocalId : std::uint32_t
    {
        RootPushChild = 1,
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
        ChildMailboxConsumeAndPop,
        RootUtilityHighestScore,
        RootUtilityHysteresis,
        RootUtilityMinCommit,
        RootUtilityTieBreakKeepCurrent,
        RootUtilityTieBreakFirstListed,
        RootUtilityTieBreakLastListed,
        UtilityActionPrimary,
        UtilityActionSecondary,
        UtilityActionFallback,
        RootActImmediateDeferred,
        RootActOrderedDeferred,
        RootActParentPushChild,
        ChildActImmediate,
        RootActUtilityDriven,
        RootTypedPhaseMailboxAct
    };

    [[nodiscard]] constexpr FrameId CanonicalFrame(CanonicalFrameLocalId id)
    {
        return FrameId{
            .domain = CanonicalFrameDomain,
            .local = static_cast<std::uint32_t>(id)
        };
    }

    namespace CanonicalFrameIds
    {
        inline constexpr FrameId RootPushChild = CanonicalFrame(CanonicalFrameLocalId::RootPushChild);
        inline constexpr FrameId RootReplace = CanonicalFrame(CanonicalFrameLocalId::RootReplace);
        inline constexpr FrameId RootWaitThenPush = CanonicalFrame(CanonicalFrameLocalId::RootWaitThenPush);
        inline constexpr FrameId RootPushFailingChild = CanonicalFrame(CanonicalFrameLocalId::RootPushFailingChild);
        inline constexpr FrameId RootContinueThenComplete = CanonicalFrame(CanonicalFrameLocalId::RootContinueThenComplete);
        inline constexpr FrameId RootSetThenReadBlackboard = CanonicalFrame(CanonicalFrameLocalId::RootSetThenReadBlackboard);
        inline constexpr FrameId RootFallbackBranch = CanonicalFrame(CanonicalFrameLocalId::RootFallbackBranch);
        inline constexpr FrameId RootParentChildBlackboard = CanonicalFrame(CanonicalFrameLocalId::RootParentChildBlackboard);
        inline constexpr FrameId ChildPop = CanonicalFrame(CanonicalFrameLocalId::ChildPop);
        inline constexpr FrameId ChildFail = CanonicalFrame(CanonicalFrameLocalId::ChildFail);
        inline constexpr FrameId ChildReadParentBool = CanonicalFrame(CanonicalFrameLocalId::ChildReadParentBool);
        inline constexpr FrameId ChildWriteParentCounter = CanonicalFrame(CanonicalFrameLocalId::ChildWriteParentCounter);
        inline constexpr FrameId RecoveryComplete = CanonicalFrame(CanonicalFrameLocalId::RecoveryComplete);
        inline constexpr FrameId RootMailboxConsumeFifo = CanonicalFrame(CanonicalFrameLocalId::RootMailboxConsumeFifo);
        inline constexpr FrameId RootMailboxPeekThenConsume = CanonicalFrame(CanonicalFrameLocalId::RootMailboxPeekThenConsume);
        inline constexpr FrameId RootMailboxParentPushChildConsume = CanonicalFrame(CanonicalFrameLocalId::RootMailboxParentPushChildConsume);
        inline constexpr FrameId RootMailboxEnqueueDuringTick = CanonicalFrame(CanonicalFrameLocalId::RootMailboxEnqueueDuringTick);
        inline constexpr FrameId ChildMailboxConsumeAndPop = CanonicalFrame(CanonicalFrameLocalId::ChildMailboxConsumeAndPop);
        inline constexpr FrameId RootUtilityHighestScore = CanonicalFrame(CanonicalFrameLocalId::RootUtilityHighestScore);
        inline constexpr FrameId RootUtilityHysteresis = CanonicalFrame(CanonicalFrameLocalId::RootUtilityHysteresis);
        inline constexpr FrameId RootUtilityMinCommit = CanonicalFrame(CanonicalFrameLocalId::RootUtilityMinCommit);
        inline constexpr FrameId RootUtilityTieBreakKeepCurrent = CanonicalFrame(CanonicalFrameLocalId::RootUtilityTieBreakKeepCurrent);
        inline constexpr FrameId RootUtilityTieBreakFirstListed = CanonicalFrame(CanonicalFrameLocalId::RootUtilityTieBreakFirstListed);
        inline constexpr FrameId RootUtilityTieBreakLastListed = CanonicalFrame(CanonicalFrameLocalId::RootUtilityTieBreakLastListed);
        inline constexpr FrameId UtilityActionPrimary = CanonicalFrame(CanonicalFrameLocalId::UtilityActionPrimary);
        inline constexpr FrameId UtilityActionSecondary = CanonicalFrame(CanonicalFrameLocalId::UtilityActionSecondary);
        inline constexpr FrameId UtilityActionFallback = CanonicalFrame(CanonicalFrameLocalId::UtilityActionFallback);
        inline constexpr FrameId RootActImmediateDeferred = CanonicalFrame(CanonicalFrameLocalId::RootActImmediateDeferred);
        inline constexpr FrameId RootActOrderedDeferred = CanonicalFrame(CanonicalFrameLocalId::RootActOrderedDeferred);
        inline constexpr FrameId RootActParentPushChild = CanonicalFrame(CanonicalFrameLocalId::RootActParentPushChild);
        inline constexpr FrameId ChildActImmediate = CanonicalFrame(CanonicalFrameLocalId::ChildActImmediate);
        inline constexpr FrameId RootActUtilityDriven = CanonicalFrame(CanonicalFrameLocalId::RootActUtilityDriven);
        inline constexpr FrameId RootTypedPhaseMailboxAct = CanonicalFrame(CanonicalFrameLocalId::RootTypedPhaseMailboxAct);
    }

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
        MailboxEnqueueDuringTickComplete,
        UtilityHighestScoreComplete,
        UtilityHysteresisComplete,
        UtilityMinCommitComplete,
        UtilityTieBreakKeepCurrentComplete,
        UtilityTieBreakFirstListedComplete,
        UtilityTieBreakLastListedComplete,
        ActImmediateDeferredComplete,
        ActOrderedDeferredComplete,
        ActParentPushChildComplete,
        ActUtilityDrivenComplete,
        TypedPhaseMailboxActComplete
    };

    enum class ActId
    {
        PlayBark,
        RaiseAlarm,
        OpenDoor,
        UtilityPrimary,
        UtilitySecondary
    };

    struct ActRequest
    {
        ActId id = ActId::PlayBark;
        bool deferred = false;
        TickIndex emittedTick = 0;
        TickIndex dueTick = 0;
        std::uint32_t delayTicks = 0;

        [[nodiscard]] bool operator==(const ActRequest& other) const = default;
    };

    struct DeferredActChunkEntry
    {
        ActId id = ActId::PlayBark;
        TickIndex dueTick = 0;
        TickIndex emittedTick = 0;
        std::uint32_t delayTicks = 0;

        [[nodiscard]] bool operator==(const DeferredActChunkEntry& other) const = default;
    };

    class ActRuntime;

    class ActCtx
    {
    public:
        ActCtx();
        explicit ActCtx(ActRuntime& runtime);
        [[nodiscard]] bool HasRuntime() const;
        void Immediate(ActId id);
        void Deferred(ActId id, std::uint32_t delayTicks);

    private:
        ActRuntime* runtime_ = nullptr;
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

        [[nodiscard]] bool operator==(const ScheduledMessage& other) const = default;
    };

    struct RuntimeMailboxInput
    {
        std::vector<Message> initialMessages;
        std::vector<ScheduledMessage> scheduledMessages;
    };

    struct MailboxChunk
    {
        std::vector<Message> visibleMessages;
        std::vector<Message> stagedMessages;

        [[nodiscard]] bool operator==(const MailboxChunk& other) const = default;
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
        [[nodiscard]] const std::vector<Message>& StagedMessages() const;
        [[nodiscard]] MailboxChunk ExportChunk() const;
        void ImportChunk(const MailboxChunk& chunk);

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
        struct BoolChunkEntry
        {
            std::uint32_t slot = 0;
            bool value = false;

            [[nodiscard]] bool operator==(const BoolChunkEntry& other) const = default;
        };

        struct IntChunkEntry
        {
            std::uint32_t slot = 0;
            int value = 0;

            [[nodiscard]] bool operator==(const IntChunkEntry& other) const = default;
        };

        struct Chunk
        {
            std::vector<BoolChunkEntry> boolEntries;
            std::vector<IntChunkEntry> intEntries;
            std::vector<std::uint32_t> dirtySlots;

            [[nodiscard]] bool operator==(const Chunk& other) const = default;
        };
        struct SlotCollision
        {
            std::uint32_t slot = 0;
            std::string_view firstName{};
            std::string_view secondName{};
            bool firstWasBool = false;
            bool secondWasBool = false;
        };

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
        [[nodiscard]] Chunk ExportChunk() const;
        void ImportChunk(const Chunk& chunk);
        [[nodiscard]] bool HasSlotCollision() const;
        [[nodiscard]] std::optional<SlotCollision> LastSlotCollision() const;

    private:
        enum class BbValueKind
        {
            Bool,
            Int
        };

        struct BbSlotMetadata
        {
            std::uint32_t slot = 0;
            std::string_view name{};
            BbValueKind kind = BbValueKind::Bool;
        };

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
        template <typename T>
        [[nodiscard]] static BbValueKind ValueKindFor();
        template <typename T>
        void ValidateKey(BbKey<T> key);

        void MarkDirty(std::uint32_t slot);
        [[nodiscard]] bool HasDirtySlot(std::uint32_t slot) const;

        std::vector<BoolEntry> boolEntries_;
        std::vector<IntEntry> intEntries_;
        std::vector<std::uint32_t> dirtySlots_;
        std::vector<BbSlotMetadata> slotMetadata_;
        std::optional<SlotCollision> lastSlotCollision_;
    };

    struct FrameControl
    {
        FrameControlKind kind = FrameControlKind::Continue;
        std::uint32_t resumePc = 0;
        std::uint32_t waitTicks = 0;
        FrameId target = CanonicalFrameIds::RootPushChild;
        int failReason = 0;
        bool stayOnCurrentPc = false;
    };

    struct UtilityDecisionCandidateTrace
    {
        FrameId target = CanonicalFrameIds::RootPushChild;
        float score = 0.0f;

        [[nodiscard]] bool operator==(const UtilityDecisionCandidateTrace& other) const = default;
    };

    struct UtilityDecisionTraceEntry
    {
        FrameId decisionFrame = CanonicalFrameIds::RootPushChild;
        std::vector<UtilityDecisionCandidateTrace> candidates;
        bool hadCommittedBefore = false;
        FrameId committedBefore = CanonicalFrameIds::RootPushChild;
        std::uint32_t committedAgeBefore = 0;
        FrameId chosen = CanonicalFrameIds::RootPushChild;
        bool minCommitBlocked = false;
        bool hysteresisBlocked = false;
        bool tieBreakUsed = false;

        [[nodiscard]] bool operator==(const UtilityDecisionTraceEntry& other) const = default;
    };

    struct UtilityMemoryChunkEntry
    {
        FrameId frame = CanonicalFrameIds::RootPushChild;
        bool hasCommitted = false;
        FrameId committed = CanonicalFrameIds::RootPushChild;
        std::uint32_t age = 0;

        [[nodiscard]] bool operator==(const UtilityMemoryChunkEntry& other) const = default;
    };

    class UtilityMemoryStore;

    class FrameCtx
    {
    public:
        // Read-only frame identity/phase/tick metadata plus access to mutable runtime surfaces.
        // This is the primary author-facing context passed to every frame function.
        FrameCtx(FrameId frameId, TickIndex tick, std::uint32_t pc, bool entered, Blackboard& blackboard);
        FrameCtx(FrameId frameId, TickIndex tick, std::uint32_t pc, bool entered, Blackboard& blackboard, Mailbox& mailbox);

        [[nodiscard]] FrameId Id() const;
        [[nodiscard]] TickIndex Tick() const;
        [[nodiscard]] std::uint32_t Pc() const;
        template <typename TEnum>
        [[nodiscard]] TEnum PcAs() const;
        [[nodiscard]] bool Entered() const;
        [[nodiscard]] Blackboard& Bb();
        [[nodiscard]] const Blackboard& Bb() const;
        [[nodiscard]] Mailbox& Mb();
        [[nodiscard]] const Mailbox& Mb() const;
        [[nodiscard]] ActCtx& Act();
        [[nodiscard]] const ActCtx& Act() const;
        [[nodiscard]] std::uint32_t ReadUtilityCommitAge(FrameId frameId) const;

    private:
        FrameId frameId_;
        TickIndex tick_;
        std::uint32_t pc_;
        bool entered_;
        Blackboard* blackboard_ = nullptr;
        Mailbox* mailbox_ = nullptr;
        ActCtx act_;
        UtilityMemoryStore* utilityMemory_ = nullptr;
        std::vector<UtilityDecisionTraceEntry>* utilityTrace_ = nullptr;

        FrameCtx(
            FrameId frameId,
            TickIndex tick,
            std::uint32_t pc,
            bool entered,
            Blackboard& blackboard,
            Mailbox& mailbox,
            ActRuntime& actRuntime,
            UtilityMemoryStore& utilityMemory,
            std::vector<UtilityDecisionTraceEntry>& utilityTrace);

        friend class StackFrameRuntimeSession;
        friend struct DecideAccess;
    };

    using FrameFn = FrameControl (*)(FrameCtx& ctx);

    struct FrameDef
    {
        FrameId id = CanonicalFrameIds::RootPushChild;
        FrameFn function = nullptr;
        std::string debugName;
    };

    class FrameRegistry
    {
    public:
        // Register caller-owned frame functions for a domain/session.
        void Add(FrameId id, FrameFn function, std::string_view debugName = {});
        [[nodiscard]] FrameFn Find(FrameId id) const;
        [[nodiscard]] std::string_view FindDebugName(FrameId id) const;

    private:
        std::vector<FrameDef> definitions_;
    };

    namespace Dg
    {
        using ConsiderationFn = float (*)(const FrameCtx& ctx);

        struct UtilityCandidate
        {
            FrameId target = CanonicalFrameIds::RootPushChild;
            ConsiderationFn consideration = nullptr;
        };

        enum class TieBreakPolicy
        {
            KeepCurrent,
            FirstListed,
            LastListed
        };

        struct DecideOptions
        {
            float hysteresis = 0.0f;
            int minCommitTicks = 0;
            TieBreakPolicy tieBreak = TieBreakPolicy::KeepCurrent;
        };

        [[nodiscard]] UtilityCandidate when(FrameId target, ConsiderationFn consideration);
        [[nodiscard]] FrameControl Decide(
            FrameCtx& ctx,
            std::initializer_list<UtilityCandidate> candidates,
            DecideOptions options = {});

        [[nodiscard]] FrameControl Continue(std::uint32_t resumePc);
        [[nodiscard]] FrameControl WaitTicks(std::uint32_t ticks, std::uint32_t resumePc);
        [[nodiscard]] FrameControl Stay();
        [[nodiscard]] FrameControl Push(FrameId target, std::uint32_t resumePc);
        [[nodiscard]] FrameControl Pop();
        [[nodiscard]] FrameControl Replace(FrameId target);
        [[nodiscard]] FrameControl Complete();
        [[nodiscard]] FrameControl Fail(int reason);

        template <typename TEnum>
            requires(std::is_enum_v<TEnum>)
        [[nodiscard]] FrameControl Continue(TEnum resumePhase);

        template <typename TEnum>
            requires(std::is_enum_v<TEnum>)
        [[nodiscard]] FrameControl WaitTicks(std::uint32_t ticks, TEnum resumePhase);
    }

    struct FrameTraceEvent
    {
        TickIndex tick = 0;
        FrameTraceKind kind = FrameTraceKind::Tick;
        FrameId activeFrame = CanonicalFrameIds::RootPushChild;
        std::uint32_t framePc = 0;
        FrameControlKind control = FrameControlKind::Continue;
        FrameId targetFrame = CanonicalFrameIds::RootPushChild;
        std::size_t stackDepth = 0;

        [[nodiscard]] bool operator==(const FrameTraceEvent& other) const = default;
    };

    struct StackFrameChunkEntry
    {
        FrameId id = CanonicalFrameIds::RootPushChild;
        std::uint32_t pc = 0;
        bool entered = false;
        std::uint32_t remainingWaitTicks = 0;

        [[nodiscard]] bool operator==(const StackFrameChunkEntry& other) const = default;
    };

    struct StackChunk
    {
        std::vector<StackFrameChunkEntry> frames;

        [[nodiscard]] bool operator==(const StackChunk& other) const = default;
    };

    struct TickTraceEntry
    {
        TickIndex tick = 0;
        StackRunOutcome outcome = StackRunOutcome::Continue;
        std::vector<StackFrameChunkEntry> stack;
        std::vector<std::uint32_t> dirtySlots;
        std::vector<Message> visibleMailbox;
        std::vector<UtilityDecisionTraceEntry> utilityDecisions;
        std::vector<ActRequest> emittedActuation;
        std::vector<ActRequest> pendingDeferredActuation;

        [[nodiscard]] bool operator==(const TickTraceEntry& other) const = default;
    };

    struct TraceComparisonResult
    {
        bool matches = true;
        std::size_t firstMismatchIndex = 0;
        std::string mismatchReason;
        TickTraceEntry expected;
        TickTraceEntry actual;
        std::size_t expectedEntryCount = 0;
        std::size_t actualEntryCount = 0;
    };

    struct [[nodiscard]] FrameRunResult
    {
        StackRunOutcome finalOutcome = StackRunOutcome::Continue;
        std::vector<FrameTraceEvent> trace;
        std::vector<TickTraceEntry> tickTrace;
        std::vector<std::vector<std::uint32_t>> dirtySlotsByTick;
        std::vector<std::vector<Message>> visibleMailboxByTick;
        std::vector<std::vector<ActRequest>> actuationByTick;
        Blackboard finalBlackboard;
    };

    [[nodiscard]] TraceComparisonResult CompareTickTraces(
        const std::vector<TickTraceEntry>& expected,
        const std::vector<TickTraceEntry>& actual);
    [[nodiscard]] std::vector<std::string> SerializeTickTrace(const std::vector<TickTraceEntry>& trace);
    [[nodiscard]] std::string FormatTraceComparison(const TraceComparisonResult& comparison);

    struct RuntimeChunk
    {
        enum class Origin
        {
            CanonicalScenario,
            ExplicitRoot
        };

        Origin origin = Origin::CanonicalScenario;
        StackScriptScenario scenario = StackScriptScenario::PushPopComplete;
        FrameId rootFrame = CanonicalFrameIds::RootPushChild;
        TickIndex nextTick = 0;
        StackRunOutcome lastOutcome = StackRunOutcome::Continue;
        std::vector<ScheduledMessage> scheduledMessages;
        StackChunk stack;
        std::vector<UtilityMemoryChunkEntry> utilityMemory;
        std::vector<DeferredActChunkEntry> deferredActuation;
        Blackboard::Chunk blackboard;
        MailboxChunk mailbox;

        [[nodiscard]] bool operator==(const RuntimeChunk& other) const = default;
    };

    struct StackFrameSessionInit
    {
        // Public M16a seam: callers provide registry + explicit root + mailbox seed input.
        FrameRegistry registry;
        FrameId rootFrame = CanonicalFrameIds::RootPushChild;
        RuntimeMailboxInput mailboxInput;
    };

    class StackFrameRuntimeSession
    {
    public:
        StackFrameRuntimeSession(StackScriptScenario scenario, const RuntimeMailboxInput& mailboxInput);
        explicit StackFrameRuntimeSession(StackFrameSessionInit init);
        explicit StackFrameRuntimeSession(const RuntimeChunk& chunk);
        StackFrameRuntimeSession(const RuntimeChunk& chunk, FrameRegistry registry);
        ~StackFrameRuntimeSession();

        [[nodiscard]] TickIndex NextTick() const;
        [[nodiscard]] StackRunOutcome LastOutcome() const;
        [[nodiscard]] bool IsTerminal() const;
        [[nodiscard]] RuntimeChunk Save() const;

        [[nodiscard]] FrameRunResult RunForTicks(TickIndex tickCount);

    private:
        RuntimeChunk::Origin origin_ = RuntimeChunk::Origin::CanonicalScenario;
        [[nodiscard]] static FrameRegistry BuildRegistry();
        [[nodiscard]] static FrameId RootFrameForScenario(StackScriptScenario scenario);
        [[nodiscard]] bool RunSingleTick(FrameRunResult& result);

        StackScriptScenario scenario_ = StackScriptScenario::PushPopComplete;
        FrameId rootFrame_ = CanonicalFrameIds::RootPushChild;
        TickIndex nextTick_ = 0;
        StackRunOutcome lastOutcome_ = StackRunOutcome::Continue;
        FrameRegistry registry_;
        Blackboard blackboard_;
        Mailbox mailbox_;
        std::vector<ScheduledMessage> scheduledMessages_;
        std::vector<StackFrameChunkEntry> stack_;
        std::unique_ptr<ActRuntime> actRuntime_;
        std::unique_ptr<UtilityMemoryStore> utilityMemory_;
    };

    class StackFrameRuntime
    {
    public:
        [[nodiscard]] FrameRunResult RunForTicks(StackScriptScenario scenario, TickIndex tickCount) const;
        [[nodiscard]] FrameRunResult RunForTicks(const StackFrameSessionInit& init, TickIndex tickCount) const;
        [[nodiscard]] FrameRunResult RunForTicks(
            StackScriptScenario scenario,
            TickIndex tickCount,
            const RuntimeMailboxInput& mailboxInput) const;
    };

    // Built-in canonical fixture helpers.
    // Author-owned domains may supply their own registries and roots through StackFrameSessionInit.
    [[nodiscard]] FrameId CanonicalScenarioRootFrame(StackScriptScenario scenario);
    [[nodiscard]] FrameRegistry BuildCanonicalFrameRegistry();

    // Built-in scorer fixtures used by canonical proof/demo scenarios.
    // These are neutral examples for deterministic utility coverage, not domain constraints.
    namespace When
    {
        [[nodiscard]] float Always(const FrameCtx& ctx);
        [[nodiscard]] float HighSignal(const FrameCtx& ctx);
        [[nodiscard]] float ResourcePressure(const FrameCtx& ctx);
    }

    template <typename T>
    void Blackboard::Set(BbKey<T> key, const T& value)
    {
        static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int>, "Blackboard key type not supported in M2a");
        ValidateKey<T>(key);
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

    template <>
    [[nodiscard]] inline Blackboard::BbValueKind Blackboard::ValueKindFor<bool>()
    {
        return BbValueKind::Bool;
    }

    template <>
    [[nodiscard]] inline Blackboard::BbValueKind Blackboard::ValueKindFor<int>()
    {
        return BbValueKind::Int;
    }

    template <typename TEnum>
    [[nodiscard]] TEnum FrameCtx::PcAs() const
    {
        static_assert(std::is_enum_v<TEnum>, "PcAs requires an enum phase type");
        return static_cast<TEnum>(pc_);
    }

    namespace Dg
    {
        template <typename TEnum>
            requires(std::is_enum_v<TEnum>)
        [[nodiscard]] FrameControl Continue(TEnum resumePhase)
        {
            return Continue(static_cast<std::uint32_t>(resumePhase));
        }

        template <typename TEnum>
            requires(std::is_enum_v<TEnum>)
        [[nodiscard]] FrameControl WaitTicks(std::uint32_t ticks, TEnum resumePhase)
        {
            return WaitTicks(ticks, static_cast<std::uint32_t>(resumePhase));
        }
    }
}
