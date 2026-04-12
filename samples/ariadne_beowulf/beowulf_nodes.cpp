#include "beowulf_nodes.h"

#include "../../src/DragonGod/runtime.h"

#include <cstdint>

namespace dragongod_samples::ariadne_beowulf
{
    namespace
    {
        enum class NarrativePhase : std::uint32_t
        {
            EnterBarrow = 0,
            PresentChoice = 1,
            ConsumeChoice = 2,
            EnterFirstClash = 3,
            EnterRetainerCollapse = 4,
            EnterWiglafRemains = 5,
            EnterTragicHandoff = 6,
            Completed = 7
        };

        struct StoryFrameCtx
        {
            dragongod::FrameCtx& frame;
            BeowulfState& state;
            BeowulfOutput& output;

            [[nodiscard]] dragongod::Blackboard& Bb()
            {
                return frame.Bb();
            }

            [[nodiscard]] dragongod::Mailbox& Mb()
            {
                return frame.Mb();
            }

            [[nodiscard]] NarrativePhase PcAsPhase() const
            {
                return frame.PcAs<NarrativePhase>();
            }
        };

        namespace Keys
        {
            constexpr dragongod::BbKey<int> Scene{ "beowulf.scene", 200 };
            constexpr dragongod::BbKey<int> Tone{ "beowulf.tone", 201 };
            constexpr dragongod::BbKey<int> Resolve{ "beowulf.resolve", 202 };
            constexpr dragongod::BbKey<bool> ChoiceConsumed{ "beowulf.choice_consumed", 203 };
            constexpr dragongod::BbKey<bool> FirstClashBegun{ "beowulf.first_clash", 204 };
            constexpr dragongod::BbKey<int> LoyaltyPressure{ "beowulf.loyalty_pressure", 205 };
            constexpr dragongod::BbKey<bool> RetainersFled{ "beowulf.retainers_fled", 206 };
            constexpr dragongod::BbKey<bool> WiglafRemains{ "beowulf.wiglaf_remains", 207 };
            constexpr dragongod::BbKey<bool> CollapseSpoken{ "beowulf.collapse_spoken", 208 };
            constexpr dragongod::BbKey<bool> WiglafSpoken{ "beowulf.wiglaf_spoken", 209 };
        }

        void SyncStateToBlackboard(const BeowulfState& state, dragongod::Blackboard& blackboard)
        {
            blackboard.Set(Keys::Scene, static_cast<int>(state.scene));
            blackboard.Set(Keys::Tone, static_cast<int>(state.tone));
            blackboard.Set(Keys::Resolve, state.resolve);
            blackboard.Set(Keys::ChoiceConsumed, state.preBattleChoiceConsumed);
            blackboard.Set(Keys::FirstClashBegun, state.firstClashBegun);
            blackboard.Set(Keys::LoyaltyPressure, static_cast<int>(state.loyaltyPressure));
            blackboard.Set(Keys::RetainersFled, state.retainersFled);
            blackboard.Set(Keys::WiglafRemains, state.wiglafRemains);
            blackboard.Set(Keys::CollapseSpoken, state.collapseSpoken);
            blackboard.Set(Keys::WiglafSpoken, state.wiglafSpoken);
        }

        void SyncStateFromBlackboard(BeowulfState& state, const dragongod::Blackboard& blackboard)
        {
            state.scene = static_cast<BeowulfScene>(blackboard.GetOr(Keys::Scene, static_cast<int>(BeowulfScene::BarrowApproach)));
            state.tone = static_cast<BeowulfTone>(blackboard.GetOr(Keys::Tone, static_cast<int>(BeowulfTone::Unset)));
            state.resolve = blackboard.GetOr(Keys::Resolve, 0);
            state.preBattleChoiceConsumed = blackboard.GetOr(Keys::ChoiceConsumed, false);
            state.firstClashBegun = blackboard.GetOr(Keys::FirstClashBegun, false);
            state.loyaltyPressure = static_cast<BeowulfLoyaltyPressure>(blackboard.GetOr(Keys::LoyaltyPressure, static_cast<int>(BeowulfLoyaltyPressure::Unset)));
            state.retainersFled = blackboard.GetOr(Keys::RetainersFled, false);
            state.wiglafRemains = blackboard.GetOr(Keys::WiglafRemains, false);
            state.collapseSpoken = blackboard.GetOr(Keys::CollapseSpoken, false);
            state.wiglafSpoken = blackboard.GetOr(Keys::WiglafSpoken, false);
        }

