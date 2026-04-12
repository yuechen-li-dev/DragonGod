#include "../../tests/Marionette/test_harness.h"

#include "beowulf_nodes.h"

namespace
{
    using namespace dragongod_samples::ariadne_beowulf;

    [[nodiscard]] dragongod::Message ChoiceMessage(const BeowulfChoiceId id)
    {
        return dragongod::Message{
            .kind = dragongod::MessageKind::Signal,
            .value = static_cast<int>(id)
        };
    }
}

FACT(M15c_Nodes_BarrowThenChoice_NoInputWaitsDeterministically)
{
    const BeowulfRunResult run = RunBeowulfGoldenPath(BeowulfRunRequest{});

    ASSERT_FALSE(run.failed, "missing choice input should wait, not fail");
    ASSERT_EQUAL(static_cast<int>(BeowulfScene::BeforeBattleChoice), static_cast<int>(run.output.scene), "run should stop at the bounded pre-battle choice scene");
    ASSERT_TRUE(run.output.awaitingChoice, "run should expose awaiting-choice status");
    ASSERT_EQUAL(3, static_cast<int>(run.output.choices.size()), "run should present deterministic bounded choice list");
}

FACT(M15c_Nodes_ChoiceProudly_TransitionsToDeathLastWordsAndCompletion)
{
    BeowulfRunRequest request{};
    request.mailboxInput.push_back(ChoiceMessage(BeowulfChoiceId::SpeakProudly));

    const BeowulfRunResult run = RunBeowulfGoldenPath(request);

    ASSERT_FALSE(run.failed, "valid choice should not fail");
    ASSERT_TRUE(run.finalState.preBattleChoiceConsumed, "valid choice should be consumed");
    ASSERT_TRUE(run.finalState.firstClashBegun, "valid choice should begin first clash");
    ASSERT_TRUE(run.finalState.retainersFled, "retainers should collapse in this milestone path");
    ASSERT_TRUE(run.finalState.wiglafRemains, "Wiglaf should remain as bounded loyalty anchor");
    ASSERT_TRUE(run.finalState.beowulfFallen, "ending path should include explicit fatal-wound beat");
    ASSERT_TRUE(run.finalState.lastWordsSpoken, "ending path should include compact last words");
    ASSERT_TRUE(run.finalState.endingCompleted, "ending path should terminate in deterministic completed state");
    ASSERT_EQUAL(static_cast<int>(BeowulfLoyaltyPressure::BoastShattered), static_cast<int>(run.finalState.loyaltyPressure), "proud tone should carry bounded pressure flavor");
    ASSERT_EQUAL(static_cast<int>(BeowulfScene::Completed), static_cast<int>(run.output.scene), "run should complete deterministic bounded sequence");
    ASSERT_EQUAL(std::string("Gold glows cold in a leaderless dark."), run.output.sceneLines[0], "final emitted ending image should match proud legacy tone");
}

FACT(M15c_Nodes_ChoiceGrimly_ChangesLegacyEndingFlavor)
{
    BeowulfRunRequest request{};
    request.mailboxInput.push_back(ChoiceMessage(BeowulfChoiceId::SpeakGrimly));

    const BeowulfRunResult run = RunBeowulfGoldenPath(request);

    ASSERT_FALSE(run.failed, "valid choice should not fail");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Grim), static_cast<int>(run.finalState.tone), "grim choice should set grim tone");
    ASSERT_EQUAL(static_cast<int>(BeowulfLoyaltyPressure::DoomWeight), static_cast<int>(run.finalState.loyaltyPressure), "grim tone should map to doom-weight pressure");
    ASSERT_EQUAL(static_cast<int>(BeowulfLegacyTone::DoomEmber), static_cast<int>(run.finalState.legacyTone), "grim loyalty pressure should map to doom-ember legacy tone");
    ASSERT_TRUE(run.finalState.collapseSpoken, "collapse beat flag should be marked");
    ASSERT_TRUE(run.finalState.wiglafSpoken, "wiglaf beat flag should be marked");
    ASSERT_EQUAL(std::string("The hoard burns low; doom keeps its ember."), run.output.sceneLines[0], "grim ending output should carry deterministic doom-ember image");
}

FACT(M15c_Nodes_InvalidChoice_FailsWithBoundedReason)
{
    BeowulfRunRequest request{};
    request.mailboxInput.push_back(dragongod::Message{
        .kind = dragongod::MessageKind::Signal,
        .value = 77
    });

    const BeowulfRunResult run = RunBeowulfGoldenPath(request);

    ASSERT_TRUE(run.failed, "invalid choice should fail fast and explicitly");
    ASSERT_EQUAL(std::string("beowulf narrative frame failed"), run.failureReason, "failure reason should remain deterministic and bounded");
}
