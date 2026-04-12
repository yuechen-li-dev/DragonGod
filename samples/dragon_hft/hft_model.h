#pragma once

#include <string>
#include <vector>

namespace dragongod_samples::dragon_hft
{
    enum class OrderSide
    {
        None,
        Buy,
        Sell
    };

    enum class DecisionKind
    {
        Hold,
        SubmitBuy,
        SubmitSell
    };

    struct MarketEvent
    {
        int bestBid = 0;
        int bestAsk = 0;
        int signal = 0;

        [[nodiscard]] bool operator==(const MarketEvent& other) const = default;
    };

    struct HftState
    {
        int threshold = 5;
        int latestSignal = 0;
        OrderSide outstandingOrder = OrderSide::None;
        int submitCount = 0;
        int holdCount = 0;

        [[nodiscard]] bool operator==(const HftState& other) const = default;
    };

    struct HftActuation
    {
        DecisionKind kind = DecisionKind::Hold;
        int price = 0;
        std::string reason;

        [[nodiscard]] bool operator==(const HftActuation& other) const = default;
    };

    struct EventOutcome
    {
        DecisionKind decision = DecisionKind::Hold;
        OrderSide outstandingAfter = OrderSide::None;
        bool blockedDuplicate = false;
        std::string reason;

        [[nodiscard]] bool operator==(const EventOutcome& other) const = default;
    };

    struct HftRunOutput
    {
        HftState finalState{};
        std::vector<HftActuation> actuation;
        std::vector<EventOutcome> outcomes;
        std::vector<std::string> trace;

        [[nodiscard]] bool operator==(const HftRunOutput& other) const = default;
    };

    [[nodiscard]] bool IsActionableSignal(int signal, int threshold);
    [[nodiscard]] OrderSide DesiredOrderSide(int signal, int threshold);
    [[nodiscard]] bool ShouldSubmitOrder(OrderSide desiredSide, OrderSide outstandingOrder);
    [[nodiscard]] int PriceForOrder(const MarketEvent& event, OrderSide side);

    [[nodiscard]] HftRunOutput RunHftGoldenPath(const HftState& initialState, const std::vector<MarketEvent>& mailboxEvents);
}
