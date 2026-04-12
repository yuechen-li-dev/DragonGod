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

    [[nodiscard]] RouterState BuildUtilityChoiceState(const int candidateCount)
    {
        RouterState state{};

        const int boundedCandidates = candidateCount < 1 ? 1 : candidateCount;
        for (int index = 0; index < boundedCandidates; ++index) {
            const int portId = index + 1;
            state.routes.push_back(RouteEntry{
                .destinationId = 10,
                .egressPort = portId,
                .healthy = true
            });

            state.ports.push_back(PortState{
                .portId = portId,
                .linkUp = true,
                .queueFull = false,
                .congestionScore = 70 - (index * 5)
            });
        }

        state.ports[0].congestionScore = 0;
        return state;
    }

    [[nodiscard]] RouterState BuildQueueState(const bool queueBlocked)
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
        g_routerBenchSink += output.finalState.retryAttempts;
        g_routerBenchSink += output.finalState.retrySkippedCount;
        g_routerBenchSink += static_cast<std::int64_t>(output.actuation.size());
    }

    void RunHeavyQueueRetryScenario(
        RouterState::RetryHeuristic heuristic,
        const std::uint64_t iteration)
    {
        RouterState state = BuildQueueState(true);
        state.retryHeuristic = heuristic;

        const Packet packetA = BuildPacket(11, iteration * 3);
        const Packet packetB = BuildPacket(11, iteration * 3 + 1);
        const Packet packetC = BuildPacket(11, iteration * 3 + 2);

        const RouterRunOutput queuedRun = RunRouterGoldenPath(state, { packetA, packetB, packetC });

        RouterState blockedRetryState = queuedRun.finalState;
        const RouterRunOutput blockedRetryRun = RunRouterGoldenPath(blockedRetryState, {});

        RouterState recoverState = blockedRetryRun.finalState;
        recoverState.ports[0].queueFull = false;
        const RouterRunOutput recoveredDrainRun = RunRouterGoldenPath(recoverState, {});

        ConsumeResult(queuedRun);
        ConsumeResult(blockedRetryRun);
        ConsumeResult(recoveredDrainRun);
    }
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_ForwardKnownRouteBench, 10000)
{
    const Packet packet = BuildPacket(10, context.iteration);
    const RouterRunOutput output = RunRouterGoldenPath(BuildForwardState(), { packet });
    ConsumeResult(output);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_UtilityCandidates1Bench, 10000)
{
    const Packet packet = BuildPacket(10, context.iteration);
    const RouterRunOutput output = RunRouterGoldenPath(BuildUtilityChoiceState(1), { packet });
    ConsumeResult(output);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_UtilityCandidates2Bench, 10000)
{
    const Packet packet = BuildPacket(10, context.iteration);
    const RouterRunOutput output = RunRouterGoldenPath(BuildUtilityChoiceState(2), { packet });
    ConsumeResult(output);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_UtilityCandidates4Bench, 10000)
{
    const Packet packet = BuildPacket(10, context.iteration);
    const RouterRunOutput output = RunRouterGoldenPath(BuildUtilityChoiceState(4), { packet });
    ConsumeResult(output);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_UtilityCandidates8Bench, 10000)
{
    const Packet packet = BuildPacket(10, context.iteration);
    const RouterRunOutput output = RunRouterGoldenPath(BuildUtilityChoiceState(8), { packet });
    ConsumeResult(output);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_QueueRetryLightBench, 5000)
{
    const Packet packet = BuildPacket(11, context.iteration);

    const RouterRunOutput queuedRun = RunRouterGoldenPath(BuildQueueState(true), { packet });

    RouterState retryState = queuedRun.finalState;
    retryState.ports[0].queueFull = false;

    const RouterRunOutput drainedRun = RunRouterGoldenPath(retryState, {});

    ConsumeResult(queuedRun);
    ConsumeResult(drainedRun);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_QueueRetryBaselineBench, 3000)
{
    RunHeavyQueueRetryScenario(RouterState::RetryHeuristic::BaselineFixedDelay, context.iteration);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_QueueRetryBackoffBench, 3000)
{
    RunHeavyQueueRetryScenario(RouterState::RetryHeuristic::BackoffDelay, context.iteration);
}

BENCHMARK_WITH_ITERATIONS(DragonRouter_QueueRetryConditionAwareBench, 3000)
{
    RunHeavyQueueRetryScenario(RouterState::RetryHeuristic::ConditionAware, context.iteration);
}
