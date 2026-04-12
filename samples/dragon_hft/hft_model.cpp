#include "hft_model.h"

#include <cstdlib>

namespace dragongod_samples::dragon_hft
{
    [[nodiscard]] bool IsActionableSignal(const int signal, const int threshold)
    {
        return signal >= threshold || signal <= -threshold;
    }

    [[nodiscard]] OrderSide DesiredOrderSide(const int signal, const int threshold)
    {
        if (signal >= threshold) {
            return OrderSide::Buy;
        }

        if (signal <= -threshold) {
            return OrderSide::Sell;
        }

        return OrderSide::None;
    }

    [[nodiscard]] bool IsOutstandingActive(const HftState& state)
    {
        return state.outstandingOrder != OrderSide::None;
    }

    void AdvanceOutstandingAge(HftState& state)
    {
        if (!IsOutstandingActive(state)) {
            state.outstandingAgeTicks = 0;
            state.outstandingState = OutstandingState::None;
            return;
        }

        state.outstandingAgeTicks += 1;
        if (state.outstandingAgeTicks > state.staleTickThreshold) {
            state.outstandingState = OutstandingState::Stale;
            return;
        }

        state.outstandingState = OutstandingState::Fresh;
    }

    [[nodiscard]] bool IsOutstandingStale(const HftState& state)
    {
        return IsOutstandingActive(state) && state.outstandingAgeTicks > state.staleTickThreshold;
    }

    [[nodiscard]] bool ShouldCancelStaleOrder(const HftState& state)
    {
        return IsOutstandingStale(state);
    }

    [[nodiscard]] bool ShouldSubmitOrder(const OrderSide desiredSide, const HftState& state)
    {
        if (desiredSide == OrderSide::None) {
            return false;
        }

        if (ShouldCancelStaleOrder(state)) {
            return false;
        }

        return desiredSide != state.outstandingOrder;
    }

    [[nodiscard]] bool ReentryHysteresisSatisfied(const int signal, const HftState& state)
    {
        if (!state.enableReentryHysteresis) {
            return true;
        }

        const int absoluteSignal = std::abs(signal);
        const int requiredSignal = state.threshold + state.reentryHysteresisMargin;
        return absoluteSignal >= requiredSignal;
    }

    [[nodiscard]] int PriceForOrder(const MarketEvent& event, const OrderSide side)
    {
        if (side == OrderSide::Buy) {
            return event.bestAsk;
        }

        if (side == OrderSide::Sell) {
            return event.bestBid;
        }

        return 0;
    }

    [[nodiscard]] HftState BuildReentryOscillationBaselineState()
    {
        HftState state{};
        state.threshold = 5;
        state.staleTickThreshold = 1;
        return state;
    }

    [[nodiscard]] HftState BuildReentryOscillationHysteresisState()
    {
        HftState state = BuildReentryOscillationBaselineState();
        state.enableReentryHysteresis = true;
        state.reentryHysteresisMargin = 2;
        return state;
    }

    [[nodiscard]] HftState BuildReentryOscillationMinCommitState()
    {
        HftState state = BuildReentryOscillationBaselineState();
        state.enableMinCommit = true;
        state.minCommitTicks = 4;
        return state;
    }

    [[nodiscard]] std::vector<MarketEvent> BuildReentryOscillationMailbox()
    {
        return {
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 5 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 4 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 5 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
            MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 }
        };
    }
}
