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

FACT(M15a_Runtime_GoldenPath_ReachesFirstClashWithKinglyChoice)
{
    const BeowulfRunResult run = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakAsKing));

    ASSERT_FALSE(run.failed, "golden path should succeed with bounded valid choice");
    ASSERT_TRUE(run.finalState.firstClashBegun, "golden path should reach first clash");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Kingly), static_cast<int>(run.finalState.tone), "kingly choice should set kingly tone");
    ASSERT_EQUAL(std::string("He speaks as ring-giver to his men."), run.output.sceneLines[0], "kingly path should emit kingly clash opening line");
}

FACT(M15a_Runtime_Replay_SameInputProducesSameDeterministicOutput)
{
    const BeowulfRunRequest request = RequestForTone(BeowulfChoiceId::SpeakGrimly);

    const BeowulfRunResult runA = RunBeowulfGoldenPath(request);
    const BeowulfRunResult runB = RunBeowulfGoldenPath(request);

    ASSERT_TRUE(runA == runB, "same mailbox input should replay to identical bounded output surface");
}

FACT(M15a_Runtime_OutputSurface_IsAssertableAndStable)
{
    const BeowulfRunResult run = RunBeowulfGoldenPath(RequestForTone(BeowulfChoiceId::SpeakProudly));

    ASSERT_EQUAL(static_cast<int>(BeowulfScene::Completed), static_cast<int>(run.output.scene), "terminal scene marker should be explicit on completion");
    ASSERT_EQUAL(3, static_cast<int>(run.output.sceneLines.size()), "first clash output should remain compact and assertable");
    ASSERT_TRUE(!run.output.trace.empty(), "trace should contain deterministic transition evidence");
}
