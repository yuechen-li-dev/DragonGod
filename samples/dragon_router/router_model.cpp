#include "router_model.h"

namespace dragongod_samples::dragon_router
{
    [[nodiscard]] RouterSampleSmokeResult RunRouterSampleSmoke(const RouterSampleConfig& config)
    {
        const dragongod::StackFrameRuntime runtime;
        const dragongod::FrameRunResult run = runtime.RunForTicks(
            dragongod::StackScriptScenario::ContinueThenComplete,
            config.tickBudget);

        RouterSampleSmokeResult result;
        result.outcome = run.finalOutcome;
        result.executedTicks = run.tickTrace.size();
        return result;
    }

    [[nodiscard]] bool RouterSampleSmokeSucceeded(const RouterSampleSmokeResult& result)
    {
        return result.outcome == dragongod::StackRunOutcome::Completed && result.executedTicks > 0;
    }
}
