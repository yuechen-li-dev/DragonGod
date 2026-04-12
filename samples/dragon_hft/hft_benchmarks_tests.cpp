#include "../../tests/Marionette/test_harness.h"
#include "hft_model.h"

#include <cstdint>
#include <vector>

namespace
{
    using namespace dragongod_samples::dragon_hft;

    volatile std::int64_t g_hftBenchSink = 0;

    [[nodiscard]] HftState BuildBaselineState()
    {
        HftState state{};
        state.threshold = 5;
        state.staleTickThreshold = 2;
        return state;
    }

    [[nodiscard]] HftState BuildHysteresisState()
    {
        HftState state = BuildBaselineState();
        state.enableReentryHysteresis = true;
        state.reentryHysteresisMargin = 2;
        return state;
    }

    [[nodiscard]] HftState BuildMinCommitState()
    {
        HftState state = BuildBaselineState();
        state.enableMinCommit = true;
        state.minCommitTicks = 2;
        state.staleTickThreshold = 0;
        return state;
    }

    [[nodiscard]] std::vector<MarketEvent> BuildBaselineMailbox()
    {
        return {
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 6 }
        };
    }

    [[nodiscard]] std::vector<MarketEvent> BuildHysteresisMailbox()
    {
        return {
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 6 },
            MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 7 }
        };
    }

    [[nodiscard]] std::vector<MarketEvent> BuildMinCommitMailbox()
    {
        return {
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 8 },
            MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 8 },
            MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 8 }
        };
    }

    void Consume(const HftRunOutput& output)
    {
        g_hftBenchSink += output.finalState.submitCount;
        g_hftBenchSink += output.finalState.cancelCount;
        g_hftBenchSink += output.finalState.reentrySubmitCount;
        g_hftBenchSink += output.finalState.reentryBlockedByHysteresisCount;
        g_hftBenchSink += output.finalState.staleCancelBlockedByMinCommitCount;
        g_hftBenchSink += output.finalState.orderStateFlipCount;
        g_hftBenchSink += static_cast<std::int64_t>(output.actuation.size());
        g_hftBenchSink += static_cast<std::int64_t>(output.outcomes.size());
    }
}

BENCHMARK_WITH_ITERATIONS(DragonHft_ReentryBaselineBench, 10000)
{
    (void)context;
    const HftRunOutput output = RunHftGoldenPath(BuildBaselineState(), BuildBaselineMailbox());
    Consume(output);
}

BENCHMARK_WITH_ITERATIONS(DragonHft_ReentryHysteresisBench, 10000)
{
    (void)context;
    const HftRunOutput output = RunHftGoldenPath(BuildHysteresisState(), BuildHysteresisMailbox());
    Consume(output);
}

BENCHMARK_WITH_ITERATIONS(DragonHft_ReentryMinCommitBench, 10000)
{
    (void)context;
    const HftRunOutput output = RunHftGoldenPath(BuildMinCommitState(), BuildMinCommitMailbox());
    Consume(output);
}
