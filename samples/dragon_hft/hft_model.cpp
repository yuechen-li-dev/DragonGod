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

    [[nodiscard]] bool ShouldSubmitOrder(const OrderSide desiredSide, const OrderSide outstandingOrder)
    {
        if (desiredSide == OrderSide::None) {
            return false;
        }

        return desiredSide != outstandingOrder;
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
