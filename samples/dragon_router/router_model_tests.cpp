#include "../../tests/Marionette/test_harness.h"
#include "router_model.h"

namespace
{
    using namespace dragongod_samples::dragon_router;
}

FACT(M12b_FindRoute_ReturnsKnownRoute)
{
    RouterState state{};
    state.routes.push_back(RouteEntry{ .destinationId = 101, .egressPort = 7, .healthy = true });

    const RouteEntry* route = FindRoute(state, 101);
    ASSERT_TRUE(route != nullptr, "known destination should resolve route");
    ASSERT_EQUAL(7, route->egressPort, "route should preserve configured egress port");
}

FACT(M12b_FindCandidateRoutes_ReturnsOnlyHealthyCandidates)
{
    RouterState state{};
    state.routes.push_back(RouteEntry{ .destinationId = 30, .egressPort = 1, .healthy = true });
    state.routes.push_back(RouteEntry{ .destinationId = 30, .egressPort = 2, .healthy = false });
    state.routes.push_back(RouteEntry{ .destinationId = 30, .egressPort = 3, .healthy = true });

    const std::vector<RouteEntry> candidates = FindCandidateRoutes(state, 30);

    ASSERT_EQUAL(2, static_cast<int>(candidates.size()), "only healthy candidate routes should remain");
    ASSERT_EQUAL(1, candidates[0].egressPort, "candidate order should remain stable");
    ASSERT_EQUAL(3, candidates[1].egressPort, "second healthy candidate should be retained");
}

FACT(M12b_ShouldQueuePacket_ReflectsLinkAndQueueState)
{
    const PortState openPort{ .portId = 1, .linkUp = true, .queueFull = false, .congestionScore = 10 };
    const PortState downPort{ .portId = 2, .linkUp = false, .queueFull = false, .congestionScore = 10 };
    const PortState fullPort{ .portId = 3, .linkUp = true, .queueFull = true, .congestionScore = 10 };

    ASSERT_FALSE(ShouldQueuePacket(openPort), "healthy port should not force queue");
    ASSERT_TRUE(ShouldQueuePacket(downPort), "link-down port should force queue");
    ASSERT_TRUE(ShouldQueuePacket(fullPort), "queue-full port should force queue");
}

FACT(M12b_SelectBestEgressPort_PrefersLowerPressureUsablePort)
{
    RouterState state{};
    state.routes.push_back(RouteEntry{ .destinationId = 20, .egressPort = 5, .healthy = true });
    state.routes.push_back(RouteEntry{ .destinationId = 20, .egressPort = 6, .healthy = true });
    state.ports.push_back(PortState{ .portId = 5, .linkUp = true, .queueFull = false, .congestionScore = 80 });
    state.ports.push_back(PortState{ .portId = 6, .linkUp = true, .queueFull = false, .congestionScore = 20 });

    const int selected = SelectBestEgressPort(state, 20);
    ASSERT_EQUAL(6, selected, "utility scoring should prefer lower congestion candidate");
}

FACT(M12b_SelectBestEgressPort_ReturnsMinusOneWhenAllCandidatesBlocked)
{
    RouterState state{};
    state.routes.push_back(RouteEntry{ .destinationId = 21, .egressPort = 5, .healthy = true });
    state.routes.push_back(RouteEntry{ .destinationId = 21, .egressPort = 6, .healthy = true });
    state.ports.push_back(PortState{ .portId = 5, .linkUp = false, .queueFull = false, .congestionScore = 20 });
    state.ports.push_back(PortState{ .portId = 6, .linkUp = true, .queueFull = true, .congestionScore = 20 });

    const int selected = SelectBestEgressPort(state, 21);
    ASSERT_EQUAL(-1, selected, "all blocked candidates should force queue path");
}

FACT(M12g_SelectPreferredQueuedPort_PrefersLeastCongestedCandidateWhenBlocked)
{
    RouterState state{};
    state.routes.push_back(RouteEntry{ .destinationId = 42, .egressPort = 7, .healthy = true });
    state.routes.push_back(RouteEntry{ .destinationId = 42, .egressPort = 8, .healthy = true });
    state.ports.push_back(PortState{ .portId = 7, .linkUp = true, .queueFull = true, .congestionScore = 10 });
    state.ports.push_back(PortState{ .portId = 8, .linkUp = true, .queueFull = true, .congestionScore = 60 });

    const int preferred = SelectPreferredQueuedPort(state, 42);
    ASSERT_EQUAL(7, preferred, "queued preference should keep the lower-pressure original candidate deterministic");
}

FACT(M12g_SelectPreferredQueuedPort_AvoidsLinkDownWhenAlternativeExists)
{
    RouterState state{};
    state.routes.push_back(RouteEntry{ .destinationId = 43, .egressPort = 9, .healthy = true });
    state.routes.push_back(RouteEntry{ .destinationId = 43, .egressPort = 10, .healthy = true });
    state.ports.push_back(PortState{ .portId = 9, .linkUp = false, .queueFull = false, .congestionScore = 0 });
    state.ports.push_back(PortState{ .portId = 10, .linkUp = true, .queueFull = true, .congestionScore = 30 });

    const int preferred = SelectPreferredQueuedPort(state, 43);
    ASSERT_EQUAL(10, preferred, "queued preference should penalize link-down candidates");
}
