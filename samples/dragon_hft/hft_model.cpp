#include "hft_model.h"

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
}
