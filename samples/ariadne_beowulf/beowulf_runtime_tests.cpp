#include "../../tests/Marionette/test_harness.h"

#include "beowulf_nodes.h"

namespace
{
    using namespace dragongod_samples::ariadne_beowulf;

    [[nodiscard]] BeowulfRunRequest RequestForTone(const BeowulfChoiceId id)
    {
        BeowulfRunRequest request{};
        request.mailboxInput.push_back(dragongod::Message{
            .kind = dragongod::MessageKind::Signal,
            .value = static_cast<int>(id)
        });
        return request;
    }
}

FACT(M15c_Runtime_GoldenPath_ReachesDeathAndLastWordsWithKinglyChoice)
{
    const BeowulfRunResult run = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakAsKing));

    ASSERT_FALSE(run.failed, "golden path should succeed with bounded valid choice");
    ASSERT_TRUE(run.finalState.firstClashBegun, "golden path should reach first clash");
    ASSERT_TRUE(run.finalState.retainersFled, "golden path should include retainer collapse beat");
    ASSERT_TRUE(run.finalState.wiglafRemains, "golden path should preserve Wiglaf as loyalty anchor");
    ASSERT_TRUE(run.finalState.beowulfFallen, "golden path should reach explicit fatal state");
    ASSERT_TRUE(run.finalState.lastWordsSpoken, "golden path should emit compact final speech beat");
    ASSERT_TRUE(run.finalState.endingCompleted, "golden path should mark bounded ending completion");
    ASSERT_EQUAL(static_cast<int>(BeowulfLegacyTone::OathKept), static_cast<int>(run.finalState.legacyTone), "kingly path should conclude with oath-kept legacy tone");
    ASSERT_EQUAL(static_cast<int>(BeowulfLoyaltyPressure::OathBurden), static_cast<int>(run.finalState.loyaltyPressure), "kingly choice should carry oath-burden pressure");
}

FACT(M15c_Runtime_Replay_SameInputProducesSameDeterministicOutput)
{
    const BeowulfRunRequest request = RequestForTone(BeowulfChoiceId::SpeakGrimly);

    const BeowulfRunResult runA = RunBeowulfGoldenPath(request);
    const BeowulfRunResult runB = RunBeowulfGoldenPath(request);

    ASSERT_TRUE(runA == runB, "same mailbox input should replay to identical bounded output surface");
}

FACT(M15c_Runtime_ToneChangesEndingLegacyOutput)
{
    const BeowulfRunResult proud = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakProudly));
    const BeowulfRunResult grim = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakGrimly));

    ASSERT_FALSE(proud.failed, "proud run should succeed");
    ASSERT_FALSE(grim.failed, "grim run should succeed");
    ASSERT_TRUE(proud.finalState.loyaltyPressure != grim.finalState.loyaltyPressure, "tone should alter bounded dramatic pressure state");
    ASSERT_TRUE(proud.finalState.legacyTone != grim.finalState.legacyTone, "bounded ending tone should differ across earlier choice pressure");
    ASSERT_TRUE(proud.output.sceneLines != grim.output.sceneLines, "ending output image should vary deterministically with legacy tone");
    ASSERT_TRUE(!proud.output.trace.empty(), "trace should remain assertable for proud run");
    ASSERT_TRUE(!grim.output.trace.empty(), "trace should remain assertable for grim run");
}

FACT(M15c_Runtime_OutputSurface_IsAssertableAndStable)
{
    const BeowulfRunResult run = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakProudly));

    ASSERT_EQUAL(static_cast<int>(BeowulfScene::Completed), static_cast<int>(run.output.scene), "terminal scene marker should remain explicit");
    ASSERT_EQUAL(3, static_cast<int>(run.output.sceneLines.size()), "ending output should remain compact and assertable");
    ASSERT_EQUAL(std::string("Wiglaf stands alone by the barrow-fire."), run.output.sceneLines[2], "bounded legacy ending line should remain stable");
}