        [[nodiscard]] dragongod::FrameControl BarrowApproachFrame(StoryFrameCtx& ctx)
        {
            if (ctx.PcAsPhase() == NarrativePhase::EnterBarrow) {
                ctx.state.scene = BeowulfScene::BarrowApproach;
                ctx.output.scene = ctx.state.scene;
                ctx.output.sceneLines = BuildSceneLines(ctx.state.scene, ctx.state.tone);
                ctx.output.choices.clear();
                ctx.output.awaitingChoice = false;
                ctx.output.advanced = true;
                ctx.output.trace.push_back("scene:barrow_approach");
                SyncStateToBlackboard(ctx.state, ctx.Bb());
                return dragongod::Dg::Continue(NarrativePhase::PresentChoice);
            }

            return dragongod::Dg::Fail(1);
        }

        [[nodiscard]] dragongod::FrameControl BeforeBattleChoiceFrame(StoryFrameCtx& ctx)
        {
            if (ctx.PcAsPhase() == NarrativePhase::PresentChoice) {
                ctx.state.scene = BeowulfScene::BeforeBattleChoice;
                ctx.output.scene = ctx.state.scene;
                ctx.output.sceneLines = BuildSceneLines(ctx.state.scene, ctx.state.tone);
                ctx.output.choices = BuildPreBattleChoices();
                ctx.output.awaitingChoice = true;
                ctx.output.advanced = false;
                ctx.output.trace.push_back("scene:before_battle_choice");
                SyncStateToBlackboard(ctx.state, ctx.Bb());
                return dragongod::Dg::Continue(NarrativePhase::ConsumeChoice);
            }

            if (ctx.PcAsPhase() == NarrativePhase::ConsumeChoice) {
                dragongod::Message choiceMessage{};
                if (!ctx.Mb().ConsumeFront(choiceMessage)) {
                    ctx.output.awaitingChoice = true;
                    ctx.output.advanced = false;
                    ctx.output.trace.push_back("awaiting_choice_input");
                    return dragongod::Dg::WaitTicks(1, NarrativePhase::ConsumeChoice);
                }

                const std::optional<BeowulfTone> chosenTone = TryToneFromMessage(choiceMessage);
                if (!chosenTone.has_value()) {
                    ctx.output.trace.push_back("invalid_choice_input");
                    return dragongod::Dg::Fail(2);
                }

                const std::optional<BeowulfLoyaltyPressure> chosenPressure = TryLoyaltyPressureFromTone(*chosenTone);
                if (!chosenPressure.has_value()) {
                    ctx.output.trace.push_back("invalid_choice_tone");
                    return dragongod::Dg::Fail(6);
                }

                ctx.state.tone = *chosenTone;
                ctx.state.loyaltyPressure = *chosenPressure;
                ctx.state.resolve = 1;
                ctx.state.preBattleChoiceConsumed = true;
                ctx.output.awaitingChoice = false;
                ctx.output.advanced = true;
                ctx.output.trace.push_back("choice_consumed");
                SyncStateToBlackboard(ctx.state, ctx.Bb());
                return dragongod::Dg::Continue(NarrativePhase::EnterFirstClash);
            }

            return dragongod::Dg::Fail(3);
        }

