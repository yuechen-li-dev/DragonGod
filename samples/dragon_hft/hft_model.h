#pragma once

#include "../../src/DragonGod/runtime.h"

#include <cstddef>

namespace dragongod_samples::dragon_hft
{
    struct MarketReactionState
    {
        bool hasStaleSignal = false;
        int outstandingOrderCount = 0;
        int reactionFrames = 0;

        [[nodiscard]] bool operator==(const MarketReactionState& other) const = default;
    };

    struct HftScaffoldSmokeOutput
    {
        MarketReactionState state{};
        dragongod::StackRunOutcome runtimeOutcome = dragongod::StackRunOutcome::Continue;
        std::size_t runtimeTraceCount = 0;

        [[nodiscard]] bool operator==(const HftScaffoldSmokeOutput& other) const = default;
    };

    [[nodiscard]] MarketReactionState BuildInitialHftState();
    void AdvanceHftScaffoldNodes(MarketReactionState& state);
    [[nodiscard]] HftScaffoldSmokeOutput RunHftScaffoldSmoke();
}
