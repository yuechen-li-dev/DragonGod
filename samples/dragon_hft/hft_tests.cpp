#include "../../tests/Marionette/test_harness.h"
#include "hft_model.h"

namespace
{
    using namespace dragongod_samples::dragon_hft;
}

FACT(PreHft_Scaffold_Smoke_WiresSampleToRuntime)
{
    const HftScaffoldSmokeOutput output = RunHftScaffoldSmoke();

    ASSERT_EQUAL(1, output.state.reactionFrames, "scaffold node lane should run exactly one placeholder frame");
    ASSERT_EQUAL(
        static_cast<int>(dragongod::StackRunOutcome::Continue),
        static_cast<int>(output.runtimeOutcome),
        "scaffold should execute a real DragonGod runtime path in this smoke pass");
    ASSERT_TRUE(output.runtimeTraceCount > 0, "runtime smoke run should emit trace entries");
}
