#include "m1_single_frame.h"

namespace dragongod
{
    class UtilityMemoryStore
    {
    public:
        struct Entry
        {
            FrameId frame = FrameId::RootPushChild;
            bool hasCommitted = false;
            FrameId committed = FrameId::RootPushChild;
            std::uint32_t age = 0;
        };

        [[nodiscard]] Entry* Find(FrameId frame)
        {
            for (Entry& entry : entries_) {
                if (entry.frame == frame) {
                    return &entry;
                }
            }

            return nullptr;
        }

        [[nodiscard]] const Entry* Find(FrameId frame) const
        {
            for (const Entry& entry : entries_) {
                if (entry.frame == frame) {
                    return &entry;
                }
            }

            return nullptr;
        }

        [[nodiscard]] Entry& Ensure(FrameId frame)
        {
            Entry* found = Find(frame);
            if (found != nullptr) {
                return *found;
            }

            entries_.push_back(Entry{
                .frame = frame
            });
            return entries_.back();
        }

        [[nodiscard]] std::vector<UtilityMemoryChunkEntry> ExportChunk() const
        {
            std::vector<UtilityMemoryChunkEntry> chunk;
            for (const Entry& entry : entries_) {
                chunk.push_back(UtilityMemoryChunkEntry{
                    .frame = entry.frame,
                    .hasCommitted = entry.hasCommitted,
                    .committed = entry.committed,
                    .age = entry.age
                });
            }

            return chunk;
        }

        void ImportChunk(const std::vector<UtilityMemoryChunkEntry>& chunk)
        {
            entries_.clear();
            for (const UtilityMemoryChunkEntry& entry : chunk) {
                entries_.push_back(Entry{
                    .frame = entry.frame,
                    .hasCommitted = entry.hasCommitted,
                    .committed = entry.committed,
                    .age = entry.age
                });
            }
        }

    private:
        std::vector<Entry> entries_;
    };

    class ActRuntime
    {
    public:
        ActRuntime() = default;

        void BeginTick(TickIndex tick)
        {
            currentTick_ = tick;
            emittedNow_.clear();
        }

        void EmitImmediate(ActId id)
        {
            emittedNow_.push_back(ActRequest{
                .id = id,
                .deferred = false,
                .emittedTick = currentTick_,
                .dueTick = currentTick_,
                .delayTicks = 0
            });
        }

        void ScheduleDeferred(ActId id, std::uint32_t delayTicks)
        {
            pending_.push_back(ActRequest{
                .id = id,
                .deferred = true,
                .emittedTick = currentTick_,
                .dueTick = currentTick_ + static_cast<TickIndex>(delayTicks),
                .delayTicks = delayTicks
            });
        }

        void FlushMatured()
        {
            std::vector<ActRequest> stillPending;
            for (const ActRequest& request : pending_) {
                if (request.dueTick <= currentTick_) {
                    emittedNow_.push_back(request);
                } else {
                    stillPending.push_back(request);
                }
            }

            pending_ = stillPending;
        }

        [[nodiscard]] const std::vector<ActRequest>& EmittedNow() const
        {
            return emittedNow_;
        }

        [[nodiscard]] const std::vector<ActRequest>& Pending() const
        {
            return pending_;
        }

        [[nodiscard]] std::vector<DeferredActChunkEntry> ExportDeferredChunk() const
        {
            std::vector<DeferredActChunkEntry> chunk;
            for (const ActRequest& request : pending_) {
                chunk.push_back(DeferredActChunkEntry{
                    .id = request.id,
                    .dueTick = request.dueTick,
                    .emittedTick = request.emittedTick,
                    .delayTicks = request.delayTicks
                });
            }

            return chunk;
        }

        void ImportDeferredChunk(const std::vector<DeferredActChunkEntry>& chunk)
        {
            pending_.clear();
            emittedNow_.clear();
            for (const DeferredActChunkEntry& entry : chunk) {
                pending_.push_back(ActRequest{
                    .id = entry.id,
                    .deferred = true,
                    .emittedTick = entry.emittedTick,
                    .dueTick = entry.dueTick,
                    .delayTicks = entry.delayTicks
                });
            }
        }

    private:
        TickIndex currentTick_ = 0;
        std::vector<ActRequest> emittedNow_;
        std::vector<ActRequest> pending_;
    };

    namespace
    {
        [[nodiscard]] FrameId ScenarioRootFrame(StackScriptScenario scenario)
        {
            if (scenario == StackScriptScenario::PushPopComplete) {
                return FrameId::RootPushChild;
            }

            if (scenario == StackScriptScenario::ReplaceComplete) {
                return FrameId::RootReplace;
            }

            if (scenario == StackScriptScenario::WaitPushPopComplete) {
                return FrameId::RootWaitThenPush;
            }

            if (scenario == StackScriptScenario::PushChildFail) {
                return FrameId::RootPushFailingChild;
            }

            if (scenario == StackScriptScenario::BlackboardSetReadComplete) {
                return FrameId::RootSetThenReadBlackboard;
            }

            if (scenario == StackScriptScenario::BlackboardFallbackComplete) {
                return FrameId::RootFallbackBranch;
            }

            if (scenario == StackScriptScenario::BlackboardParentChildComplete) {
                return FrameId::RootParentChildBlackboard;
            }

            if (scenario == StackScriptScenario::MailboxConsumeFifoComplete) {
                return FrameId::RootMailboxConsumeFifo;
            }

            if (scenario == StackScriptScenario::MailboxPeekThenConsumeComplete) {
                return FrameId::RootMailboxPeekThenConsume;
            }

            if (scenario == StackScriptScenario::MailboxParentChildConsumeComplete) {
                return FrameId::RootMailboxParentPushChildConsume;
            }

            if (scenario == StackScriptScenario::MailboxEnqueueDuringTickComplete) {
                return FrameId::RootMailboxEnqueueDuringTick;
            }

            if (scenario == StackScriptScenario::UtilityHighestScoreComplete) {
                return FrameId::RootUtilityHighestScore;
            }

            if (scenario == StackScriptScenario::UtilityHysteresisComplete) {
                return FrameId::RootUtilityHysteresis;
            }

            if (scenario == StackScriptScenario::UtilityMinCommitComplete) {
                return FrameId::RootUtilityMinCommit;
            }

            if (scenario == StackScriptScenario::UtilityTieBreakKeepCurrentComplete) {
                return FrameId::RootUtilityTieBreakKeepCurrent;
            }

            if (scenario == StackScriptScenario::UtilityTieBreakFirstListedComplete) {
                return FrameId::RootUtilityTieBreakFirstListed;
            }

            if (scenario == StackScriptScenario::UtilityTieBreakLastListedComplete) {
                return FrameId::RootUtilityTieBreakLastListed;
            }

            if (scenario == StackScriptScenario::ActImmediateDeferredComplete) {
                return FrameId::RootActImmediateDeferred;
            }

            if (scenario == StackScriptScenario::ActOrderedDeferredComplete) {
                return FrameId::RootActOrderedDeferred;
            }

            if (scenario == StackScriptScenario::ActParentPushChildComplete) {
                return FrameId::RootActParentPushChild;
            }

            if (scenario == StackScriptScenario::ActUtilityDrivenComplete) {
                return FrameId::RootActUtilityDriven;
            }

            if (scenario == StackScriptScenario::TypedPhaseMailboxActComplete) {
                return FrameId::RootTypedPhaseMailboxAct;
            }

            return FrameId::RootContinueThenComplete;
        }

