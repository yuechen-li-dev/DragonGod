#include "hft_model.h"

namespace dragongod_samples::dragon_hft
{
    enum class HftPhase
    {
        IngestMarketEvent,
        StaleCheck,
        CancelStaleOrder,
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
        bool isReentryAttempt = false;
        bool blockedDuplicate = false;
        bool canceledStale = false;
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

    void RecordOutstandingFlip(HftState& state, const OrderSide previousSide, const OrderSide nextSide)
    {
        if (previousSide == nextSide) {
            return;
        }

        state.orderStateFlipCount += 1;
    }

    [[nodiscard]] HftPhase IngestMarketEventFrame(FrameCtx& ctx)
    {
        ctx.scratch.event = ctx.mailboxEvent;
        ctx.Bb().latestSignal = ctx.scratch.event.signal;
        AdvanceOutstandingAge(ctx.Bb());
        if (ctx.Bb().enableMinCommit && IsOutstandingActive(ctx.Bb()) && ctx.Bb().minCommitTicksRemaining > 0) {
            ctx.Bb().minCommitTicksRemaining -= 1;
        }
        if (ctx.Bb().awaitingReentryAfterStaleCancel) {
            ctx.Bb().ticksSinceStaleCancel += 1;
        }
        ctx.trace.push_back(
            "Ingest signal=" + std::to_string(ctx.scratch.event.signal) +
            " outstandingAgeTicks=" + std::to_string(ctx.Bb().outstandingAgeTicks));
        return HftPhase::StaleCheck;
    }

    [[nodiscard]] HftPhase StaleCheckFrame(FrameCtx& ctx)
    {
        if (ShouldCancelStaleOrder(ctx.Bb())) {
            if (ctx.Bb().enableMinCommit && ctx.Bb().minCommitTicksRemaining > 0) {
                ctx.Bb().staleCancelBlockedByMinCommitCount += 1;
                return HftPhase::Decision;
            }

            return HftPhase::CancelStaleOrder;
        }

        return HftPhase::Decision;
    }

    [[nodiscard]] HftPhase CancelStaleOrderFrame(FrameCtx& ctx)
    {
        const OrderSide canceledSide = ctx.Bb().outstandingOrder;
        RecordOutstandingFlip(ctx.Bb(), canceledSide, OrderSide::None);
        ctx.Bb().outstandingOrder = OrderSide::None;
        ctx.Bb().outstandingState = OutstandingState::None;
        ctx.Bb().outstandingAgeTicks = 0;
        ctx.Bb().minCommitTicksRemaining = 0;
        ctx.Bb().awaitingReentryAfterStaleCancel = true;
        ctx.Bb().ticksSinceStaleCancel = 0;
        ctx.Bb().cancelCount += 1;
        ctx.scratch.canceledStale = true;

        ctx.actuation.push_back(HftActuation{
            .kind = DecisionKind::CancelOrder,
            .price = 0,
            .reason = "StaleOutstandingOrder"
        });

        ctx.trace.push_back(
            "CancelStale side=" + std::to_string(static_cast<int>(canceledSide)) +
            " signal=" + std::to_string(ctx.Bb().latestSignal));
        return HftPhase::Completed;
    }

    [[nodiscard]] HftPhase DecisionFrame(FrameCtx& ctx)
    {
        const OrderSide desiredSide = DesiredOrderSide(ctx.Bb().latestSignal, ctx.Bb().threshold);
        ctx.scratch.desiredSide = desiredSide;
        ctx.scratch.isReentryAttempt = ctx.Bb().awaitingReentryAfterStaleCancel && desiredSide != OrderSide::None;
        ctx.scratch.blockedDuplicate = false;

        if (desiredSide == OrderSide::None) {
            ctx.scratch.holdReason = "NoEdge";
            return HftPhase::Hold;
        }

        if (ctx.scratch.isReentryAttempt && !ReentryHysteresisSatisfied(ctx.Bb().latestSignal, ctx.Bb())) {
            ctx.Bb().reentryBlockedByHysteresisCount += 1;
            ctx.scratch.holdReason = "ReentryBlockedByHysteresis";
            return HftPhase::Hold;
        }

        if (!ShouldSubmitOrder(desiredSide, ctx.Bb())) {
            if (ctx.Bb().outstandingOrder == desiredSide) {
                ctx.scratch.blockedDuplicate = true;
                ctx.scratch.holdReason = "DuplicateOutstandingOrderBlocked";
            } else {
                ctx.scratch.holdReason = "SubmitBlocked";
            }
            return HftPhase::Hold;
        }

        if (desiredSide == OrderSide::Buy) {
            return HftPhase::PlaceBuy;
        }

        return HftPhase::PlaceSell;
    }

    [[nodiscard]] HftPhase PlaceBuyFrame(FrameCtx& ctx)
    {
        const OrderSide previousSide = ctx.Bb().outstandingOrder;
        ctx.Bb().outstandingOrder = OrderSide::Buy;
        ctx.Bb().outstandingState = OutstandingState::Fresh;
        ctx.Bb().outstandingAgeTicks = 0;
        RecordOutstandingFlip(ctx.Bb(), previousSide, ctx.Bb().outstandingOrder);
        if (ctx.scratch.isReentryAttempt) {
            ctx.Bb().reentrySubmitCount += 1;
            ctx.Bb().lastReentryLatencyTicks = ctx.Bb().ticksSinceStaleCancel;
            ctx.Bb().awaitingReentryAfterStaleCancel = false;
            ctx.Bb().ticksSinceStaleCancel = 0;
            if (ctx.Bb().enableMinCommit) {
                ctx.Bb().minCommitTicksRemaining = ctx.Bb().minCommitTicks;
            }
        }
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
        const OrderSide previousSide = ctx.Bb().outstandingOrder;
        ctx.Bb().outstandingOrder = OrderSide::Sell;
        ctx.Bb().outstandingState = OutstandingState::Fresh;
        ctx.Bb().outstandingAgeTicks = 0;
        RecordOutstandingFlip(ctx.Bb(), previousSide, ctx.Bb().outstandingOrder);
        if (ctx.scratch.isReentryAttempt) {
            ctx.Bb().reentrySubmitCount += 1;
            ctx.Bb().lastReentryLatencyTicks = ctx.Bb().ticksSinceStaleCancel;
            ctx.Bb().awaitingReentryAfterStaleCancel = false;
            ctx.Bb().ticksSinceStaleCancel = 0;
            if (ctx.Bb().enableMinCommit) {
                ctx.Bb().minCommitTicksRemaining = ctx.Bb().minCommitTicks;
            }
        }
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
            case HftPhase::StaleCheck:
                phase = StaleCheckFrame(ctx);
                break;
            case HftPhase::CancelStaleOrder:
                phase = CancelStaleOrderFrame(ctx);
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
        outcome.outstandingStateAfter = state.outstandingState;
        outcome.blockedDuplicate = scratch.blockedDuplicate;
        outcome.canceledStale = scratch.canceledStale;

        if (scratch.canceledStale) {
            outcome.decision = DecisionKind::CancelOrder;
            outcome.reason = "CanceledStaleOutstandingOrder";
            return outcome;
        }

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
