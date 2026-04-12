#include "router_model.h"

namespace dragongod_samples::dragon_router
{
    [[nodiscard]] int ComputeRetryDelayTicks(const RouterState& state, const int retryCount)
    {
        if (state.retryHeuristic != RouterState::RetryHeuristic::BackoffDelay) {
            return state.retryDelayTicks;
        }

        const int delayedTicks = state.retryDelayTicks + retryCount;
        if (delayedTicks > state.maxBackoffDelayTicks) {
            return state.maxBackoffDelayTicks;
        }

        return delayedTicks;
    }

    [[nodiscard]] bool HasConditionAwareRetrySignal(const RouterState& state, const int destinationId)
    {
        const std::vector<RouteEntry> candidates = FindCandidateRoutes(state, destinationId);
        for (const RouteEntry& route : candidates) {
            const PortState* port = FindPort(state, route.egressPort);
            if (port == nullptr) {
                continue;
            }

            if (port->linkUp && !port->queueFull && port->congestionScore <= state.retryConditionMaxCongestion) {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] PacketResult ExecutePacket(
        RouterState& state,
        const Packet& packet,
        std::vector<Actuation>& actuation,
        std::vector<std::string>& trace);

    void DrainQueuedPackets(
        RouterState& state,
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

    [[nodiscard]] std::vector<RouteEntry> FindCandidateRoutes(const RouterState& state, const int destinationId)
    {
        std::vector<RouteEntry> routes;

        for (const RouteEntry& route : state.routes) {
            if (route.destinationId == destinationId && route.healthy) {
                routes.push_back(route);
            }
        }

        return routes;
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

    [[nodiscard]] bool IsPortUsable(const PortState& port)
    {
        return port.linkUp && !port.queueFull;
    }

    [[nodiscard]] int PortUtilityScore(const PortState& port)
    {
        if (!IsPortUsable(port)) {
            return -1;
        }

        return 100 - port.congestionScore;
    }

    [[nodiscard]] int SelectBestEgressPort(const RouterState& state, const int destinationId)
    {
        const std::vector<RouteEntry> candidates = FindCandidateRoutes(state, destinationId);

        int selectedPort = -1;
        int selectedScore = -1;

        for (const RouteEntry& route : candidates) {
            const PortState* port = FindPort(state, route.egressPort);
            if (port == nullptr) {
                continue;
            }

            const int score = PortUtilityScore(*port);
            if (score < 0) {
                continue;
            }

            if (score > selectedScore ||
                (score == selectedScore && (selectedPort == -1 || route.egressPort < selectedPort))) {
                selectedScore = score;
                selectedPort = route.egressPort;
            }
        }

        return selectedPort;
    }

    [[nodiscard]] int SelectPreferredQueuedPort(const RouterState& state, const int destinationId)
    {
        const std::vector<RouteEntry> candidates = FindCandidateRoutes(state, destinationId);

        int selectedPort = -1;
        int selectedScore = -10000;

        for (const RouteEntry& route : candidates) {
            const PortState* port = FindPort(state, route.egressPort);
            if (port == nullptr) {
                continue;
            }

            int score = 100 - port->congestionScore;
            if (!port->linkUp) {
                score -= 1000;
            }
            if (port->queueFull) {
                score -= 200;
            }

            if (score > selectedScore ||
                (score == selectedScore && (selectedPort == -1 || route.egressPort < selectedPort))) {
                selectedScore = score;
                selectedPort = route.egressPort;
            }
        }

        return selectedPort;
    }

    [[nodiscard]] bool IsRoutePortUsable(const RouterState& state, const int destinationId, const int portId)
    {
        for (const RouteEntry& route : state.routes) {
            if (route.destinationId != destinationId || route.egressPort != portId || !route.healthy) {
                continue;
            }

            const PortState* port = FindPort(state, portId);
            if (port == nullptr) {
                return false;
            }

            return IsPortUsable(*port);
        }

        return false;
    }

    void DrainQueuedPackets(
        RouterState& state,
        std::vector<Actuation>& actuation,
        std::vector<std::string>& trace)
    {
        std::size_t index = 0;

        while (index < state.queuedPackets.size()) {
            QueueEntry& queued = state.queuedPackets[index];
            if (queued.nextRetryTick > state.currentTick) {
                ++index;
                continue;
            }

            if (state.retryHeuristic == RouterState::RetryHeuristic::ConditionAware &&
                !HasConditionAwareRetrySignal(state, queued.packet.destinationId)) {
                state.retrySkippedCount += 1;
                queued.nextRetryTick = state.currentTick + state.retryDelayTicks;
                actuation.push_back(Actuation{
                    .kind = ActuationKind::RetryDeferred,
                    .packetId = queued.packet.packetId,
                    .portId = -1,
                    .reason = "DeferredRetrySkippedNoRecoverySignal"
                });
                trace.push_back(
                    "Drain packet=" + std::to_string(queued.packet.packetId) +
                    " action=skip-retry signal=absent");
                ++index;
                continue;
            }

            int selectedPort = -1;
            if (state.drainPolicy == RouterState::DrainPolicy::PreferOriginalPath) {
                if (queued.preferredPortId >= 0 &&
                    IsRoutePortUsable(state, queued.packet.destinationId, queued.preferredPortId)) {
                    selectedPort = queued.preferredPortId;
                }
            }
            else {
                if (queued.preferredPortId >= 0 &&
                    IsRoutePortUsable(state, queued.packet.destinationId, queued.preferredPortId)) {
                    selectedPort = queued.preferredPortId;
                }
                else {
                    selectedPort = SelectBestEgressPort(state, queued.packet.destinationId);
                }
            }

            if (selectedPort >= 0) {
                state.forwardedCount += 1;
                state.drainedCount += 1;
                if (queued.preferredPortId == selectedPort) {
                    state.drainedPreferredPathCount += 1;
                }
                else {
                    state.drainedAlternatePathCount += 1;
                }
                actuation.push_back(Actuation{
                    .kind = ActuationKind::DrainForwarded,
                    .packetId = queued.packet.packetId,
                    .portId = selectedPort,
                    .reason = queued.preferredPortId == selectedPort
                        ? "DeferredRetryForwardedPreferredPath"
                        : "DeferredRetryForwardedAlternatePath"
                });
                trace.push_back(
                    "Drain packet=" + std::to_string(queued.packet.packetId) +
                    " action=forward port=" + std::to_string(selectedPort) +
                    " path=" + (queued.preferredPortId == selectedPort ? "preferred" : "alternate"));
                state.queuedPackets.erase(state.queuedPackets.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            queued.retryCount += 1;
            state.retryAttempts += 1;
            queued.nextRetryTick = state.currentTick + ComputeRetryDelayTicks(state, queued.retryCount);
            actuation.push_back(Actuation{
                .kind = ActuationKind::RetryDeferred,
                .packetId = queued.packet.packetId,
                .portId = -1,
                .reason = "DeferredRetryNoUsablePath"
            });
            trace.push_back(
                "Drain packet=" + std::to_string(queued.packet.packetId) +
                " action=defer retry=" + std::to_string(queued.retryCount) +
                " next-retry-at=" + std::to_string(queued.nextRetryTick));
            ++index;
        }
    }

    [[nodiscard]] RouterRunOutput RunRouterGoldenPath(
        const RouterState& initialState,
        const std::vector<Packet>& incomingPackets)
    {
        RouterRunOutput output{};
        output.finalState = initialState;

        output.finalState.currentTick += 1;
        DrainQueuedPackets(output.finalState, output.actuation, output.trace);

        for (const Packet& packet : incomingPackets) {
            const PacketResult result = ExecutePacket(output.finalState, packet, output.actuation, output.trace);
            output.packetResults.push_back(result);

            output.finalState.currentTick += 1;
            DrainQueuedPackets(output.finalState, output.actuation, output.trace);
        }

        return output;
    }
}
