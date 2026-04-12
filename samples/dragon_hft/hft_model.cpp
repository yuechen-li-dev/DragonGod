#include "hft_model.h"

namespace dragongod_samples::dragon_hft
{
    [[nodiscard]] MarketReactionState BuildInitialHftState()
    {
        return MarketReactionState{};
    }

    [[nodiscard]] HftScaffoldSmokeOutput RunHftScaffoldSmoke()
    {
        MarketReactionState state = BuildInitialHftState();
        AdvanceHftScaffoldNodes(state);

        const dragongod::StackFrameRuntime runtime;
        const dragongod::FrameRunResult run = runtime.RunForTicks(
            dragongod::StackScriptScenario::PushPopComplete,
            1);

        return HftScaffoldSmokeOutput{
            .state = state,
            .runtimeOutcome = run.finalOutcome,
            .runtimeTraceCount = run.trace.size()
        };
    }
}
