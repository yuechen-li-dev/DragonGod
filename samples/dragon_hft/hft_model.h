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
        SubmitSell,
        CancelOrder
    };

    enum class OutstandingState
    {
        None,
        Fresh,
        Stale
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
        int staleTickThreshold = 2;
        bool enableReentryHysteresis = false;
        int reentryHysteresisMargin = 0;
        bool enableMinCommit = false;
        int minCommitTicks = 0;
        int latestSignal = 0;
        OrderSide outstandingOrder = OrderSide::None;
        OutstandingState outstandingState = OutstandingState::None;
        int outstandingAgeTicks = 0;
        int minCommitTicksRemaining = 0;
        bool awaitingReentryAfterStaleCancel = false;
        int ticksSinceStaleCancel = 0;
        int lastReentryLatencyTicks = -1;
        int submitCount = 0;
        int reentrySubmitCount = 0;
        int cancelCount = 0;
        int staleCancelBlockedByMinCommitCount = 0;
        int reentryBlockedByHysteresisCount = 0;
        int orderStateFlipCount = 0;
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
        OutstandingState outstandingStateAfter = OutstandingState::None;
        bool blockedDuplicate = false;
        bool canceledStale = false;
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
    [[nodiscard]] bool IsOutstandingActive(const HftState& state);
    void AdvanceOutstandingAge(HftState& state);
    [[nodiscard]] bool IsOutstandingStale(const HftState& state);
    [[nodiscard]] bool ShouldCancelStaleOrder(const HftState& state);
    [[nodiscard]] bool ShouldSubmitOrder(OrderSide desiredSide, const HftState& state);
    [[nodiscard]] bool ReentryHysteresisSatisfied(int signal, const HftState& state);
    [[nodiscard]] int PriceForOrder(const MarketEvent& event, OrderSide side);

    [[nodiscard]] HftRunOutput RunHftGoldenPath(const HftState& initialState, const std::vector<MarketEvent>& mailboxEvents);
}
