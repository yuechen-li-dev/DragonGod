#include "../../tests/Marionette/test_harness.h"

#include "beowulf_model.h"

namespace
{
    using namespace dragongod_samples::ariadne_beowulf;
}

FACT(M15b_Model_InitialState_IsDeterministicAndValid)
{
    const BeowulfState state = BuildInitialBeowulfState();

    ASSERT_EQUAL(static_cast<int>(BeowulfScene::BarrowApproach), static_cast<int>(state.scene), "initial scene should begin at barrow approach");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Unset), static_cast<int>(state.tone), "initial tone should be unset before choice");
    ASSERT_EQUAL(static_cast<int>(BeowulfLoyaltyPressure::Unset), static_cast<int>(state.loyaltyPressure), "initial loyalty pressure should be unset");
    ASSERT_FALSE(state.retainersFled, "initial state should not have retainer collapse");
    ASSERT_FALSE(state.wiglafRemains, "initial state should not yet keep Wiglaf in frame");
    ASSERT_TRUE(IsBeowulfStateValid(state), "initial state should satisfy bounded invariants");
}

FACT(M15b_Model_ChoiceMapping_MapsToExpectedToneAndPressure)
{
    const std::optional<BeowulfTone> proudTone = TryToneFromChoiceId(BeowulfChoiceId::SpeakProudly);
    const std::optional<BeowulfTone> grimTone = TryToneFromChoiceId(BeowulfChoiceId::SpeakGrimly);
    const std::optional<BeowulfTone> kinglyTone = TryToneFromChoiceId(BeowulfChoiceId::SpeakAsKing);

    ASSERT_TRUE(proudTone.has_value(), "proud choice should map to a tone");
    ASSERT_TRUE(grimTone.has_value(), "grim choice should map to a tone");
    ASSERT_TRUE(kinglyTone.has_value(), "kingly choice should map to a tone");

    const std::optional<BeowulfLoyaltyPressure> proudPressure = TryLoyaltyPressureFromTone(*proudTone);
    const std::optional<BeowulfLoyaltyPressure> grimPressure = TryLoyaltyPressureFromTone(*grimTone);
    const std::optional<BeowulfLoyaltyPressure> kinglyPressure = TryLoyaltyPressureFromTone(*kinglyTone);

    ASSERT_EQUAL(static_cast<int>(BeowulfLoyaltyPressure::BoastShattered), static_cast<int>(*proudPressure), "proud tone should map to boast-shattered pressure");
    ASSERT_EQUAL(static_cast<int>(BeowulfLoyaltyPressure::DoomWeight), static_cast<int>(*grimPressure), "grim tone should map to doom-weight pressure");
    ASSERT_EQUAL(static_cast<int>(BeowulfLoyaltyPressure::OathBurden), static_cast<int>(*kinglyPressure), "kingly tone should map to oath-burden pressure");
}

FACT(M15b_Model_CollapseAndLoyaltySceneLines_AreBoundedAndToneSensitive)
{
    const std::vector<std::string> collapseProud = BuildSceneLines(BeowulfScene::RetainerCollapse, BeowulfTone::Proud);
    const std::vector<std::string> collapseGrim = BuildSceneLines(BeowulfScene::RetainerCollapse, BeowulfTone::Grim);
    const std::vector<std::string> wiglaf = BuildSceneLines(BeowulfScene::WiglafRemains, BeowulfTone::Kingly);
    const std::vector<std::string> handoff = BuildSceneLines(BeowulfScene::TragicHandoff, BeowulfTone::Kingly);

    ASSERT_EQUAL(3, static_cast<int>(collapseProud.size()), "collapse beat should remain compact");
    ASSERT_EQUAL(3, static_cast<int>(collapseGrim.size()), "collapse beat variants should stay compact");
    ASSERT_TRUE(collapseProud != collapseGrim, "earlier tone should alter collapse phrasing");
    ASSERT_EQUAL(std::string("Wiglaf steps through ash to his lord."), wiglaf[0], "wiglaf loyalty beat should stay explicit");
    ASSERT_EQUAL(3, static_cast<int>(handoff.size()), "tragic handoff beat should remain compact and assertable");
}

FACT(M15b_Model_StateValidation_RejectsOutOfOrderLoyaltyFlags)
{
    BeowulfState invalid = BuildInitialBeowulfState();
    invalid.preBattleChoiceConsumed = true;
    invalid.tone = BeowulfTone::Proud;
    invalid.loyaltyPressure = BeowulfLoyaltyPressure::BoastShattered;
    invalid.wiglafRemains = true;

    ASSERT_FALSE(IsBeowulfStateValid(invalid), "Wiglaf cannot remain before retainers collapse in bounded ordering");
}
