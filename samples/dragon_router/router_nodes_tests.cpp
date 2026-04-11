#include "../../tests/Marionette/test_harness.h"
#include "router_model.h"

namespace
{
    using namespace dragongod_samples::dragon_router;

    [[nodiscard]] RouterState BaseState()
    {
        RouterState state{};
        state.routes.push_back(RouteEntry{ .destinationId = 10, .egressPort = 2, .healthy = true });
        state.routes.push_back(RouteEntry{ .destinationId = 10, .egressPort = 3, .healthy = true });
        state.ports.push_back(PortState{ .portId = 2, .linkUp = true, .queueFull = false, .congestionScore = 75 });
        state.ports.push_back(PortState{ .portId = 3, .linkUp = true, .queueFull = false, .congestionScore = 10 });
        return state;
    }
}

FACT(M12b_Nodes_ForwardKnownRoute_UsesUtilitySelectedPort)
{
    const Packet packet{ .packetId = 1, .destinationId = 10, .ingressPort = 8, .priority = 0, .sizeBytes = 128 };
    const RouterRunOutput run = RunRouterGoldenPath(BaseState(), { packet });

    ASSERT_EQUAL(1, run.finalState.forwardedCount, "known healthy route should increment forwarded counter");
    ASSERT_EQUAL(0, run.finalState.droppedCount, "known healthy route should not drop");
    ASSERT_EQUAL(0, run.finalState.queuedCount, "known healthy route should not queue");
    ASSERT_EQUAL(1, static_cast<int>(run.actuation.size()), "single packet should produce single actuation");
    ASSERT_EQUAL(static_cast<int>(ActuationKind::ForwardPort), static_cast<int>(run.actuation[0].kind), "known route should emit forward actuation");
    ASSERT_EQUAL(3, run.actuation[0].portId, "forward actuation should target lower pressure egress port");
}

FACT(M12b_Nodes_UnknownRoute_EmitsDropActuation)
{
    const Packet packet{ .packetId = 2, .destinationId = 77, .ingressPort = 8, .priority = 0, .sizeBytes = 128 };
    const RouterRunOutput run = RunRouterGoldenPath(BaseState(), { packet });

    ASSERT_EQUAL(0, run.finalState.forwardedCount, "unknown route should not forward");
    ASSERT_EQUAL(1, run.finalState.droppedCount, "unknown route should increment dropped counter");
    ASSERT_EQUAL(static_cast<int>(ActuationKind::DropPacket), static_cast<int>(run.actuation[0].kind), "unknown route should emit drop actuation");
}

FACT(M12b_Nodes_AllCandidatePathsBlocked_QueuesPacket)
{
    RouterState state = BaseState();
    state.ports[0].queueFull = true;
    state.ports[1].queueFull = true;

    const Packet packet{ .packetId = 3, .destinationId = 10, .ingressPort = 8, .priority = 0, .sizeBytes = 128 };
    const RouterRunOutput run = RunRouterGoldenPath(state, { packet });

    ASSERT_EQUAL(0, run.finalState.forwardedCount, "blocked egress should not forward immediately");
    ASSERT_EQUAL(1, run.finalState.queuedCount, "blocked egress should queue");
    ASSERT_EQUAL(1, static_cast<int>(run.finalState.queuedPackets.size()), "queued packet should persist in state");
    ASSERT_EQUAL(3, run.finalState.queuedPackets[0].packet.packetId, "queued packet identity should be preserved");
    ASSERT_EQUAL(static_cast<int>(ActuationKind::QueuePacket), static_cast<int>(run.actuation[0].kind), "blocked egress should emit queue actuation");
}
