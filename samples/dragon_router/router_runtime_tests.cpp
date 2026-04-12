#include "../../tests/Marionette/test_harness.h"
#include "router_model.h"

#include <vector>

namespace
{
    using namespace dragongod_samples::dragon_router;

    [[nodiscard]] RouterState BuildStateForRuntime()
    {
        RouterState state{};
        state.routes.push_back(RouteEntry{ .destinationId = 10, .egressPort = 1, .healthy = true });
        state.routes.push_back(RouteEntry{ .destinationId = 10, .egressPort = 2, .healthy = true });
        state.routes.push_back(RouteEntry{ .destinationId = 11, .egressPort = 3, .healthy = true });
        state.ports.push_back(PortState{ .portId = 1, .linkUp = true, .queueFull = false, .congestionScore = 90 });
        state.ports.push_back(PortState{ .portId = 2, .linkUp = true, .queueFull = false, .congestionScore = 5 });
        state.ports.push_back(PortState{ .portId = 3, .linkUp = true, .queueFull = true, .congestionScore = 50 });
        return state;
    }
}

FACT(M12b_Runtime_GoldenPath_CoversUtilityForwardDropQueue)
{
    const std::vector<Packet> packets{
        Packet{ .packetId = 100, .destinationId = 10, .ingressPort = 8, .priority = 0, .sizeBytes = 100 },
        Packet{ .packetId = 101, .destinationId = 99, .ingressPort = 8, .priority = 0, .sizeBytes = 100 },
        Packet{ .packetId = 102, .destinationId = 11, .ingressPort = 8, .priority = 0, .sizeBytes = 100 }
    };

    const RouterRunOutput run = RunRouterGoldenPath(BuildStateForRuntime(), packets);

    ASSERT_EQUAL(1, run.finalState.forwardedCount, "runtime path should forward one known destination packet");
    ASSERT_EQUAL(1, run.finalState.droppedCount, "runtime path should drop unknown route");
    ASSERT_EQUAL(1, run.finalState.queuedCount, "runtime path should queue blocked destination packet");
    ASSERT_EQUAL(2, run.packetResults[0].selectedPort, "utility path selection should pick lower-pressure candidate");
    ASSERT_EQUAL(3, static_cast<int>(run.packetResults.size()), "each packet should produce one result");
}

FACT(M12b_Runtime_DegradedPrimary_UsesAlternatePath)
{
    RouterState state = BuildStateForRuntime();
    state.ports[0].queueFull = true;
    state.ports[1].queueFull = false;

    const Packet packet{ .packetId = 200, .destinationId = 10, .ingressPort = 8, .priority = 1, .sizeBytes = 64 };
    const RouterRunOutput run = RunRouterGoldenPath(state, { packet });

    ASSERT_EQUAL(
        static_cast<int>(PacketOutcomeKind::Forwarded),
        static_cast<int>(run.packetResults[0].outcome),
        "alternate path should still forward");
    ASSERT_EQUAL(2, run.packetResults[0].selectedPort, "blocked primary should route to viable alternate path");
}

FACT(M12b_Runtime_QueuedPacketDrainsAfterDeferredRetry)
{
    RouterState initial = BuildStateForRuntime();
    initial.ports[2].queueFull = true;

    const Packet firstPacket{ .packetId = 300, .destinationId = 11, .ingressPort = 8, .priority = 1, .sizeBytes = 64 };
    const RouterRunOutput runA = RunRouterGoldenPath(initial, { firstPacket });

    ASSERT_EQUAL(1, static_cast<int>(runA.finalState.queuedPackets.size()), "first run should queue packet when no usable path exists");

    RouterState retryState = runA.finalState;
    retryState.ports[2].queueFull = false;

    const RouterRunOutput runB = RunRouterGoldenPath(retryState, {});

    ASSERT_EQUAL(0, static_cast<int>(runB.finalState.queuedPackets.size()), "retry pass should drain queued packet when path recovers");
    ASSERT_EQUAL(1, runB.finalState.drainedCount, "drain counter should record deferred forward");
    ASSERT_EQUAL(static_cast<int>(ActuationKind::DrainForwarded), static_cast<int>(runB.actuation[0].kind), "deferred retry should emit drain-forwarded actuation");
    ASSERT_EQUAL(3, runB.actuation[0].portId, "drained packet should forward through recovered path");
}

