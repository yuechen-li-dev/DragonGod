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

FACT(M15a_Nodes_BarrowThenChoice_NoInputWaitsDeterministically)
{
    const BeowulfRunResult run = RunBeowulfGoldenPath(BeowulfRunRequest{});

    ASSERT_FALSE(run.failed, "missing choice input should wait, not fail");
    ASSERT_EQUAL(static_cast<int>(BeowulfScene::BeforeBattleChoice), static_cast<int>(run.output.scene), "run should stop at the bounded pre-battle choice scene");
    ASSERT_TRUE(run.output.awaitingChoice, "run should expose awaiting-choice status");
    ASSERT_EQUAL(3, static_cast<int>(run.output.choices.size()), "run should present deterministic bounded choice list");
}

FACT(M15a_Nodes_ChoiceProudly_TransitionsToFirstClash)
{
    BeowulfRunRequest request{};
    request.mailboxInput.push_back(ChoiceMessage(BeowulfChoiceId::SpeakProudly));

    const BeowulfRunResult run = RunBeowulfGoldenPath(request);

    ASSERT_FALSE(run.failed, "valid choice should not fail");
    ASSERT_TRUE(run.finalState.preBattleChoiceConsumed, "valid choice should be consumed");
    ASSERT_TRUE(run.finalState.firstClashBegun, "valid choice should begin first clash");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Proud), static_cast<int>(run.finalState.tone), "proud choice should set proud tone");
    ASSERT_EQUAL(std::string("He boasts against the barrow-mouth."), run.output.sceneLines[0], "proud choice should produce proud clash text");
}

FACT(M15a_Nodes_ChoiceGrimly_TransitionsToFirstClash)
{
    BeowulfRunRequest request{};
    request.mailboxInput.push_back(ChoiceMessage(BeowulfChoiceId::SpeakGrimly));

    const BeowulfRunResult run = RunBeowulfGoldenPath(request);

    ASSERT_FALSE(run.failed, "valid choice should not fail");
    ASSERT_EQUAL(static_cast<int>(BeowulfTone::Grim), static_cast<int>(run.finalState.tone), "grim choice should set grim tone");
    ASSERT_EQUAL(std::string("He names doom and does not turn."), run.output.sceneLines[0], "grim choice should produce grim clash text");
}

FACT(M15a_Nodes_InvalidChoice_FailsWithBoundedReason)
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
