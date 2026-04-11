#pragma once

#include "../../src/DragonGod/runtime_compat.h"

namespace dragongod_samples::dragon_router
{
    struct RouterSampleConfig
    {
        dragongod::TickIndex tickBudget = 4;
    };

    struct RouterSampleSmokeResult
    {
        dragongod::StackRunOutcome outcome = dragongod::StackRunOutcome::Continue;
        dragongod::TickIndex executedTicks = 0;
    };

    [[nodiscard]] RouterSampleSmokeResult RunRouterSampleSmoke(const RouterSampleConfig& config);
    [[nodiscard]] bool RouterSampleSmokeSucceeded(const RouterSampleSmokeResult& result);
}
