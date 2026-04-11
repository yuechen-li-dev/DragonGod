#include "../../tests/Marionette/test_harness.h"
#include "router_model.h"

namespace
{
    using namespace dragongod_samples::dragon_router;
}

FACT(M12a_FindRoute_ReturnsKnownRoute)
{
    RouterState state{};
    state.routes.push_back(RouteEntry{ .destinationId = 101, .egressPort = 7, .healthy = true });

    const RouteEntry* route = FindRoute(state, 101);
    ASSERT_TRUE(route != nullptr, "known destination should resolve route");
    ASSERT_EQUAL(7, route->egressPort, "route should preserve configured egress port");
}

FACT(M12a_FindRoute_ReturnsNullForUnknownRoute)
{
    RouterState state{};
    state.routes.push_back(RouteEntry{ .destinationId = 101, .egressPort = 7, .healthy = true });

    const RouteEntry* route = FindRoute(state, 404);
    ASSERT_TRUE(route == nullptr, "unknown destination should not resolve route");
}

FACT(M12a_ShouldQueuePacket_ReflectsLinkAndQueueState)
{
    const PortState openPort{ .portId = 1, .linkUp = true, .queueFull = false };
    const PortState downPort{ .portId = 2, .linkUp = false, .queueFull = false };
    const PortState fullPort{ .portId = 3, .linkUp = true, .queueFull = true };

    ASSERT_FALSE(ShouldQueuePacket(openPort), "healthy port should not force queue");
    ASSERT_TRUE(ShouldQueuePacket(downPort), "link-down port should force queue");
    ASSERT_TRUE(ShouldQueuePacket(fullPort), "queue-full port should force queue");
}
