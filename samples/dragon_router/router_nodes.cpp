#include "router_model.h"

namespace dragongod_samples::dragon_router
{
    enum class RouterPhase
    {
        Ingress = 0,
        RouteDecision = 1,
        Forward = 2,
        Queue = 3,
        Drop = 4,
        Completed = 5
    };

    enum class RouterControlKind
    {
        Continue,
        Complete,
        Fail
    };

    struct RouterControl
    {
        RouterControlKind kind = RouterControlKind::Continue;
        RouterPhase next = RouterPhase::Completed;
    };

    struct RouterScratch
    {
        int selectedPort = -1;
    };

    struct RouterFrameCtx
    {
        RouterState& bb;
        const Packet& packet;
        RouterScratch& scratch;
        std::vector<Actuation>& actuation;
        std::vector<std::string>& trace;

        [[nodiscard]] RouterState& Bb() { return bb; }
    };

    [[nodiscard]] RouterControl IngressFrame(RouterFrameCtx& ctx)
    {
        ctx.trace.push_back("Ingress packet=" + std::to_string(ctx.packet.packetId));
        return RouterControl{ .kind = RouterControlKind::Continue, .next = RouterPhase::RouteDecision };
    }

    [[nodiscard]] RouterControl RouteDecisionFrame(RouterFrameCtx& ctx)
    {
        const std::vector<RouteEntry> candidates = FindCandidateRoutes(ctx.Bb(), ctx.packet.destinationId);
        if (candidates.empty()) {
            ctx.trace.push_back("RouteDecision packet=" + std::to_string(ctx.packet.packetId) + " route=unknown");
            return RouterControl{ .kind = RouterControlKind::Continue, .next = RouterPhase::Drop };
        }

        const int selectedPort = SelectBestEgressPort(ctx.Bb(), ctx.packet.destinationId);
        if (selectedPort >= 0) {
            ctx.scratch.selectedPort = selectedPort;
            ctx.trace.push_back(
                "RouteDecision packet=" + std::to_string(ctx.packet.packetId) +
                " action=forward port=" + std::to_string(selectedPort));
            return RouterControl{ .kind = RouterControlKind::Continue, .next = RouterPhase::Forward };
        }

        ctx.trace.push_back(
            "RouteDecision packet=" + std::to_string(ctx.packet.packetId) +
            " action=queue reason=no-usable-path");
        return RouterControl{ .kind = RouterControlKind::Continue, .next = RouterPhase::Queue };
    }

    [[nodiscard]] RouterControl ForwardFrame(RouterFrameCtx& ctx)
    {
        ctx.Bb().forwardedCount += 1;
        ctx.actuation.push_back(Actuation{
            .kind = ActuationKind::ForwardPort,
            .packetId = ctx.packet.packetId,
            .portId = ctx.scratch.selectedPort,
            .reason = "UtilitySelectedPath"
        });
        ctx.trace.push_back("Forward packet=" + std::to_string(ctx.packet.packetId) + " port=" + std::to_string(ctx.scratch.selectedPort));
        return RouterControl{ .kind = RouterControlKind::Complete, .next = RouterPhase::Completed };
    }

    [[nodiscard]] RouterControl QueueFrame(RouterFrameCtx& ctx)
    {
        ctx.Bb().queuedCount += 1;
        const int preferredPort = SelectPreferredQueuedPort(ctx.Bb(), ctx.packet.destinationId);
        ctx.Bb().queuedPackets.push_back(QueueEntry{
            .packet = ctx.packet,
            .preferredPortId = preferredPort,
            .retryCount = 0,
            .nextRetryTick = ctx.Bb().currentTick + ctx.Bb().retryDelayTicks
        });
        ctx.actuation.push_back(Actuation{
            .kind = ActuationKind::QueuePacket,
            .packetId = ctx.packet.packetId,
            .portId = -1,
            .reason = "AllCandidatePathsUnavailable"
        });
        ctx.trace.push_back(
            "Queue packet=" + std::to_string(ctx.packet.packetId) +
            " retry-at=" + std::to_string(ctx.Bb().currentTick + ctx.Bb().retryDelayTicks));
        return RouterControl{ .kind = RouterControlKind::Complete, .next = RouterPhase::Completed };
    }

    [[nodiscard]] RouterControl DropFrame(RouterFrameCtx& ctx)
    {
        ctx.Bb().droppedCount += 1;
        ctx.actuation.push_back(Actuation{
            .kind = ActuationKind::DropPacket,
            .packetId = ctx.packet.packetId,
            .portId = -1,
            .reason = "UnknownRoute"
        });
        ctx.trace.push_back("Drop packet=" + std::to_string(ctx.packet.packetId) + " reason=unknown-route");
        return RouterControl{ .kind = RouterControlKind::Complete, .next = RouterPhase::Completed };
    }

    [[nodiscard]] PacketResult ExecutePacket(RouterState& state, const Packet& packet, std::vector<Actuation>& actuation, std::vector<std::string>& trace)
    {
        RouterScratch scratch{};
        RouterFrameCtx ctx{
            .bb = state,
            .packet = packet,
            .scratch = scratch,
            .actuation = actuation,
            .trace = trace
        };

        RouterPhase phase = RouterPhase::Ingress;
        PacketResult result{ .packetId = packet.packetId, .outcome = PacketOutcomeKind::Dropped, .selectedPort = -1 };

        while (phase != RouterPhase::Completed) {
            RouterControl control{};
            switch (phase) {
            case RouterPhase::Ingress:
                control = IngressFrame(ctx);
                break;
            case RouterPhase::RouteDecision:
                control = RouteDecisionFrame(ctx);
                break;
            case RouterPhase::Forward:
                control = ForwardFrame(ctx);
                result.outcome = PacketOutcomeKind::Forwarded;
                result.selectedPort = scratch.selectedPort;
                break;
            case RouterPhase::Queue:
                control = QueueFrame(ctx);
                result.outcome = PacketOutcomeKind::Queued;
                result.selectedPort = -1;
                break;
            case RouterPhase::Drop:
                control = DropFrame(ctx);
                result.outcome = PacketOutcomeKind::Dropped;
                result.selectedPort = -1;
                break;
            case RouterPhase::Completed:
                control = RouterControl{ .kind = RouterControlKind::Complete, .next = RouterPhase::Completed };
                break;
            }

            if (control.kind == RouterControlKind::Fail) {
                break;
            }

            phase = control.next;
        }

        return result;
    }
}
