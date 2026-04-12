#pragma once

#include <string>
#include <vector>

namespace dragongod_samples::dragon_router
{
    struct Packet
    {
        int packetId = 0;
        int destinationId = 0;
        int ingressPort = 0;
        int priority = 0;
        int sizeBytes = 0;

        [[nodiscard]] bool operator==(const Packet& other) const = default;
    };

    struct RouteEntry
    {
        int destinationId = 0;
        int egressPort = 0;
        bool healthy = true;

        [[nodiscard]] bool operator==(const RouteEntry& other) const = default;
    };

    struct PortState
    {
        int portId = 0;
        bool linkUp = true;
        bool queueFull = false;
        int congestionScore = 0;

        [[nodiscard]] bool operator==(const PortState& other) const = default;
    };

    struct QueueEntry
    {
        Packet packet{};
        int retryCount = 0;
        int nextRetryTick = 0;

        [[nodiscard]] bool operator==(const QueueEntry& other) const = default;
    };

    struct RouterState
    {
        enum class RetryHeuristic
        {
            BaselineFixedDelay,
            BackoffDelay,
            ConditionAware
        };

        std::vector<RouteEntry> routes;
        std::vector<PortState> ports;
        std::vector<QueueEntry> queuedPackets;
        int forwardedCount = 0;
        int droppedCount = 0;
        int queuedCount = 0;
        int drainedCount = 0;
        int retryAttempts = 0;
        int retrySkippedCount = 0;
        int currentTick = 0;
        int retryDelayTicks = 1;
        int maxBackoffDelayTicks = 3;
        int retryConditionMaxCongestion = 70;
        RetryHeuristic retryHeuristic = RetryHeuristic::BaselineFixedDelay;

        [[nodiscard]] bool operator==(const RouterState& other) const = default;
    };

    enum class PacketOutcomeKind
    {
        Forwarded,
        Dropped,
        Queued
    };

    enum class ActuationKind
    {
        ForwardPort,
        DropPacket,
        QueuePacket,
        RetryDeferred,
        DrainForwarded
    };

    struct Actuation
    {
        ActuationKind kind = ActuationKind::DropPacket;
        int packetId = 0;
        int portId = -1;
        std::string reason;

        [[nodiscard]] bool operator==(const Actuation& other) const = default;
    };

    struct PacketResult
    {
        int packetId = 0;
        PacketOutcomeKind outcome = PacketOutcomeKind::Dropped;
        int selectedPort = -1;

        [[nodiscard]] bool operator==(const PacketResult& other) const = default;
    };

    struct RouterRunOutput
    {
        RouterState finalState{};
        std::vector<PacketResult> packetResults;
        std::vector<Actuation> actuation;
        std::vector<std::string> trace;

        [[nodiscard]] bool operator==(const RouterRunOutput& other) const = default;
    };

    [[nodiscard]] const RouteEntry* FindRoute(const RouterState& state, int destinationId);
    [[nodiscard]] std::vector<RouteEntry> FindCandidateRoutes(const RouterState& state, int destinationId);
    [[nodiscard]] const PortState* FindPort(const RouterState& state, int portId);
    [[nodiscard]] bool ShouldQueuePacket(const PortState& port);
    [[nodiscard]] bool IsPortUsable(const PortState& port);
    [[nodiscard]] int PortUtilityScore(const PortState& port);
    [[nodiscard]] int SelectBestEgressPort(const RouterState& state, int destinationId);

    [[nodiscard]] RouterRunOutput RunRouterGoldenPath(
        const RouterState& initialState,
        const std::vector<Packet>& incomingPackets);
}
