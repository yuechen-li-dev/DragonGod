#include "../../tests/Marionette/test_harness.h"
#include "hft_model.h"

namespace
{
    using namespace dragongod_samples::dragon_hft;

    [[nodiscard]] HftState BaseState()
    {
        HftState state{};
        state.threshold = 5;
        state.staleTickThreshold = 2;
        return state;
    }
}

FACT(M14b_Nodes_NonActionableSignal_HoldsWithoutSubmitActuation)
{
    const std::vector<MarketEvent> mailbox{ MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 2 } };
    const HftRunOutput run = RunHftGoldenPath(BaseState(), mailbox);

    ASSERT_EQUAL(0, static_cast<int>(run.actuation.size()), "hold branch should not emit submit actuation");
    ASSERT_EQUAL(1, run.finalState.holdCount, "hold branch should increment hold counter");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::Hold), static_cast<int>(run.outcomes[0].decision), "outcome should encode hold decision");
    ASSERT_EQUAL(std::string("NoEdge"), run.outcomes[0].reason, "hold reason should be explicit");
}

FACT(M14b_Nodes_FreshOutstandingOrder_DoesNotCancel)
{
    HftState initial = BaseState();
    initial.outstandingOrder = OrderSide::Buy;

    const std::vector<MarketEvent> mailbox{ MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 1 } };
    const HftRunOutput run = RunHftGoldenPath(initial, mailbox);

    ASSERT_EQUAL(0, static_cast<int>(run.actuation.size()), "fresh outstanding order should not cancel");
    ASSERT_EQUAL(static_cast<int>(OrderSide::Buy), static_cast<int>(run.finalState.outstandingOrder), "fresh order should remain outstanding");
    ASSERT_EQUAL(static_cast<int>(OutstandingState::Fresh), static_cast<int>(run.finalState.outstandingState), "fresh order should keep fresh state");
    ASSERT_EQUAL(1, run.finalState.outstandingAgeTicks, "fresh order should age by one tick");
}

FACT(M14b_Nodes_StaleOutstandingBuy_CancelsDeterministically)
{
    HftState initial = BaseState();
    initial.outstandingOrder = OrderSide::Buy;
    initial.outstandingAgeTicks = 2;
    initial.outstandingState = OutstandingState::Fresh;

    const std::vector<MarketEvent> mailbox{ MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 7 } };
    const HftRunOutput run = RunHftGoldenPath(initial, mailbox);

    ASSERT_EQUAL(1, static_cast<int>(run.actuation.size()), "stale path should emit one cancel actuation");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::CancelOrder), static_cast<int>(run.actuation[0].kind), "stale buy should cancel");
    ASSERT_EQUAL(1, run.finalState.cancelCount, "cancel should increment cancel counter");
    ASSERT_EQUAL(static_cast<int>(OrderSide::None), static_cast<int>(run.finalState.outstandingOrder), "cancel should clear outstanding side");
    ASSERT_TRUE(run.outcomes[0].canceledStale, "outcome should record stale cancel path");
}

FACT(M14b_Nodes_StaleOutstandingSell_CancelsDeterministically)
{
    HftState initial = BaseState();
    initial.outstandingOrder = OrderSide::Sell;
    initial.outstandingAgeTicks = 2;
    initial.outstandingState = OutstandingState::Fresh;

    const std::vector<MarketEvent> mailbox{ MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = -7 } };
    const HftRunOutput run = RunHftGoldenPath(initial, mailbox);

    ASSERT_EQUAL(1, static_cast<int>(run.actuation.size()), "stale path should emit one cancel actuation");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::CancelOrder), static_cast<int>(run.actuation[0].kind), "stale sell should cancel");
    ASSERT_EQUAL(1, run.finalState.cancelCount, "cancel should increment cancel counter");
    ASSERT_EQUAL(static_cast<int>(OrderSide::None), static_cast<int>(run.finalState.outstandingOrder), "cancel should clear outstanding side");
}

FACT(M14b_Nodes_ActionablePositiveSignal_SubmitsBuyDeterministically)
{
    const std::vector<MarketEvent> mailbox{ MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 7 } };
    const HftRunOutput run = RunHftGoldenPath(BaseState(), mailbox);

    ASSERT_EQUAL(1, static_cast<int>(run.actuation.size()), "buy branch should emit one submit actuation");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::SubmitBuy), static_cast<int>(run.actuation[0].kind), "positive edge should submit buy");
    ASSERT_EQUAL(101, run.actuation[0].price, "buy submit price should use best ask");
    ASSERT_EQUAL(static_cast<int>(OrderSide::Buy), static_cast<int>(run.finalState.outstandingOrder), "buy should become outstanding state");
    ASSERT_EQUAL(1, run.finalState.submitCount, "buy submit should increment submit counter");
}

FACT(M14b_Nodes_ActionableNegativeSignal_SubmitsSellDeterministically)
{
    const std::vector<MarketEvent> mailbox{ MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = -8 } };
    const HftRunOutput run = RunHftGoldenPath(BaseState(), mailbox);

    ASSERT_EQUAL(1, static_cast<int>(run.actuation.size()), "sell branch should emit one submit actuation");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::SubmitSell), static_cast<int>(run.actuation[0].kind), "negative edge should submit sell");
    ASSERT_EQUAL(99, run.actuation[0].price, "sell submit price should use best bid");
    ASSERT_EQUAL(static_cast<int>(OrderSide::Sell), static_cast<int>(run.finalState.outstandingOrder), "sell should become outstanding state");
}

FACT(M14b_Nodes_MatchingOutstandingOrder_BlocksDuplicateSubmit)
{
    HftState initial = BaseState();
    initial.outstandingOrder = OrderSide::Buy;

    const std::vector<MarketEvent> mailbox{ MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 9 } };
    const HftRunOutput run = RunHftGoldenPath(initial, mailbox);

    ASSERT_EQUAL(0, static_cast<int>(run.actuation.size()), "duplicate outstanding side should suppress submit actuation");
    ASSERT_EQUAL(0, run.finalState.submitCount, "duplicate suppression should not change submit counter");
    ASSERT_TRUE(run.outcomes[0].blockedDuplicate, "outcome should explicitly record duplicate suppression");
    ASSERT_EQUAL(std::string("DuplicateOutstandingOrderBlocked"), run.outcomes[0].reason, "duplicate hold reason should be explicit");
}
