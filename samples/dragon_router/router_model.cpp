#include "router_model.h"

namespace dragongod_samples::dragon_router
{
    [[nodiscard]] PacketResult ExecutePacket(
        RouterState& state,
        const Packet& packet,
        std::vector<Actuation>& actuation,
        std::vector<std::string>& trace);

    [[nodiscard]] const RouteEntry* FindRoute(const RouterState& state, const int destinationId)
    {
        for (const RouteEntry& route : state.routes) {
            if (route.destinationId == destinationId) {
                return &route;
            }
        }

        return nullptr;
    }

    [[nodiscard]] const PortState* FindPort(const RouterState& state, const int portId)
    {
        for (const PortState& port : state.ports) {
            if (port.portId == portId) {
                return &port;
            }
        }

        return nullptr;
    }

    [[nodiscard]] bool ShouldQueuePacket(const PortState& port)
    {
        return !port.linkUp || port.queueFull;
    }

    [[nodiscard]] RouterRunOutput RunRouterGoldenPath(
        const RouterState& initialState,
        const std::vector<Packet>& incomingPackets)
    {
        RouterRunOutput output{};
        output.finalState = initialState;

        for (const Packet& packet : incomingPackets) {
            const PacketResult result = ExecutePacket(output.finalState, packet, output.actuation, output.trace);
            output.packetResults.push_back(result);
        }

        return output;
    }
}