        void EmitTrace(
            FrameRunResult& result,
            TickIndex tick,
            FrameTraceKind kind,
            const StackFrameChunkEntry& active,
            FrameControlKind control,
            FrameId targetFrame,
            std::size_t stackDepth)
        {
            result.trace.push_back(FrameTraceEvent{
                .tick = tick,
                .kind = kind,
                .activeFrame = active.id,
                .framePc = active.pc,
                .control = control,
                .targetFrame = targetFrame,
                .stackDepth = stackDepth
            });
        }

        [[nodiscard]] TickTraceEntry MakeTickTraceEntry(
            TickIndex tick,
            StackRunOutcome outcome,
            const std::vector<StackFrameChunkEntry>& stack,
            const std::vector<std::uint32_t>& dirtySlots,
            const std::vector<Message>& visibleMailbox,
            const std::vector<UtilityDecisionTraceEntry>& utilityDecisions,
            const std::vector<ActRequest>& emittedActuation,
            const std::vector<ActRequest>& pendingDeferredActuation)
        {
            return TickTraceEntry{
                .tick = tick,
                .outcome = outcome,
                .stack = stack,
                .dirtySlots = dirtySlots,
                .visibleMailbox = visibleMailbox,
                .utilityDecisions = utilityDecisions,
                .emittedActuation = emittedActuation,
                .pendingDeferredActuation = pendingDeferredActuation
            };
        }

        [[nodiscard]] float ClampScore01(float value)
        {
            if (value < 0.0f) {
                return 0.0f;
            }

            if (value > 1.0f) {
                return 1.0f;
            }

            return value;
        }


        namespace nodes
        {
            enum class RootMailboxConsumePhase : std::uint32_t
            {
                ConsumeFirst,
                ConsumeSecond
            };

            enum class RootTypedPhaseMailboxActPhase : std::uint32_t
            {
                AwaitSignal,
                AwaitAlert
            };

            namespace Keys
            {
                constexpr BbKey<bool> Alerted{ .name = "Alerted", .slot = 1 };
                constexpr BbKey<bool> ChildSawAlerted{ .name = "ChildSawAlerted", .slot = 2 };
                constexpr BbKey<int> Counter{ .name = "Counter", .slot = 3 };
                constexpr BbKey<int> FirstMessageValue{ .name = "FirstMessageValue", .slot = 4 };
                constexpr BbKey<int> SecondMessageValue{ .name = "SecondMessageValue", .slot = 5 };
                constexpr BbKey<int> SeenPeekValue{ .name = "SeenPeekValue", .slot = 6 };
                constexpr BbKey<bool> MailboxTriggered{ .name = "MailboxTriggered", .slot = 7 };
                constexpr BbKey<int> AlertScore{ .name = "AlertScore", .slot = 8 };
                constexpr BbKey<int> LowAmmoScore{ .name = "LowAmmoScore", .slot = 9 };
                constexpr BbKey<int> UtilityChoice{ .name = "UtilityChoice", .slot = 10 };
                constexpr BbKey<int> UtilityDecisionsMade{ .name = "UtilityDecisionsMade", .slot = 11 };
                constexpr BbKey<bool> ActMailboxSeen{ .name = "ActMailboxSeen", .slot = 12 };
            }

            [[nodiscard]] FrameControl RootPushChild(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Push(FrameId::ChildPop, 1);
                case 1:
                    return Dg::Complete();
                default:
                    return Dg::Fail(100);
                }
            }

            [[nodiscard]] FrameControl RootReplace(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Replace(FrameId::RecoveryComplete);
                default:
                    return Dg::Fail(101);
                }
            }

