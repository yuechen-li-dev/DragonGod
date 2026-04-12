#include "../../tests/Marionette/test_harness.h"
#include "hft_model.h"

#include <cstdint>
#include <vector>

namespace
{
    using namespace dragongod_samples::dragon_hft;

    volatile std::int64_t g_hftBenchSink = 0;

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

BENCHMARK_WITH_ITERATIONS(DragonHft_ReentryOscillationBaselineBench, 10000)
{
    (void)context;
    const HftRunOutput output = RunHftGoldenPath(BuildReentryOscillationBaselineState(), BuildReentryOscillationMailbox());
    Consume(output);
}

BENCHMARK_WITH_ITERATIONS(DragonHft_ReentryOscillationHysteresisBench, 10000)
{
    (void)context;
    const HftRunOutput output = RunHftGoldenPath(BuildReentryOscillationHysteresisState(), BuildReentryOscillationMailbox());
    Consume(output);
}

BENCHMARK_WITH_ITERATIONS(DragonHft_ReentryOscillationMinCommitBench, 10000)
{
    (void)context;
    const HftRunOutput output = RunHftGoldenPath(BuildReentryOscillationMinCommitState(), BuildReentryOscillationMailbox());
    Consume(output);
}
