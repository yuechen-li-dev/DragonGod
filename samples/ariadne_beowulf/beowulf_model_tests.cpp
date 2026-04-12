#include "../../tests/Marionette/test_harness.h"

#include "beowulf_model.h"

namespace
{
    using namespace dragongod_samples::ariadne_beowulf;
}

FACT(M15a_Model_InitialState_IsDeterministicAndValid)
{
    const BeowulfState state = BuildInitialBeowulfState();

    ASSERT_EQUAL(static_cast<int>(BeowulfScene::BarrowApproach), static_cast<int>(state.scene), "initial scene should begin at barrow approach");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Unset), static_cast<int>(state.tone), "initial tone should be unset before choice");
    ASSERT_EQUAL(0, state.resolve, "initial resolve should be zero");
    ASSERT_FALSE(state.preBattleChoiceConsumed, "initial state should not consume a choice");
    ASSERT_FALSE(state.firstClashBegun, "initial state should not begin first clash");
    ASSERT_TRUE(IsBeowulfStateValid(state), "initial state should satisfy bounded invariants");
}

FACT(M15a_Model_PreBattleChoices_AreBoundedAndStable)
{
    const std::vector<BeowulfChoice> choices = BuildPreBattleChoices();

    ASSERT_EQUAL(3, static_cast<int>(choices.size()), "pre-battle should expose exactly three bounded choices");
    ASSERT_EQUAL(static_cast<int>(BeowulfChoiceId::SpeakProudly), static_cast<int>(choices[0].id), "first choice id should be stable");
    ASSERT_EQUAL(static_cast<int>(BeowulfChoiceId::SpeakGrimly), static_cast<int>(choices[1].id), "second choice id should be stable");
    ASSERT_EQUAL(static_cast<int>(BeowulfChoiceId::SpeakAsKing), static_cast<int>(choices[2].id), "third choice id should be stable");
}

FACT(M15a_Model_ChoiceMapping_MapsToExpectedTones)
{
    const std::optional<BeowulfTone> proud = TryToneFromChoiceId(BeowulfChoiceId::SpeakProudly);
    const std::optional<BeowulfTone> grim = TryToneFromChoiceId(BeowulfChoiceId::SpeakGrimly);
    const std::optional<BeowulfTone> kingly = TryToneFromChoiceId(BeowulfChoiceId::SpeakAsKing);

    ASSERT_TRUE(proud.has_value(), "proud choice should map to a tone");
    ASSERT_TRUE(grim.has_value(), "grim choice should map to a tone");
    ASSERT_TRUE(kingly.has_value(), "kingly choice should map to a tone");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Proud), static_cast<int>(*proud), "proud mapping should be deterministic");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Grim), static_cast<int>(*grim), "grim mapping should be deterministic");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Kingly), static_cast<int>(*kingly), "kingly mapping should be deterministic");
}

FACT(M15a_Model_SceneLines_AreBoundedPoeticAndDeterministic)
{
    const std::vector<std::string> barrow = BuildSceneLines(BeowulfScene::BarrowApproach, BeowulfTone::Unset);
    const std::vector<std::string> clashProud = BuildSceneLines(BeowulfScene::FirstClash, BeowulfTone::Proud);
    const std::vector<std::string> clashGrim = BuildSceneLines(BeowulfScene::FirstClash, BeowulfTone::Grim);

    ASSERT_EQUAL(3, static_cast<int>(barrow.size()), "barrow scene should emit compact three-line beat");
    ASSERT_EQUAL(std::string("Old king under iron sky."), barrow[0], "barrow first line should remain stable");
    ASSERT_EQUAL(3, static_cast<int>(clashProud.size()), "first clash should emit compact three-line beat");
    ASSERT_EQUAL(3, static_cast<int>(clashGrim.size()), "first clash variants should be equally bounded");
    ASSERT_TRUE(clashProud != clashGrim, "tone choice should materially alter first clash text");
}
