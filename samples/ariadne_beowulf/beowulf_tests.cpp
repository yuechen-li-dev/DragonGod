#include "../../tests/Marionette/test_harness.h"

#include "beowulf_model.h"
#include "beowulf_nodes.h"

namespace
{
    using namespace dragongod_samples::ariadne_beowulf;
}

FACT(PreAriadneDG_ModelScaffold_InitialStateIsValid)
{
    const BeowulfModelState state = BuildInitialModelState();

    ASSERT_EQUAL(0, state.sceneIndex, "scaffold should start with scene index at zero");
    ASSERT_EQUAL(0, state.memoryTokenCount, "scaffold should start with empty memory token set");
    ASSERT_FALSE(state.rollbackCheckpointReady, "scaffold should not assume rollback checkpoints yet");
    ASSERT_TRUE(IsScaffoldStateShapeValid(state), "initial scaffold state should satisfy bounded model shape");
}

FACT(PreAriadneDG_SmokePath_LinksAgainstDragonGodRuntime)
{
    const BeowulfScaffoldSmoke smoke = RunBeowulfScaffoldSmokePath();

    ASSERT_EQUAL(
        static_cast<int>(dragongod::StackRunOutcome::Completed),
        static_cast<int>(smoke.outcome),
        "scaffold runtime smoke path should complete a built-in DragonGod scenario");
    ASSERT_TRUE(smoke.ticksRan > 0, "smoke path should execute at least one tick");
}
