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

FACT(M15b_Runtime_GoldenPath_ReachesCollapseAndWiglafWithKinglyChoice)
{
    const BeowulfRunResult run = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakAsKing));

    ASSERT_FALSE(run.failed, "golden path should succeed with bounded valid choice");
    ASSERT_TRUE(run.finalState.firstClashBegun, "golden path should reach first clash");
    ASSERT_TRUE(run.finalState.retainersFled, "golden path should include retainer collapse beat");
    ASSERT_TRUE(run.finalState.wiglafRemains, "golden path should preserve Wiglaf as loyalty anchor");
    ASSERT_EQUAL(static_cast<int>(BeowulfLoyaltyPressure::OathBurden), static_cast<int>(run.finalState.loyaltyPressure), "kingly choice should carry oath-burden pressure");
}

FACT(M15b_Runtime_Replay_SameInputProducesSameDeterministicOutput)
{
    const BeowulfRunRequest request = RequestForTone(BeowulfChoiceId::SpeakGrimly);

    const BeowulfRunResult runA = RunBeowulfGoldenPath(request);
    const BeowulfRunResult runB = RunBeowulfGoldenPath(request);

    ASSERT_TRUE(runA == runB, "same mailbox input should replay to identical bounded output surface");
}

FACT(M15b_Runtime_ToneChangesCollapseAndLoyaltyTrace)
{
    const BeowulfRunResult proud = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakProudly));
    const BeowulfRunResult grim = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakGrimly));

    ASSERT_FALSE(proud.failed, "proud run should succeed");
    ASSERT_FALSE(grim.failed, "grim run should succeed");
    ASSERT_TRUE(proud.finalState.loyaltyPressure != grim.finalState.loyaltyPressure, "tone should alter bounded dramatic pressure state");
    ASSERT_TRUE(!proud.output.trace.empty(), "trace should remain assertable for proud run");
    ASSERT_TRUE(!grim.output.trace.empty(), "trace should remain assertable for grim run");
}

FACT(M15b_Runtime_OutputSurface_IsAssertableAndStable)
{
    const BeowulfRunResult run = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakProudly));

    ASSERT_EQUAL(static_cast<int>(BeowulfScene::Completed), static_cast<int>(run.output.scene), "terminal scene marker should remain explicit");
    ASSERT_EQUAL(3, static_cast<int>(run.output.sceneLines.size()), "handoff output should remain compact and assertable");
    ASSERT_EQUAL(std::string("Night leans toward its grievous turn."), run.output.sceneLines[2], "late-phase handoff line should remain stable");
}
