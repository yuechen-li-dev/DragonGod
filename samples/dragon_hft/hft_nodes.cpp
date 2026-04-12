#include "hft_model.h"

namespace dragongod_samples::dragon_hft
{
    enum class HftPhase
    {
        IngestMarketEvent,
        Decision,
        PlaceBuy,
        PlaceSell,
        Hold,
        Completed
    };

    struct HftScratch
    {
        MarketEvent event{};
        OrderSide desiredSide = OrderSide::None;
        bool blockedDuplicate = false;
        std::string holdReason;
    };

    struct FrameCtx
    {
        HftState& bb;
        const MarketEvent& mailboxEvent;
        HftScratch& scratch;
        std::vector<HftActuation>& actuation;
        std::vector<std::string>& trace;

        [[nodiscard]] HftState& Bb() { return bb; }
    };

    [[nodiscard]] HftPhase IngestMarketEventFrame(FrameCtx& ctx)
    {
        ctx.scratch.event = ctx.mailboxEvent;
        ctx.Bb().latestSignal = ctx.scratch.event.signal;
        ctx.trace.push_back("Ingest signal=" + std::to_string(ctx.scratch.event.signal));
        return HftPhase::Decision;
    }

    [[nodiscard]] HftPhase DecisionFrame(FrameCtx& ctx)
    {
        const OrderSide desiredSide = DesiredOrderSide(ctx.Bb().latestSignal, ctx.Bb().threshold);
        ctx.scratch.desiredSide = desiredSide;
        ctx.scratch.blockedDuplicate = false;

        if (desiredSide == OrderSide::None) {
            ctx.scratch.holdReason = "NoEdge";
            return HftPhase::Hold;
        }

        if (!ShouldSubmitOrder(desiredSide, ctx.Bb().outstandingOrder)) {
            ctx.scratch.blockedDuplicate = true;
            ctx.scratch.holdReason = "DuplicateOutstandingOrderBlocked";
            return HftPhase::Hold;
        }

        if (desiredSide == OrderSide::Buy) {
            return HftPhase::PlaceBuy;
        }

        return HftPhase::PlaceSell;
    }

    [[nodiscard]] HftPhase PlaceBuyFrame(FrameCtx& ctx)
    {
        ctx.Bb().outstandingOrder = OrderSide::Buy;
        ctx.Bb().submitCount += 1;

        ctx.actuation.push_back(HftActuation{
            .kind = DecisionKind::SubmitBuy,
            .price = PriceForOrder(ctx.scratch.event, OrderSide::Buy),
            .reason = "ActionablePositiveSignal"
        });

        ctx.trace.push_back(
            "PlaceBuy signal=" + std::to_string(ctx.Bb().latestSignal) +
            " price=" + std::to_string(ctx.actuation.back().price));
        return HftPhase::Completed;
    }

    [[nodiscard]] HftPhase PlaceSellFrame(FrameCtx& ctx)
    {
        ctx.Bb().outstandingOrder = OrderSide::Sell;
        ctx.Bb().submitCount += 1;

        ctx.actuation.push_back(HftActuation{
            .kind = DecisionKind::SubmitSell,
            .price = PriceForOrder(ctx.scratch.event, OrderSide::Sell),
            .reason = "ActionableNegativeSignal"
        });

        ctx.trace.push_back(
            "PlaceSell signal=" + std::to_string(ctx.Bb().latestSignal) +
            " price=" + std::to_string(ctx.actuation.back().price));
        return HftPhase::Completed;
    }

    [[nodiscard]] HftPhase HoldFrame(FrameCtx& ctx)
    {
        ctx.Bb().holdCount += 1;
        ctx.trace.push_back(
            "Hold signal=" + std::to_string(ctx.Bb().latestSignal) +
            " reason=" + ctx.scratch.holdReason);
        return HftPhase::Completed;
    }

    [[nodiscard]] EventOutcome RunSingleEvent(HftState& state, const MarketEvent& event, std::vector<HftActuation>& actuation, std::vector<std::string>& trace)
    {
        HftScratch scratch{};
        FrameCtx ctx{
            .bb = state,
            .mailboxEvent = event,
            .scratch = scratch,
            .actuation = actuation,
            .trace = trace
        };

        HftPhase phase = HftPhase::IngestMarketEvent;
        while (phase != HftPhase::Completed) {
            switch (phase) {
            case HftPhase::IngestMarketEvent:
                phase = IngestMarketEventFrame(ctx);
                break;
            case HftPhase::Decision:
                phase = DecisionFrame(ctx);
                break;
            case HftPhase::PlaceBuy:
                phase = PlaceBuyFrame(ctx);
                break;
            case HftPhase::PlaceSell:
                phase = PlaceSellFrame(ctx);
                break;
            case HftPhase::Hold:
                phase = HoldFrame(ctx);
                break;
            case HftPhase::Completed:
                break;
            }
        }

        EventOutcome outcome{};
        outcome.outstandingAfter = state.outstandingOrder;
        outcome.blockedDuplicate = scratch.blockedDuplicate;

        if (scratch.desiredSide == OrderSide::Buy && !scratch.blockedDuplicate) {
            outcome.decision = DecisionKind::SubmitBuy;
            outcome.reason = "SubmittedBuy";
            return outcome;
        }

        if (scratch.desiredSide == OrderSide::Sell && !scratch.blockedDuplicate) {
            outcome.decision = DecisionKind::SubmitSell;
            outcome.reason = "SubmittedSell";
            return outcome;
        }

        outcome.decision = DecisionKind::Hold;
        outcome.reason = scratch.holdReason;
        return outcome;
    }

    [[nodiscard]] HftRunOutput RunHftGoldenPath(const HftState& initialState, const std::vector<MarketEvent>& mailboxEvents)
    {
        HftRunOutput output{};
        output.finalState = initialState;

        for (const MarketEvent& event : mailboxEvents) {
            const EventOutcome outcome = RunSingleEvent(output.finalState, event, output.actuation, output.trace);
            output.outcomes.push_back(outcome);
        }

        return output;
    }
}
