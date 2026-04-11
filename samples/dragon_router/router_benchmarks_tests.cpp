#include "../../tests/Marionette/test_harness.h"
#include "router_model.h"

#include <cstdint>
#include <vector>

namespace
{
    using namespace dragongod_samples::dragon_router;

    volatile std::int64_t g_routerBenchSink = 0;

    [[nodiscard]] RouterState BuildForwardState()
    {
        RouterState state{};
        state.routes.push_back(RouteEntry{ .destinationId = 10, .egressPort = 1, .healthy = true });
        state.ports.push_back(PortState{ .portId = 1, .linkUp = true, .queueFull = false, .congestionScore = 0 });
        return state;
    }

    [[nodiscard]] RouterState BuildUtilityChoiceState()
    {
        RouterState state{};
        state.routes.push_back(RouteEntry{ .destinationId = 10, .egressPort = 1, .healthy = true });
        state.routes.push_back(RouteEntry{ .destinationId = 10, .egressPort = 2, .healthy = true });
        state.ports.push_back(PortState{ .portId = 1, .linkUp = true, .queueFull = false, .congestionScore = 55 });
        state.ports.push_back(PortState{ .portId = 2, .linkUp = true, .queueFull = false, .congestionScore = 5 });
        return state;
    }

    [[nodiscard]] RouterState BuildQueueDrainState(const bool queueBlocked)
    {
        RouterState state{};
        state.routes.push_back(RouteEntry{ .destinationId = 11, .egressPort = 3, .healthy = true });
        state.ports.push_back(PortState{ .portId = 3, .linkUp = true, .queueFull = queueBlocked, .congestionScore = 10 });
        return state;
    }

    [[nodiscard]] Packet BuildPacket(const int destinationId, const std::uint64_t iteration)
    {
        return Packet{
            .packetId = static_cast<int>(1000 + (iteration % 100000)),
            .destinationId = destinationId,
            .ingressPort = 7,
            .priority = 0,
            .sizeBytes = 128
        };
    }

    void ConsumeResult(const RouterRunOutput& output)
    {
        g_routerBenchSink += output.finalState.forwardedCount;
        g_routerBenchSink += output.finalState.queuedCount;
        g_routerBenchSink += output.finalState.drainedCount;
        g_routerBenchSink += output.finalState.droppedCount;
        g_routerBenchSink += static_cast<std::int64_t>(output.actuation.size());
    }
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_ForwardKnownRouteBench, 10000)
{
    const Packet packet = BuildPacket(10, context.iteration);
    const RouterRunOutput output = RunRouterGoldenPath(BuildForwardState(), { packet });
    ConsumeResult(output);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_UtilityPathChoiceBench, 10000)
{
    const Packet packet = BuildPacket(10, context.iteration);
    const RouterRunOutput output = RunRouterGoldenPath(BuildUtilityChoiceState(), { packet });
    ConsumeResult(output);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_QueueRetryDrainBench, 5000)
{
    const Packet packet = BuildPacket(11, context.iteration);

    const RouterRunOutput queuedRun = RunRouterGoldenPath(BuildQueueDrainState(true), { packet });

    RouterState retryState = queuedRun.finalState;
    retryState.ports[0].queueFull = false;

    const RouterRunOutput drainedRun = RunRouterGoldenPath(retryState, {});

    ConsumeResult(queuedRun);
    ConsumeResult(drainedRun);
}
