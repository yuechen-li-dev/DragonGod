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
        state.routes.push_back(RouteEntry{ .destinationId = 11, .egressPort = 2, .healthy = true });
        state.ports.push_back(PortState{ .portId = 1, .linkUp = true, .queueFull = false });
        state.ports.push_back(PortState{ .portId = 2, .linkUp = true, .queueFull = true });
        return state;
    }
}

FACT(M12a_Runtime_GoldenPath_CoversForwardDropQueue)
{
    const std::vector<Packet> packets{
        Packet{ .packetId = 100, .destinationId = 10, .ingressPort = 8, .priority = 0, .sizeBytes = 100 },
        Packet{ .packetId = 101, .destinationId = 99, .ingressPort = 8, .priority = 0, .sizeBytes = 100 },
        Packet{ .packetId = 102, .destinationId = 11, .ingressPort = 8, .priority = 0, .sizeBytes = 100 }
    };

    const RouterRunOutput run = RunRouterGoldenPath(BuildStateForRuntime(), packets);

    ASSERT_EQUAL(1, run.finalState.forwardedCount, "golden path should forward known healthy route");
    ASSERT_EQUAL(1, run.finalState.droppedCount, "golden path should drop unknown route");
    ASSERT_EQUAL(1, run.finalState.queuedCount, "golden path should queue blocked egress packet");
    ASSERT_EQUAL(3, static_cast<int>(run.packetResults.size()), "each packet should produce one result");
}

FACT(M12a_Runtime_Replay_IsDeterministic)
{
    const std::vector<Packet> packets{
        Packet{ .packetId = 200, .destinationId = 10, .ingressPort = 8, .priority = 1, .sizeBytes = 64 },
        Packet{ .packetId = 201, .destinationId = 11, .ingressPort = 8, .priority = 1, .sizeBytes = 64 },
        Packet{ .packetId = 202, .destinationId = 99, .ingressPort = 8, .priority = 1, .sizeBytes = 64 }
    };

    const RouterRunOutput runA = RunRouterGoldenPath(BuildStateForRuntime(), packets);
    const RouterRunOutput runB = RunRouterGoldenPath(BuildStateForRuntime(), packets);

    ASSERT_TRUE(runA.packetResults == runB.packetResults, "replays should produce identical packet outcomes");
    ASSERT_TRUE(runA.actuation == runB.actuation, "replays should produce identical actuation ordering");
    ASSERT_TRUE(runA.trace == runB.trace, "replays should produce identical router trace");
    ASSERT_TRUE(runA.finalState == runB.finalState, "replays should produce identical final state");
}