            [[nodiscard]] FrameControl RootWaitThenPush(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::WaitTicks(1, 1);
                case 1:
                    return Dg::Push(FrameId::ChildPop, 2);
                case 2:
                    return Dg::Complete();
                default:
                    return Dg::Fail(102);
                }
            }

            [[nodiscard]] FrameControl RootPushFailingChild(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Push(FrameId::ChildFail, 1);
                default:
                    return Dg::Fail(103);
                }
            }

            [[nodiscard]] FrameControl RootContinueThenComplete(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Continue(1);
                case 1:
                    return Dg::Complete();
                default:
                    return Dg::Fail(104);
                }
            }

            [[nodiscard]] FrameControl RootSetThenReadBlackboard(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::Alerted, true);
                    return Dg::Continue(1);
                case 1:
                    if (ctx.Bb().GetOr(Keys::Alerted, false)) {
                        return Dg::Complete();
                    }

                    return Dg::Fail(300);
                default:
                    return Dg::Fail(301);
                }
            }

            [[nodiscard]] FrameControl RootFallbackBranch(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Bb().GetOr(Keys::Alerted, false)) {
                        return Dg::Complete();
                    }

                    return Dg::Fail(302);
                default:
                    return Dg::Fail(303);
                }
            }

            [[nodiscard]] FrameControl RootParentChildBlackboard(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::Alerted, true);
                    ctx.Bb().Set(Keys::Counter, 1);
                    return Dg::Push(FrameId::ChildReadParentBool, 1);
                case 1:
                    if (!ctx.Bb().GetOr(Keys::ChildSawAlerted, false)) {
                        return Dg::Fail(304);
                    }

                    return Dg::Push(FrameId::ChildWriteParentCounter, 2);
                case 2:
                    if (ctx.Bb().GetOr(Keys::Counter, 0) == 5) {
                        return Dg::Complete();
                    }

                    return Dg::Fail(305);
                default:
                    return Dg::Fail(306);
                }
            }

            [[nodiscard]] FrameControl ChildPop(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Pop();
                default:
                    return Dg::Fail(105);
                }
            }

            [[nodiscard]] FrameControl ChildFail(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Fail(201);
                default:
                    return Dg::Fail(202);
                }
            }

            [[nodiscard]] FrameControl ChildReadParentBool(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    if (ctx.Bb().GetOr(Keys::Alerted, false)) {
                        ctx.Bb().Set(Keys::ChildSawAlerted, true);
                        return Dg::Pop();
                    }

                    return Dg::Fail(307);
                default:
                    return Dg::Fail(308);
                }
            }

            [[nodiscard]] FrameControl ChildWriteParentCounter(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::Counter, 5);
                    return Dg::Pop();
                default:
                    return Dg::Fail(309);
                }
            }

            [[nodiscard]] FrameControl RecoveryComplete(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Complete();
                default:
                    return Dg::Fail(106);
                }
            }

            [[nodiscard]] FrameControl RootMailboxConsumeFifo(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.PcAs<RootMailboxConsumePhase>()) {
                case RootMailboxConsumePhase::ConsumeFirst:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, RootMailboxConsumePhase::ConsumeFirst);
                    }

                    ctx.Bb().Set(Keys::FirstMessageValue, message.value);
                    return Dg::Continue(RootMailboxConsumePhase::ConsumeSecond);
                case RootMailboxConsumePhase::ConsumeSecond:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, RootMailboxConsumePhase::ConsumeSecond);
                    }

                    ctx.Bb().Set(Keys::SecondMessageValue, message.value);
                    return Dg::Complete();
                default:
                    return Dg::Fail(400);
                }
            }

            [[nodiscard]] FrameControl RootTypedPhaseMailboxAct(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.PcAs<RootTypedPhaseMailboxActPhase>()) {
                case RootTypedPhaseMailboxActPhase::AwaitSignal:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, RootTypedPhaseMailboxActPhase::AwaitSignal);
                    }

                    if (message.kind != MessageKind::Signal) {
                        return Dg::Fail(706);
                    }

                    ctx.Bb().Set(Keys::FirstMessageValue, message.value);
                    ctx.Act().Deferred(ActId::RaiseAlarm, 1);
                    return Dg::Continue(RootTypedPhaseMailboxActPhase::AwaitAlert);
                case RootTypedPhaseMailboxActPhase::AwaitAlert:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::Stay();
                    }

                    if (message.kind != MessageKind::Alert) {
                        return Dg::Fail(707);
                    }

                    ctx.Bb().Set(Keys::SecondMessageValue, message.value);
                    return Dg::Complete();
                default:
                    return Dg::Fail(708);
                }
            }

            [[nodiscard]] FrameControl RootMailboxPeekThenConsume(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Mb().PeekFront(message)) {
                        return Dg::WaitTicks(1, 0);
                    }

                    ctx.Bb().Set(Keys::SeenPeekValue, message.value);
                    return Dg::Continue(1);
                case 1:
                    if (!ctx.Mb().PeekFront(message)) {
                        return Dg::Fail(401);
                    }

                    if (ctx.Bb().GetOr(Keys::SeenPeekValue, -1) != message.value) {
                        return Dg::Fail(402);
                    }

                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::Fail(403);
                    }

                    return Dg::Complete();
                default:
                    return Dg::Fail(404);
                }
            }

            [[nodiscard]] FrameControl RootMailboxParentPushChildConsume(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 0);
                    }

                    ctx.Bb().Set(Keys::FirstMessageValue, message.value);
                    return Dg::Push(FrameId::ChildMailboxConsumeAndPop, 1);
                case 1:
                    return Dg::Complete();
                default:
                    return Dg::Fail(405);
                }
            }

            [[nodiscard]] FrameControl RootMailboxEnqueueDuringTick(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 0);
                    }

                    ctx.Bb().Set(Keys::FirstMessageValue, message.value);
                    ctx.Bb().Set(Keys::MailboxTriggered, true);
                    ctx.Mb().Enqueue(Message{
                        .kind = MessageKind::Alert,
                        .value = 22
                    });
                    return Dg::Continue(1);
                case 1:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 1);
                    }

                    if (message.kind != MessageKind::Alert || message.value != 22) {
                        return Dg::Fail(406);
                    }

                    ctx.Bb().Set(Keys::SecondMessageValue, message.value);
                    return Dg::Complete();
                default:
                    return Dg::Fail(407);
                }
            }

            [[nodiscard]] FrameControl ChildMailboxConsumeAndPop(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 0);
                    }

                    ctx.Bb().Set(Keys::SecondMessageValue, message.value);
                    return Dg::Pop();
                default:
                    return Dg::Fail(408);
                }
            }

            [[nodiscard]] FrameControl RootUtilityHighestScore(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::AlertScore, 10);
                    ctx.Bb().Set(Keys::LowAmmoScore, 80);
                    ctx.Bb().Set(Keys::UtilityDecisionsMade, 0);
                    return Dg::Continue(1);
                case 1:
                    if (ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) >= 1) {
                        return Dg::Complete();
                    }

                    return Dg::Decide(
                        ctx,
                        {
                            Dg::when(FrameId::UtilityActionCombat, When::Alerted),
                            Dg::when(FrameId::UtilityActionReload, When::LowAmmo),
                            Dg::when(FrameId::UtilityActionPatrol, When::Always)
                        },
                        Dg::DecideOptions{ .tieBreak = Dg::TieBreakPolicy::FirstListed });
                default:
                    return Dg::Fail(500);
                }
            }

            [[nodiscard]] FrameControl RootUtilityHysteresis(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::AlertScore, 70);
                    ctx.Bb().Set(Keys::LowAmmoScore, 65);
                    ctx.Bb().Set(Keys::UtilityDecisionsMade, 0);
                    return Dg::Continue(1);
                case 1:
                    if (ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) == 1) {
                        ctx.Bb().Set(Keys::AlertScore, 70);
                        ctx.Bb().Set(Keys::LowAmmoScore, 74);
                    }

                    if (ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) >= 2) {
                        return Dg::Complete();
                    }

                    return Dg::Decide(
                        ctx,
                        {
                            Dg::when(FrameId::UtilityActionCombat, When::Alerted),
                            Dg::when(FrameId::UtilityActionReload, When::LowAmmo),
                            Dg::when(FrameId::UtilityActionPatrol, When::Always)
                        },
                        Dg::DecideOptions{
                            .hysteresis = 0.10f,
                            .tieBreak = Dg::TieBreakPolicy::KeepCurrent
                        });
                default:
                    return Dg::Fail(501);
                }
            }

            [[nodiscard]] FrameControl RootUtilityMinCommit(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::AlertScore, 80);
                    ctx.Bb().Set(Keys::LowAmmoScore, 10);
                    ctx.Bb().Set(Keys::UtilityDecisionsMade, 0);
                    return Dg::Continue(1);
                case 1:
                    if (ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) >= 1) {
                        ctx.Bb().Set(Keys::AlertScore, 20);
                        ctx.Bb().Set(Keys::LowAmmoScore, 90);
                    }

                    if (ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) >= 4) {
                        return Dg::Complete();
                    }

                    return Dg::Decide(
                        ctx,
                        {
                            Dg::when(FrameId::UtilityActionCombat, When::Alerted),
                            Dg::when(FrameId::UtilityActionReload, When::LowAmmo),
                            Dg::when(FrameId::UtilityActionPatrol, When::Always)
                        },
                        Dg::DecideOptions{
                            .minCommitTicks = 2,
                            .tieBreak = Dg::TieBreakPolicy::KeepCurrent
                        });
                default:
                    return Dg::Fail(502);
                }
            }

            [[nodiscard]] FrameControl RootUtilityTieBreakKeepCurrent(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::AlertScore, 50);
                    ctx.Bb().Set(Keys::LowAmmoScore, 20);
                    ctx.Bb().Set(Keys::UtilityDecisionsMade, 0);
                    return Dg::Continue(1);
                case 1:
                    if (ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) == 1) {
                        ctx.Bb().Set(Keys::LowAmmoScore, 50);
                    }

                    if (ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) >= 2) {
                        return Dg::Complete();
                    }

                    return Dg::Decide(
                        ctx,
                        {
                            Dg::when(FrameId::UtilityActionCombat, When::Alerted),
                            Dg::when(FrameId::UtilityActionReload, When::LowAmmo)
                        },
                        Dg::DecideOptions{ .tieBreak = Dg::TieBreakPolicy::KeepCurrent });
                default:
                    return Dg::Fail(503);
                }
            }

            [[nodiscard]] FrameControl RootUtilityTieBreakFirstListed(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::AlertScore, 60);
                    ctx.Bb().Set(Keys::LowAmmoScore, 60);
                    return Dg::Decide(
                        ctx,
                        {
                            Dg::when(FrameId::UtilityActionCombat, When::Alerted),
                            Dg::when(FrameId::UtilityActionReload, When::LowAmmo)
                        },
                        Dg::DecideOptions{ .tieBreak = Dg::TieBreakPolicy::FirstListed });
                case 1:
                    return Dg::Complete();
                default:
                    return Dg::Fail(504);
                }
            }

            [[nodiscard]] FrameControl RootUtilityTieBreakLastListed(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::AlertScore, 60);
                    ctx.Bb().Set(Keys::LowAmmoScore, 60);
                    return Dg::Decide(
                        ctx,
                        {
                            Dg::when(FrameId::UtilityActionCombat, When::Alerted),
                            Dg::when(FrameId::UtilityActionReload, When::LowAmmo)
                        },
                        Dg::DecideOptions{ .tieBreak = Dg::TieBreakPolicy::LastListed });
                case 1:
                    return Dg::Complete();
                default:
                    return Dg::Fail(505);
                }
            }

            [[nodiscard]] FrameControl UtilityActionCombat(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::UtilityChoice, 1);
                    ctx.Bb().Set(Keys::UtilityDecisionsMade, ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) + 1);
                    return Dg::Pop();
                default:
                    return Dg::Fail(506);
                }
            }

            [[nodiscard]] FrameControl UtilityActionReload(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::UtilityChoice, 2);
                    ctx.Bb().Set(Keys::UtilityDecisionsMade, ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) + 1);
                    return Dg::Pop();
                default:
                    return Dg::Fail(507);
                }
            }

            [[nodiscard]] FrameControl UtilityActionPatrol(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::UtilityChoice, 3);
                    ctx.Bb().Set(Keys::UtilityDecisionsMade, ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) + 1);
                    return Dg::Pop();
                default:
                    return Dg::Fail(508);
                }
            }

            [[nodiscard]] FrameControl RootActImmediateDeferred(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Act().Immediate(ActId::PlayBark);
                    return Dg::WaitTicks(2, 1);
                case 1:
                    ctx.Act().Deferred(ActId::RaiseAlarm, 3);
                    return Dg::Complete();
                default:
                    return Dg::Fail(700);
                }
            }

            [[nodiscard]] FrameControl RootActOrderedDeferred(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Act().Immediate(ActId::OpenDoor);
                    ctx.Act().Deferred(ActId::PlayBark, 2);
                    ctx.Act().Deferred(ActId::RaiseAlarm, 2);
                    return Dg::Complete();
                default:
                    return Dg::Fail(701);
                }
            }

            [[nodiscard]] FrameControl RootActParentPushChild(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    ctx.Act().Immediate(ActId::OpenDoor);
                    if (ctx.Mb().ConsumeFront(message)) {
                        ctx.Bb().Set(Keys::ActMailboxSeen, true);
                    }

                    ctx.Bb().Set(Keys::Counter, 1);
                    return Dg::Push(FrameId::ChildActImmediate, 1);
                case 1:
                    if (ctx.Bb().GetOr(Keys::Counter, 0) != 2) {
                        return Dg::Fail(702);
                    }

                    return Dg::Complete();
                default:
                    return Dg::Fail(703);
                }
            }

            [[nodiscard]] FrameControl ChildActImmediate(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Act().Immediate(ActId::PlayBark);
                    ctx.Bb().Set(Keys::Counter, 2);
                    return Dg::Pop();
                default:
                    return Dg::Fail(704);
                }
            }

            [[nodiscard]] FrameControl RootActUtilityDriven(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::AlertScore, 80);
                    ctx.Bb().Set(Keys::LowAmmoScore, 20);
                    return Dg::Continue(1);
                case 1:
                    if (When::Alerted(ctx) >= When::LowAmmo(ctx)) {
                        ctx.Act().Immediate(ActId::UtilityCombat);
                    } else {
                        ctx.Act().Immediate(ActId::UtilityReload);
                    }

                    return Dg::Complete();
                default:
                    return Dg::Fail(705);
                }
            }
        }
    }

    ActCtx::ActCtx() = default;

    ActCtx::ActCtx(ActRuntime& runtime)
        : runtime_(&runtime)
    {
    }

    void ActCtx::Immediate(ActId id)
    {
        if (runtime_ == nullptr) {
            return;
        }

        runtime_->EmitImmediate(id);
    }

    void ActCtx::Deferred(ActId id, std::uint32_t delayTicks)
    {
        if (runtime_ == nullptr) {
            return;
        }

        runtime_->ScheduleDeferred(id, delayTicks);
    }

    FrameCtx::FrameCtx(FrameId frameId, TickIndex tick, std::uint32_t pc, bool entered, Blackboard& blackboard)
        : frameId_(frameId)
        , tick_(tick)
        , pc_(pc)
        , entered_(entered)
        , blackboard_(&blackboard)
    {
    }

    FrameCtx::FrameCtx(FrameId frameId, TickIndex tick, std::uint32_t pc, bool entered, Blackboard& blackboard, Mailbox& mailbox)
        : frameId_(frameId)
        , tick_(tick)
        , pc_(pc)
        , entered_(entered)
        , blackboard_(&blackboard)
        , mailbox_(&mailbox)
    {
    }

    FrameCtx::FrameCtx(
        FrameId frameId,
        TickIndex tick,
        std::uint32_t pc,
        bool entered,
        Blackboard& blackboard,
        Mailbox& mailbox,
        ActRuntime& actRuntime,
        UtilityMemoryStore& utilityMemory,
        std::vector<UtilityDecisionTraceEntry>& utilityTrace)
        : frameId_(frameId)
        , tick_(tick)
        , pc_(pc)
        , entered_(entered)
        , blackboard_(&blackboard)
        , mailbox_(&mailbox)
        , act_(actRuntime)
        , utilityMemory_(&utilityMemory)
        , utilityTrace_(&utilityTrace)
    {
    }

    [[nodiscard]] FrameId FrameCtx::Id() const
    {
        return frameId_;
    }

    [[nodiscard]] TickIndex FrameCtx::Tick() const
    {
        return tick_;
    }

    [[nodiscard]] std::uint32_t FrameCtx::Pc() const
    {
        return pc_;
    }

    [[nodiscard]] bool FrameCtx::Entered() const
    {
        return entered_;
    }

    [[nodiscard]] Blackboard& FrameCtx::Bb()
    {
        return *blackboard_;
    }

    [[nodiscard]] const Blackboard& FrameCtx::Bb() const
    {
        return *blackboard_;
    }

    [[nodiscard]] Mailbox& FrameCtx::Mb()
    {
        return *mailbox_;
    }

    [[nodiscard]] const Mailbox& FrameCtx::Mb() const
    {
        return *mailbox_;
    }

    [[nodiscard]] ActCtx& FrameCtx::Act()
    {
        return act_;
    }

    [[nodiscard]] const ActCtx& FrameCtx::Act() const
    {
        return act_;
    }

    [[nodiscard]] std::uint32_t FrameCtx::ReadUtilityCommitAge(FrameId frameId) const
    {
        if (utilityMemory_ == nullptr) {
            return 0;
        }

        const UtilityMemoryStore::Entry* entry = utilityMemory_->Find(frameId);
        if (entry == nullptr || !entry->hasCommitted) {
            return 0;
        }

        return entry->age;
    }

    void Mailbox::Enqueue(const Message& message)
    {
        staged_.push_back(message);
    }

    void Mailbox::BeginTick()
    {
        // M3 visibility rule:
        // - Messages enqueued before BeginTick are visible during this tick.
        // - Messages enqueued during frame execution are staged and become visible next tick.
        for (const Message& message : staged_) {
            visible_.push_back(message);
        }

        staged_.clear();
    }

    [[nodiscard]] bool Mailbox::HasMessage() const
    {
        return !visible_.empty();
    }

    [[nodiscard]] bool Mailbox::PeekFront(Message& message) const
    {
        if (visible_.empty()) {
            return false;
        }

        message = visible_.front();
        return true;
    }

    [[nodiscard]] bool Mailbox::ConsumeFront(Message& message)
    {
        if (visible_.empty()) {
            return false;
        }

        message = visible_.front();
        visible_.erase(visible_.begin());
        return true;
    }

    [[nodiscard]] const std::vector<Message>& Mailbox::VisibleMessages() const
    {
        return visible_;
    }

    [[nodiscard]] const std::vector<Message>& Mailbox::StagedMessages() const
    {
        return staged_;
    }

    [[nodiscard]] MailboxChunk Mailbox::ExportChunk() const
    {
        return MailboxChunk{
            .visibleMessages = visible_,
            .stagedMessages = staged_
        };
    }

    void Mailbox::ImportChunk(const MailboxChunk& chunk)
    {
        visible_ = chunk.visibleMessages;
        staged_ = chunk.stagedMessages;
    }

    template <>
    [[nodiscard]] const bool* Blackboard::FindValue<bool>(std::uint32_t slot) const
    {
        for (const BoolEntry& entry : boolEntries_) {
            if (entry.slot == slot) {
                return &entry.value;
            }
        }

        return nullptr;
    }

    template <>
    [[nodiscard]] const int* Blackboard::FindValue<int>(std::uint32_t slot) const
    {
        for (const IntEntry& entry : intEntries_) {
            if (entry.slot == slot) {
                return &entry.value;
            }
        }

        return nullptr;
    }

    template <>
    void Blackboard::UpsertValue<bool>(std::uint32_t slot, const bool& value)
    {
        for (BoolEntry& entry : boolEntries_) {
            if (entry.slot == slot) {
                entry.value = value;
                return;
            }
        }

        boolEntries_.push_back(BoolEntry{
            .slot = slot,
            .value = value
        });
    }

    template <>
    void Blackboard::UpsertValue<int>(std::uint32_t slot, const int& value)
    {
        for (IntEntry& entry : intEntries_) {
            if (entry.slot == slot) {
                entry.value = value;
                return;
            }
        }

        intEntries_.push_back(IntEntry{
            .slot = slot,
            .value = value
        });
    }

    [[nodiscard]] const std::vector<std::uint32_t>& Blackboard::DirtySlots() const
    {
        return dirtySlots_;
    }

    void Blackboard::ClearDirty()
    {
        dirtySlots_.clear();
    }

    [[nodiscard]] Blackboard::Chunk Blackboard::ExportChunk() const
    {
        Chunk chunk;

        for (const BoolEntry& entry : boolEntries_) {
            chunk.boolEntries.push_back(BoolChunkEntry{
                .slot = entry.slot,
                .value = entry.value
            });
        }

        for (const IntEntry& entry : intEntries_) {
            chunk.intEntries.push_back(IntChunkEntry{
                .slot = entry.slot,
                .value = entry.value
            });
        }

        chunk.dirtySlots = dirtySlots_;
        return chunk;
    }

    void Blackboard::ImportChunk(const Chunk& chunk)
    {
        boolEntries_.clear();
        intEntries_.clear();
        dirtySlots_.clear();

        for (const BoolChunkEntry& entry : chunk.boolEntries) {
            boolEntries_.push_back(BoolEntry{
                .slot = entry.slot,
                .value = entry.value
            });
        }

        for (const IntChunkEntry& entry : chunk.intEntries) {
            intEntries_.push_back(IntEntry{
                .slot = entry.slot,
                .value = entry.value
            });
        }

        dirtySlots_ = chunk.dirtySlots;
    }

    void Blackboard::MarkDirty(std::uint32_t slot)
    {
        if (HasDirtySlot(slot)) {
            return;
        }

        dirtySlots_.push_back(slot);
    }

    [[nodiscard]] bool Blackboard::HasDirtySlot(std::uint32_t slot) const
    {
        for (const std::uint32_t dirtySlot : dirtySlots_) {
            if (dirtySlot == slot) {
                return true;
            }
        }

        return false;
    }

    void FrameRegistry::Add(FrameId id, FrameFn function)
    {
        definitions_.push_back(FrameDef{
            .id = id,
            .function = function
        });
    }

    [[nodiscard]] FrameFn FrameRegistry::Find(FrameId id) const
    {
        for (const FrameDef& definition : definitions_) {
            if (definition.id == id) {
                return definition.function;
            }
        }

        return nullptr;
    }

    struct DecideAccess
    {
        [[nodiscard]] static UtilityMemoryStore* Memory(FrameCtx& ctx)
        {
            return ctx.utilityMemory_;
        }

        [[nodiscard]] static std::vector<UtilityDecisionTraceEntry>* Trace(FrameCtx& ctx)
        {
            return ctx.utilityTrace_;
        }
    };

    namespace Dg
    {
        [[nodiscard]] UtilityCandidate when(FrameId target, ConsiderationFn consideration)
        {
            return UtilityCandidate{
                .target = target,
                .consideration = consideration
            };
        }

        [[nodiscard]] FrameControl Decide(
            FrameCtx& ctx,
            std::initializer_list<UtilityCandidate> candidates,
            DecideOptions options)
        {
            if (candidates.size() == 0) {
                return Fail(600);
            }

            UtilityMemoryStore* memory = DecideAccess::Memory(ctx);
            std::vector<UtilityDecisionTraceEntry>* trace = DecideAccess::Trace(ctx);
            if (memory == nullptr || trace == nullptr) {
                return Fail(601);
            }

            UtilityDecisionTraceEntry traceEntry;
            traceEntry.decisionFrame = ctx.Id();
            UtilityMemoryStore::Entry& memoryEntry = memory->Ensure(ctx.Id());
            traceEntry.hadCommittedBefore = memoryEntry.hasCommitted;
            traceEntry.committedBefore = memoryEntry.committed;
            traceEntry.committedAgeBefore = memoryEntry.age;

            float bestScore = -1.0f;
            std::vector<std::size_t> bestIndices;
            std::size_t index = 0;
            for (const UtilityCandidate& candidate : candidates) {
                float score = 0.0f;
                if (candidate.consideration != nullptr) {
                    score = ClampScore01(candidate.consideration(ctx));
                }

                traceEntry.candidates.push_back(UtilityDecisionCandidateTrace{
                    .target = candidate.target,
                    .score = score
                });

                if (score > bestScore) {
                    bestScore = score;
                    bestIndices.clear();
                    bestIndices.push_back(index);
                } else if (score == bestScore) {
                    bestIndices.push_back(index);
                }

                ++index;
            }

            std::size_t winnerIndex = bestIndices.front();
            if (bestIndices.size() > 1) {
                traceEntry.tieBreakUsed = true;
                if (options.tieBreak == TieBreakPolicy::LastListed) {
                    winnerIndex = bestIndices.back();
                } else if (options.tieBreak == TieBreakPolicy::KeepCurrent && memoryEntry.hasCommitted) {
                    for (const std::size_t tiedIndex : bestIndices) {
                        if (traceEntry.candidates[tiedIndex].target == memoryEntry.committed) {
                            winnerIndex = tiedIndex;
                            break;
                        }
                    }
                }
            }

            FrameId winner = traceEntry.candidates[winnerIndex].target;
            const float winnerScore = traceEntry.candidates[winnerIndex].score;
            float committedScore = 0.0f;
            bool hasCommittedCandidate = false;

            if (memoryEntry.hasCommitted) {
                for (const UtilityDecisionCandidateTrace& candidate : traceEntry.candidates) {
                    if (candidate.target == memoryEntry.committed) {
                        committedScore = candidate.score;
                        hasCommittedCandidate = true;
                        break;
                    }
                }
            }

            if (memoryEntry.hasCommitted && hasCommittedCandidate && winner != memoryEntry.committed) {
                if (options.minCommitTicks > 0 && memoryEntry.age < static_cast<std::uint32_t>(options.minCommitTicks)) {
                    winner = memoryEntry.committed;
                    traceEntry.minCommitBlocked = true;
                } else if (winnerScore < committedScore + options.hysteresis) {
                    winner = memoryEntry.committed;
                    traceEntry.hysteresisBlocked = true;
                }
            }

            if (memoryEntry.hasCommitted && winner == memoryEntry.committed) {
                ++memoryEntry.age;
            } else {
                memoryEntry.hasCommitted = true;
                memoryEntry.committed = winner;
                memoryEntry.age = 0;
            }

            traceEntry.chosen = winner;
            trace->push_back(traceEntry);
            return Push(winner, ctx.Pc());
        }

        [[nodiscard]] FrameControl Continue(std::uint32_t resumePc)
        {
            return FrameControl{
                .kind = FrameControlKind::Continue,
                .resumePc = resumePc
            };
        }

        [[nodiscard]] FrameControl WaitTicks(std::uint32_t ticks, std::uint32_t resumePc)
        {
            return FrameControl{
                .kind = FrameControlKind::Wait,
                .resumePc = resumePc,
                .waitTicks = ticks
            };
        }

        [[nodiscard]] FrameControl Stay()
        {
            return FrameControl{
                .kind = FrameControlKind::Continue,
                .stayOnCurrentPc = true
            };
        }

        [[nodiscard]] FrameControl Push(FrameId target, std::uint32_t resumePc)
        {
            return FrameControl{
                .kind = FrameControlKind::Push,
                .resumePc = resumePc,
                .target = target
            };
        }

        [[nodiscard]] FrameControl Pop()
        {
            return FrameControl{
                .kind = FrameControlKind::Pop
            };
        }

        [[nodiscard]] FrameControl Replace(FrameId target)
        {
            return FrameControl{
                .kind = FrameControlKind::Replace,
                .target = target
            };
        }

        [[nodiscard]] FrameControl Complete()
        {
            return FrameControl{
                .kind = FrameControlKind::Complete
            };
        }

        [[nodiscard]] FrameControl Fail(int reason)
        {
            return FrameControl{
                .kind = FrameControlKind::Fail,
                .failReason = reason
            };
        }
    }

    namespace When
    {
        [[nodiscard]] float Always(const FrameCtx&)
        {
            return 0.25f;
        }

        [[nodiscard]] float Alerted(const FrameCtx& ctx)
        {
            namespace Keys = nodes::Keys;
            const int raw = ctx.Bb().GetOr(Keys::AlertScore, 0);
            return ClampScore01(static_cast<float>(raw) / 100.0f);
        }

        [[nodiscard]] float LowAmmo(const FrameCtx& ctx)
        {
            namespace Keys = nodes::Keys;
            const int raw = ctx.Bb().GetOr(Keys::LowAmmoScore, 0);
            return ClampScore01(static_cast<float>(raw) / 100.0f);
        }
    }

    StackFrameRuntimeSession::StackFrameRuntimeSession(
        StackScriptScenario scenario,
        const RuntimeMailboxInput& mailboxInput)
        : scenario_(scenario)
        , registry_(BuildRegistry())
        , scheduledMessages_(mailboxInput.scheduledMessages)
        , actRuntime_(std::make_unique<ActRuntime>())
        , utilityMemory_(std::make_unique<UtilityMemoryStore>())
    {
        stack_.push_back(StackFrameChunkEntry{ .id = ScenarioRootFrame(scenario_) });
        for (const Message& message : mailboxInput.initialMessages) {
            mailbox_.Enqueue(message);
        }
    }

    StackFrameRuntimeSession::StackFrameRuntimeSession(const RuntimeChunk& chunk)
        : scenario_(chunk.scenario)
        , nextTick_(chunk.nextTick)
        , lastOutcome_(chunk.lastOutcome)
        , registry_(BuildRegistry())
        , actRuntime_(std::make_unique<ActRuntime>())
        , utilityMemory_(std::make_unique<UtilityMemoryStore>())
    {
        stack_ = chunk.stack.frames;
        actRuntime_->ImportDeferredChunk(chunk.deferredActuation);
        utilityMemory_->ImportChunk(chunk.utilityMemory);
        blackboard_.ImportChunk(chunk.blackboard);
        mailbox_.ImportChunk(chunk.mailbox);
        scheduledMessages_ = chunk.scheduledMessages;
    }

    StackFrameRuntimeSession::~StackFrameRuntimeSession() = default;

    [[nodiscard]] TickIndex StackFrameRuntimeSession::NextTick() const
    {
        return nextTick_;
    }

    [[nodiscard]] StackRunOutcome StackFrameRuntimeSession::LastOutcome() const
    {
        return lastOutcome_;
    }

    [[nodiscard]] bool StackFrameRuntimeSession::IsTerminal() const
    {
        if (lastOutcome_ == StackRunOutcome::Failed) {
            return true;
        }

        if (lastOutcome_ == StackRunOutcome::Completed) {
            return actRuntime_->Pending().empty();
        }

        return false;
    }

    [[nodiscard]] RuntimeChunk StackFrameRuntimeSession::Save() const
    {
        // M4 persistence boundary:
        // Save() is valid between ticks after all effects of tick N are complete and before tick N+1 starts.
        return RuntimeChunk{
            .scenario = scenario_,
            .nextTick = nextTick_,
            .lastOutcome = lastOutcome_,
            .scheduledMessages = scheduledMessages_,
            .stack = StackChunk{ .frames = stack_ },
            .utilityMemory = utilityMemory_->ExportChunk(),
            .deferredActuation = actRuntime_->ExportDeferredChunk(),
            .blackboard = blackboard_.ExportChunk(),
            .mailbox = mailbox_.ExportChunk()
        };
    }

    [[nodiscard]] FrameRunResult StackFrameRuntimeSession::RunForTicks(TickIndex tickCount)
    {
        FrameRunResult result;
        for (TickIndex tick = 0; tick < tickCount; ++tick) {
            if (!RunSingleTick(result)) {
                break;
            }
        }

        result.finalOutcome = lastOutcome_;
        result.finalBlackboard = blackboard_;
        return result;
    }

    [[nodiscard]] bool StackFrameRuntimeSession::RunSingleTick(FrameRunResult& result)
    {
        if (IsTerminal()) {
            return false;
        }

        // M2b + M4 dirty persistence contract:
        // dirty slots represent writes in the currently executing tick only.
        // Save() at the boundary persists the exact dirty slots from the most recently completed tick.
        blackboard_.ClearDirty();

        for (const ScheduledMessage& scheduled : scheduledMessages_) {
            if (scheduled.tick == nextTick_) {
                mailbox_.Enqueue(scheduled.message);
            }
        }

        mailbox_.BeginTick();
        actRuntime_->BeginTick(nextTick_);
        actRuntime_->FlushMatured();
        result.visibleMailboxByTick.push_back(mailbox_.VisibleMessages());
        std::vector<UtilityDecisionTraceEntry> tickUtilityDecisions;

        if (stack_.empty()) {
            lastOutcome_ = StackRunOutcome::Completed;
            result.dirtySlotsByTick.push_back(blackboard_.DirtySlots());
            result.actuationByTick.push_back(actRuntime_->EmittedNow());
            result.tickTrace.push_back(MakeTickTraceEntry(
                nextTick_,
                lastOutcome_,
                stack_,
                blackboard_.DirtySlots(),
                mailbox_.VisibleMessages(),
                tickUtilityDecisions,
                actRuntime_->EmittedNow(),
                actRuntime_->Pending()));
            ++nextTick_;
            return !IsTerminal();
        }

        StackFrameChunkEntry& frame = stack_.back();
        EmitTrace(result, nextTick_, FrameTraceKind::Tick, frame, FrameControlKind::Continue, frame.id, stack_.size());

        if (!frame.entered) {
            frame.entered = true;
            EmitTrace(result, nextTick_, FrameTraceKind::Enter, frame, FrameControlKind::Continue, frame.id, stack_.size());
        }

        if (frame.remainingWaitTicks > 0) {
            --frame.remainingWaitTicks;
            EmitTrace(result, nextTick_, FrameTraceKind::Step, frame, FrameControlKind::Wait, frame.id, stack_.size());
            lastOutcome_ = StackRunOutcome::Wait;
            result.dirtySlotsByTick.push_back(blackboard_.DirtySlots());
            result.actuationByTick.push_back(actRuntime_->EmittedNow());
            result.tickTrace.push_back(MakeTickTraceEntry(
                nextTick_,
                lastOutcome_,
                stack_,
                blackboard_.DirtySlots(),
                mailbox_.VisibleMessages(),
                tickUtilityDecisions,
                actRuntime_->EmittedNow(),
                actRuntime_->Pending()));
            ++nextTick_;
            return true;
        }

        FrameCtx ctx(frame.id, nextTick_, frame.pc, frame.entered, blackboard_, mailbox_, *actRuntime_, *utilityMemory_, tickUtilityDecisions);
        const FrameFn frameFunction = registry_.Find(frame.id);
        if (frameFunction == nullptr) {
            EmitTrace(result, nextTick_, FrameTraceKind::ExitFailed, frame, FrameControlKind::Fail, frame.id, stack_.size());
            EmitTrace(result, nextTick_, FrameTraceKind::TerminalFailed, frame, FrameControlKind::Fail, frame.id, stack_.size());
            lastOutcome_ = StackRunOutcome::Failed;
            result.dirtySlotsByTick.push_back(blackboard_.DirtySlots());
            result.actuationByTick.push_back(actRuntime_->EmittedNow());
            result.tickTrace.push_back(MakeTickTraceEntry(
                nextTick_,
                lastOutcome_,
                stack_,
                blackboard_.DirtySlots(),
                mailbox_.VisibleMessages(),
                tickUtilityDecisions,
                actRuntime_->EmittedNow(),
                actRuntime_->Pending()));
            ++nextTick_;
            return false;
        }

        const FrameControl control = frameFunction(ctx);
        EmitTrace(result, nextTick_, FrameTraceKind::Step, frame, control.kind, control.target, stack_.size());

        if (control.kind == FrameControlKind::Continue) {
            if (!control.stayOnCurrentPc) {
                frame.pc = control.resumePc;
            }
            lastOutcome_ = StackRunOutcome::Continue;
        } else if (control.kind == FrameControlKind::Wait) {
            frame.pc = control.resumePc;
            frame.remainingWaitTicks = control.waitTicks > 0 ? control.waitTicks - 1 : 0;
            lastOutcome_ = StackRunOutcome::Wait;
        } else if (control.kind == FrameControlKind::Push) {
            frame.pc = control.resumePc;
            EmitTrace(result, nextTick_, FrameTraceKind::Push, frame, control.kind, control.target, stack_.size());
            stack_.push_back(StackFrameChunkEntry{ .id = control.target });
            lastOutcome_ = StackRunOutcome::Continue;
        } else if (control.kind == FrameControlKind::Replace) {
            const std::size_t depthBeforeReplace = stack_.size();
            const StackFrameChunkEntry replaced = frame;
            EmitTrace(result, nextTick_, FrameTraceKind::Replace, replaced, control.kind, control.target, depthBeforeReplace);
            stack_.pop_back();
            stack_.push_back(StackFrameChunkEntry{ .id = control.target });
            lastOutcome_ = StackRunOutcome::Continue;
        } else if (control.kind == FrameControlKind::Pop || control.kind == FrameControlKind::Complete) {
            const std::size_t depthBeforePop = stack_.size();
            const StackFrameChunkEntry completedFrame = frame;
            EmitTrace(result, nextTick_, FrameTraceKind::ExitCompleted, completedFrame, control.kind, completedFrame.id, depthBeforePop);
            stack_.pop_back();
            EmitTrace(result, nextTick_, FrameTraceKind::Pop, completedFrame, control.kind, completedFrame.id, depthBeforePop);

            if (stack_.empty()) {
                lastOutcome_ = StackRunOutcome::Completed;
                EmitTrace(result, nextTick_, FrameTraceKind::TerminalCompleted, completedFrame, control.kind, completedFrame.id, 0);
            } else {
                lastOutcome_ = StackRunOutcome::Continue;
            }
        } else {
            const std::size_t depthBeforeFail = stack_.size();
            const StackFrameChunkEntry failedFrame = frame;
            EmitTrace(result, nextTick_, FrameTraceKind::ExitFailed, failedFrame, control.kind, failedFrame.id, depthBeforeFail);
            EmitTrace(result, nextTick_, FrameTraceKind::TerminalFailed, failedFrame, control.kind, failedFrame.id, depthBeforeFail);
            lastOutcome_ = StackRunOutcome::Failed;
        }

        result.dirtySlotsByTick.push_back(blackboard_.DirtySlots());
        result.actuationByTick.push_back(actRuntime_->EmittedNow());
        result.tickTrace.push_back(MakeTickTraceEntry(
            nextTick_,
            lastOutcome_,
            stack_,
            blackboard_.DirtySlots(),
            mailbox_.VisibleMessages(),
            tickUtilityDecisions,
            actRuntime_->EmittedNow(),
            actRuntime_->Pending()));
        ++nextTick_;
        return !IsTerminal();
    }

    [[nodiscard]] FrameRegistry StackFrameRuntimeSession::BuildRegistry()
    {
        FrameRegistry registry;
        registry.Add(FrameId::RootPushChild, &nodes::RootPushChild);
        registry.Add(FrameId::RootReplace, &nodes::RootReplace);
        registry.Add(FrameId::RootWaitThenPush, &nodes::RootWaitThenPush);
        registry.Add(FrameId::RootPushFailingChild, &nodes::RootPushFailingChild);
        registry.Add(FrameId::RootContinueThenComplete, &nodes::RootContinueThenComplete);
        registry.Add(FrameId::RootSetThenReadBlackboard, &nodes::RootSetThenReadBlackboard);
        registry.Add(FrameId::RootFallbackBranch, &nodes::RootFallbackBranch);
        registry.Add(FrameId::RootParentChildBlackboard, &nodes::RootParentChildBlackboard);
        registry.Add(FrameId::ChildPop, &nodes::ChildPop);
        registry.Add(FrameId::ChildFail, &nodes::ChildFail);
        registry.Add(FrameId::ChildReadParentBool, &nodes::ChildReadParentBool);
        registry.Add(FrameId::ChildWriteParentCounter, &nodes::ChildWriteParentCounter);
        registry.Add(FrameId::RecoveryComplete, &nodes::RecoveryComplete);
        registry.Add(FrameId::RootMailboxConsumeFifo, &nodes::RootMailboxConsumeFifo);
        registry.Add(FrameId::RootTypedPhaseMailboxAct, &nodes::RootTypedPhaseMailboxAct);
        registry.Add(FrameId::RootMailboxPeekThenConsume, &nodes::RootMailboxPeekThenConsume);
        registry.Add(FrameId::RootMailboxParentPushChildConsume, &nodes::RootMailboxParentPushChildConsume);
        registry.Add(FrameId::RootMailboxEnqueueDuringTick, &nodes::RootMailboxEnqueueDuringTick);
        registry.Add(FrameId::ChildMailboxConsumeAndPop, &nodes::ChildMailboxConsumeAndPop);
        registry.Add(FrameId::RootUtilityHighestScore, &nodes::RootUtilityHighestScore);
        registry.Add(FrameId::RootUtilityHysteresis, &nodes::RootUtilityHysteresis);
        registry.Add(FrameId::RootUtilityMinCommit, &nodes::RootUtilityMinCommit);
        registry.Add(FrameId::RootUtilityTieBreakKeepCurrent, &nodes::RootUtilityTieBreakKeepCurrent);
        registry.Add(FrameId::RootUtilityTieBreakFirstListed, &nodes::RootUtilityTieBreakFirstListed);
        registry.Add(FrameId::RootUtilityTieBreakLastListed, &nodes::RootUtilityTieBreakLastListed);
        registry.Add(FrameId::UtilityActionCombat, &nodes::UtilityActionCombat);
        registry.Add(FrameId::UtilityActionReload, &nodes::UtilityActionReload);
        registry.Add(FrameId::UtilityActionPatrol, &nodes::UtilityActionPatrol);
        registry.Add(FrameId::RootActImmediateDeferred, &nodes::RootActImmediateDeferred);
        registry.Add(FrameId::RootActOrderedDeferred, &nodes::RootActOrderedDeferred);
        registry.Add(FrameId::RootActParentPushChild, &nodes::RootActParentPushChild);
        registry.Add(FrameId::ChildActImmediate, &nodes::ChildActImmediate);
        registry.Add(FrameId::RootActUtilityDriven, &nodes::RootActUtilityDriven);
        return registry;
    }

    [[nodiscard]] TraceComparisonResult CompareTickTraces(
        const std::vector<TickTraceEntry>& expected,
        const std::vector<TickTraceEntry>& actual)
    {
        TraceComparisonResult comparison;
        comparison.expectedEntryCount = expected.size();
        comparison.actualEntryCount = actual.size();

        const std::size_t minSize = expected.size() < actual.size() ? expected.size() : actual.size();
        for (std::size_t index = 0; index < minSize; ++index) {
            if (!(expected[index] == actual[index])) {
                comparison.matches = false;
                comparison.firstMismatchIndex = index;
                comparison.mismatchReason = "entry content mismatch";
                comparison.expected = expected[index];
                comparison.actual = actual[index];
                return comparison;
            }
        }

        if (expected.size() != actual.size()) {
            comparison.matches = false;
            comparison.firstMismatchIndex = minSize;
            comparison.mismatchReason = "entry count mismatch";
        }

        return comparison;
    }

    [[nodiscard]] std::vector<std::string> SerializeTickTrace(const std::vector<TickTraceEntry>& trace)
    {
        std::vector<std::string> serialized;
        serialized.reserve(trace.size());

        for (const TickTraceEntry& entry : trace) {
            std::string line;
            line += "tick=" + std::to_string(entry.tick);
            line += "|outcome=" + std::to_string(static_cast<int>(entry.outcome));
            line += "|stack=";

            for (const StackFrameChunkEntry& frame : entry.stack) {
                line += std::to_string(static_cast<int>(frame.id));
                line += ",";
                line += std::to_string(frame.pc);
                line += ",";
                line += frame.entered ? "1" : "0";
                line += ",";
                line += std::to_string(frame.remainingWaitTicks);
                line += ";";
            }

            line += "|dirty=";
            for (const std::uint32_t slot : entry.dirtySlots) {
                line += std::to_string(slot);
                line += ";";
            }

            line += "|mailbox=";
            for (const Message& message : entry.visibleMailbox) {
                line += std::to_string(static_cast<int>(message.kind));
                line += ",";
                line += std::to_string(message.value);
                line += ";";
            }

            line += "|utility=";
            for (const UtilityDecisionTraceEntry& decision : entry.utilityDecisions) {
                line += std::to_string(static_cast<int>(decision.decisionFrame));
                line += ">";
                line += std::to_string(static_cast<int>(decision.chosen));
                line += ":";
                line += decision.minCommitBlocked ? "m1" : "m0";
                line += ",";
                line += decision.hysteresisBlocked ? "h1" : "h0";
                line += ",";
                line += decision.tieBreakUsed ? "t1" : "t0";
                line += "[";
                for (const UtilityDecisionCandidateTrace& candidate : decision.candidates) {
                    line += std::to_string(static_cast<int>(candidate.target));
                    line += "=";
                    line += std::to_string(candidate.score);
                    line += ";";
                }
                line += "]";
            }

            line += "|act=";
            for (const ActRequest& request : entry.emittedActuation) {
                line += std::to_string(static_cast<int>(request.id));
                line += ",";
                line += request.deferred ? "d" : "i";
                line += ",";
                line += std::to_string(request.emittedTick);
                line += ",";
                line += std::to_string(request.dueTick);
                line += ",";
                line += std::to_string(request.delayTicks);
                line += ";";
            }

            line += "|pendingAct=";
            for (const ActRequest& request : entry.pendingDeferredActuation) {
                line += std::to_string(static_cast<int>(request.id));
                line += ",";
                line += std::to_string(request.dueTick);
                line += ",";
                line += std::to_string(request.delayTicks);
                line += ";";
            }

            serialized.push_back(line);
        }

        return serialized;
    }

    [[nodiscard]] std::string FormatTraceComparison(const TraceComparisonResult& comparison)
    {
        std::string text = "matches=";
        text += comparison.matches ? "true" : "false";
        text += "\nexpectedCount=" + std::to_string(comparison.expectedEntryCount);
        text += "\nactualCount=" + std::to_string(comparison.actualEntryCount);

        if (!comparison.matches) {
            text += "\nfirstMismatchIndex=" + std::to_string(comparison.firstMismatchIndex);
            text += "\nreason=" + comparison.mismatchReason;
            const std::vector<std::string> expectedEntry = SerializeTickTrace({ comparison.expected });
            const std::vector<std::string> actualEntry = SerializeTickTrace({ comparison.actual });
            text += "\nexpectedEntry=" + (expectedEntry.empty() ? std::string("<none>") : expectedEntry[0]);
            text += "\nactualEntry=" + (actualEntry.empty() ? std::string("<none>") : actualEntry[0]);
        }

        return text;
    }

    [[nodiscard]] FrameRunResult StackFrameRuntime::RunForTicks(StackScriptScenario scenario, TickIndex tickCount) const
    {
        return RunForTicks(scenario, tickCount, RuntimeMailboxInput{});
    }

    [[nodiscard]] FrameRunResult StackFrameRuntime::RunForTicks(
        StackScriptScenario scenario,
        TickIndex tickCount,
        const RuntimeMailboxInput& mailboxInput) const
    {
        StackFrameRuntimeSession session(scenario, mailboxInput);
        return session.RunForTicks(tickCount);
    }
}
