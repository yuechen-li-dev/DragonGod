#include "../../tests/Marionette/test_harness.h"

#include "beowulf_model.h"

namespace
{
    using namespace dragongod_samples::ariadne_beowulf;
}

FACT(M15c_Model_InitialState_IsDeterministicAndValid)
{
    const BeowulfState state = BuildInitialBeowulfState();

    ASSERT_EQUAL(static_cast<int>(BeowulfScene::BarrowApproach), static_cast<int>(state.scene), "initial scene should begin at barrow approach");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Unset), static_cast<int>(state.tone), "initial tone should be unset before choice");
    ASSERT_EQUAL(static_cast<int>(BeowulfLoyaltyPressure::Unset), static_cast<int>(state.loyaltyPressure), "initial loyalty pressure should be unset");
    ASSERT_FALSE(state.retainersFled, "initial state should not have retainer collapse");
    ASSERT_FALSE(state.wiglafRemains, "initial state should not yet keep Wiglaf in frame");
    ASSERT_FALSE(state.beowulfFallen, "initial state should not begin in death beat");
    ASSERT_FALSE(state.lastWordsSpoken, "initial state should not have spoken last words");
    ASSERT_FALSE(state.endingCompleted, "initial state should not begin completed");
    ASSERT_TRUE(IsBeowulfStateValid(state), "initial state should satisfy bounded invariants");
}

FACT(M15c_Model_ChoiceMapping_MapsToExpectedToneAndPressure)
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

FACT(M15c_Model_EndingHelpers_MapToLegacyToneAndCompactSceneLines)
{
    const std::optional<BeowulfLegacyTone> proudLegacy = TryLegacyToneFromState(BeowulfTone::Proud, BeowulfLoyaltyPressure::BoastShattered);
    const std::optional<BeowulfLegacyTone> grimLegacy = TryLegacyToneFromState(BeowulfTone::Grim, BeowulfLoyaltyPressure::DoomWeight);
    const std::optional<BeowulfLegacyTone> kinglyLegacy = TryLegacyToneFromState(BeowulfTone::Kingly, BeowulfLoyaltyPressure::OathBurden);

    ASSERT_TRUE(proudLegacy.has_value(), "proud path should map to a bounded legacy tone");
    ASSERT_TRUE(grimLegacy.has_value(), "grim path should map to a bounded legacy tone");
    ASSERT_TRUE(kinglyLegacy.has_value(), "kingly path should map to a bounded legacy tone");

    const std::vector<std::string> lastWords = BuildSceneLines(BeowulfScene::LastWords, BeowulfTone::Kingly, *kinglyLegacy);
    const std::vector<std::string> proudEnding = BuildSceneLines(BeowulfScene::LegacyEnding, BeowulfTone::Proud, *proudLegacy);
    const std::vector<std::string> grimEnding = BuildSceneLines(BeowulfScene::LegacyEnding, BeowulfTone::Grim, *grimLegacy);

    ASSERT_EQUAL(3, static_cast<int>(lastWords.size()), "last-words beat should remain compact");
    ASSERT_EQUAL(std::string("My days are spent; keep faith after me."), lastWords[0], "last words should stay explicit and bounded");
    ASSERT_EQUAL(3, static_cast<int>(proudEnding.size()), "legacy ending beat should remain compact");
    ASSERT_TRUE(proudEnding != grimEnding, "legacy ending image should vary with bounded earlier pressure");
}

FACT(M15c_Model_StateValidation_RejectsOutOfOrderLoyaltyFlags)
{
    BeowulfState invalid = BuildInitialBeowulfState();
    invalid.preBattleChoiceConsumed = true;
    invalid.tone = BeowulfTone::Proud;
    invalid.loyaltyPressure = BeowulfLoyaltyPressure::BoastShattered;
    invalid.wiglafRemains = true;

    ASSERT_FALSE(IsBeowulfStateValid(invalid), "Wiglaf cannot remain before retainers collapse in bounded ordering");
}

FACT(M15c_Model_StateValidation_RejectsPrematureEndingCompletion)
{
    BeowulfState invalid = BuildInitialBeowulfState();
    invalid.preBattleChoiceConsumed = true;
    invalid.tone = BeowulfTone::Kingly;
    invalid.loyaltyPressure = BeowulfLoyaltyPressure::OathBurden;
    invalid.firstClashBegun = true;
    invalid.retainersFled = true;
    invalid.wiglafRemains = true;
    invalid.beowulfFallen = true;
    invalid.lastWordsSpoken = true;
    invalid.endingCompleted = true;
    invalid.legacyTone = BeowulfLegacyTone::Unset;

    ASSERT_FALSE(IsBeowulfStateValid(invalid), "completed ending must include an explicit legacy tone tag");
}