FACT(M12b_Runtime_Replay_IsDeterministic)
{
    const std::vector<Packet> packets{
        Packet{ .packetId = 400, .destinationId = 10, .ingressPort = 8, .priority = 1, .sizeBytes = 64 },
        Packet{ .packetId = 401, .destinationId = 11, .ingressPort = 8, .priority = 1, .sizeBytes = 64 },
        Packet{ .packetId = 402, .destinationId = 99, .ingressPort = 8, .priority = 1, .sizeBytes = 64 }
    };

    const RouterRunOutput runA = RunRouterGoldenPath(BuildStateForRuntime(), packets);
    const RouterRunOutput runB = RunRouterGoldenPath(BuildStateForRuntime(), packets);

    ASSERT_TRUE(runA.packetResults == runB.packetResults, "replays should produce identical packet outcomes");
    ASSERT_TRUE(runA.actuation == runB.actuation, "replays should produce identical actuation ordering");
    ASSERT_TRUE(runA.trace == runB.trace, "replays should produce identical router trace");
    ASSERT_TRUE(runA.finalState == runB.finalState, "replays should produce identical final state");
}

FACT(M12f_Runtime_BackoffRetry_SpacesDeferredAttempts)
{
    RouterState state = BuildStateForRuntime();
    state.retryHeuristic = RouterState::RetryHeuristic::BackoffDelay;
    state.retryDelayTicks = 1;
    state.maxBackoffDelayTicks = 3;
    state.ports[2].queueFull = true;

    const Packet packet{ .packetId = 500, .destinationId = 11, .ingressPort = 8, .priority = 0, .sizeBytes = 64 };
    const RouterRunOutput queuedRun = RunRouterGoldenPath(state, { packet });
    const RouterRunOutput blockedRetryA = RunRouterGoldenPath(queuedRun.finalState, {});
    const RouterRunOutput blockedRetryB = RunRouterGoldenPath(blockedRetryA.finalState, {});

    ASSERT_EQUAL(1, blockedRetryA.finalState.retryAttempts, "first blocked retry pass should include exactly one additional retry attempt");
    ASSERT_EQUAL(2, blockedRetryB.finalState.retryAttempts, "backoff should defer another retry attempt in the next blocked pass");
    ASSERT_EQUAL(7, blockedRetryB.finalState.queuedPackets[0].nextRetryTick, "backoff retry should schedule the next attempt farther out");
}

FACT(M12f_Runtime_ConditionAwareRetry_SkipsDoomedRetryPass)
{
    RouterState state = BuildStateForRuntime();
    state.retryHeuristic = RouterState::RetryHeuristic::ConditionAware;
    state.retryConditionMaxCongestion = 70;
    state.ports[2].queueFull = true;

    const Packet packet{ .packetId = 510, .destinationId = 11, .ingressPort = 8, .priority = 0, .sizeBytes = 64 };
    const RouterRunOutput queuedRun = RunRouterGoldenPath(state, { packet });
    const RouterRunOutput blockedRetryRun = RunRouterGoldenPath(queuedRun.finalState, {});

    ASSERT_EQUAL(0, blockedRetryRun.finalState.retryAttempts, "condition-aware mode should avoid retrying while no recovery signal exists");
    ASSERT_EQUAL(2, blockedRetryRun.finalState.retrySkippedCount, "condition-aware mode should count each skipped retry pass");
    ASSERT_EQUAL(1, static_cast<int>(blockedRetryRun.finalState.queuedPackets.size()), "packet should remain queued while path is still blocked");
}

FACT(M12f_Runtime_ConditionAwareRetry_DrainsAfterRecoverySignal)
{
    RouterState state = BuildStateForRuntime();
    state.retryHeuristic = RouterState::RetryHeuristic::ConditionAware;
    state.retryConditionMaxCongestion = 70;
    state.ports[2].queueFull = true;

    const Packet packet{ .packetId = 520, .destinationId = 11, .ingressPort = 8, .priority = 0, .sizeBytes = 64 };
    const RouterRunOutput queuedRun = RunRouterGoldenPath(state, { packet });
    const RouterRunOutput blockedRetryRun = RunRouterGoldenPath(queuedRun.finalState, {});

    RouterState recovered = blockedRetryRun.finalState;
    recovered.ports[2].queueFull = false;

    const RouterRunOutput drainRun = RunRouterGoldenPath(recovered, {});

    ASSERT_EQUAL(0, static_cast<int>(drainRun.finalState.queuedPackets.size()), "queue should drain once condition-aware recovery signal appears");
    ASSERT_EQUAL(1, drainRun.finalState.drainedCount, "recovered retry should still forward through deferred drain");
}
