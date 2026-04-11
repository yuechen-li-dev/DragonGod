#include "runtime_internal.h"

namespace dragongod
{
    namespace
    {
        [[nodiscard]] FrameId ScenarioRootFrameImpl(StackScriptScenario scenario)
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

    [[nodiscard]] FrameId ScenarioRootFrame(StackScriptScenario scenario)
    {
        return ScenarioRootFrameImpl(scenario);
    }

    [[nodiscard]] FrameRegistry BuildFrameRegistry()
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
}