        [[nodiscard]] dragongod::FrameControl LateBattleFrame(StoryFrameCtx& ctx)
        {
            if (ctx.PcAsPhase() == NarrativePhase::EnterFirstClash) {
                ctx.state.scene = BeowulfScene::FirstClash;
                ctx.state.firstClashBegun = true;
                ctx.output.scene = ctx.state.scene;
                ctx.output.sceneLines = BuildSceneLines(ctx.state.scene, ctx.state.tone);
                ctx.output.choices.clear();
                ctx.output.awaitingChoice = false;
                ctx.output.advanced = true;
                ctx.output.trace.push_back("scene:first_clash");
                SyncStateToBlackboard(ctx.state, ctx.Bb());
                return dragongod::Dg::Continue(NarrativePhase::EnterRetainerCollapse);
            }

            if (ctx.PcAsPhase() == NarrativePhase::EnterRetainerCollapse) {
                ctx.state.scene = BeowulfScene::RetainerCollapse;
                ctx.state.retainersFled = true;
                ctx.state.collapseSpoken = true;
                ctx.output.scene = ctx.state.scene;
                ctx.output.sceneLines = BuildSceneLines(ctx.state.scene, ctx.state.tone);
                ctx.output.advanced = true;
                ctx.output.trace.push_back("scene:retainer_collapse");
                ctx.output.trace.push_back("state:retainers_fled");
                SyncStateToBlackboard(ctx.state, ctx.Bb());
                return dragongod::Dg::Continue(NarrativePhase::EnterWiglafRemains);
            }

            if (ctx.PcAsPhase() == NarrativePhase::EnterWiglafRemains) {
                ctx.state.scene = BeowulfScene::WiglafRemains;
                ctx.state.wiglafRemains = true;
                ctx.state.wiglafSpoken = true;
                ctx.output.scene = ctx.state.scene;
                ctx.output.sceneLines = BuildSceneLines(ctx.state.scene, ctx.state.tone);
                ctx.output.advanced = true;
                ctx.output.trace.push_back("scene:wiglaf_remains");
                ctx.output.trace.push_back("state:wiglaf_remains");
                SyncStateToBlackboard(ctx.state, ctx.Bb());
                return dragongod::Dg::Continue(NarrativePhase::EnterTragicHandoff);
            }

            if (ctx.PcAsPhase() == NarrativePhase::EnterTragicHandoff) {
                ctx.state.scene = BeowulfScene::TragicHandoff;
                ctx.output.scene = ctx.state.scene;
                ctx.output.sceneLines = BuildSceneLines(ctx.state.scene, ctx.state.tone);
                ctx.output.advanced = true;
                ctx.output.trace.push_back("scene:tragic_handoff");
                SyncStateToBlackboard(ctx.state, ctx.Bb());
                return dragongod::Dg::Continue(NarrativePhase::Completed);
            }

            if (ctx.PcAsPhase() == NarrativePhase::Completed) {
                ctx.state.scene = BeowulfScene::Completed;
                ctx.output.scene = ctx.state.scene;
                ctx.output.trace.push_back("scene:completed");
                SyncStateToBlackboard(ctx.state, ctx.Bb());
                return dragongod::Dg::Complete();
            }

            return dragongod::Dg::Fail(4);
        }
    }

    [[nodiscard]] BeowulfRunResult RunBeowulfGoldenPath(const BeowulfRunRequest& request)
    {
        BeowulfRunResult result{};
        result.finalState = BuildInitialBeowulfState();

        dragongod::Blackboard blackboard{};
        dragongod::Mailbox mailbox{};

        for (const dragongod::Message& message : request.mailboxInput) {
            mailbox.Enqueue(message);
        }
        mailbox.BeginTick();

        SyncStateToBlackboard(result.finalState, blackboard);

        std::uint32_t pc = static_cast<std::uint32_t>(NarrativePhase::EnterBarrow);
        bool entered = true;

        for (int step = 0; step < request.maxSteps; ++step) {
            dragongod::FrameCtx frameCtx(
                dragongod::FrameId::RootTypedPhaseMailboxAct,
                static_cast<dragongod::TickIndex>(step),
                pc,
                entered,
                blackboard,
                mailbox);

            StoryFrameCtx storyCtx{
                .frame = frameCtx,
                .state = result.finalState,
                .output = result.output
            };

            dragongod::FrameControl control = dragongod::Dg::Fail(99);
            if (storyCtx.PcAsPhase() == NarrativePhase::EnterBarrow) {
                control = BarrowApproachFrame(storyCtx);
            } else if (storyCtx.PcAsPhase() == NarrativePhase::PresentChoice ||
                storyCtx.PcAsPhase() == NarrativePhase::ConsumeChoice) {
                control = BeforeBattleChoiceFrame(storyCtx);
            } else {
                control = LateBattleFrame(storyCtx);
            }

            SyncStateFromBlackboard(result.finalState, blackboard);

            if (!IsBeowulfStateValid(result.finalState)) {
                result.failed = true;
                result.failureReason = "beowulf state invariant failed";
                return result;
            }

            if (control.kind == dragongod::FrameControlKind::Complete) {
                result.failed = false;
                return result;
            }

            if (control.kind == dragongod::FrameControlKind::Fail) {
                result.failed = true;
                result.failureReason = "beowulf narrative frame failed";
                return result;
            }

            if (control.kind == dragongod::FrameControlKind::Wait) {
                result.failed = false;
                return result;
            }

            if (control.kind != dragongod::FrameControlKind::Continue) {
                result.failed = true;
                result.failureReason = "unsupported control kind in sample loop";
                return result;
            }

            pc = control.resumePc;
            entered = false;
        }

        result.failed = true;
        result.failureReason = "step budget exhausted";
        return result;
    }
}
