#include "../../tests/Marionette/test_harness.h"
#include "hft_model.h"

namespace
{
    using namespace dragongod_samples::dragon_hft;

    [[nodiscard]] HftState ModelBaseState()
    {
        HftState state{};
        state.threshold = 5;
        state.staleTickThreshold = 2;
        return state;
    }
}

FACT(M14b_Model_IsActionableSignal_UsesSymmetricThreshold)
{
    ASSERT_FALSE(IsActionableSignal(4, 5), "below threshold should not be actionable");
    ASSERT_TRUE(IsActionableSignal(5, 5), "positive threshold boundary should be actionable");
    ASSERT_TRUE(IsActionableSignal(-5, 5), "negative threshold boundary should be actionable");
}

FACT(M14b_Model_DesiredOrderSide_MapsSignalToBuySellOrNone)
{
    ASSERT_EQUAL(static_cast<int>(OrderSide::Buy), static_cast<int>(DesiredOrderSide(8, 5)), "positive actionable signal should map to buy");
    ASSERT_EQUAL(static_cast<int>(OrderSide::Sell), static_cast<int>(DesiredOrderSide(-8, 5)), "negative actionable signal should map to sell");
    ASSERT_EQUAL(static_cast<int>(OrderSide::None), static_cast<int>(DesiredOrderSide(0, 5)), "neutral signal should map to hold");
}

FACT(M14b_Model_AdvanceOutstandingAge_TracksFreshAndStale)
{
    HftState state = ModelBaseState();
    state.outstandingOrder = OrderSide::Buy;

    AdvanceOutstandingAge(state);
    ASSERT_EQUAL(1, state.outstandingAgeTicks, "first tick should age outstanding order");
    ASSERT_EQUAL(static_cast<int>(OutstandingState::Fresh), static_cast<int>(state.outstandingState), "below threshold should remain fresh");
    ASSERT_FALSE(IsOutstandingStale(state), "age below threshold is not stale");

    AdvanceOutstandingAge(state);
    ASSERT_EQUAL(2, state.outstandingAgeTicks, "threshold boundary should still be fresh");
    ASSERT_FALSE(IsOutstandingStale(state), "threshold boundary is not stale");

    AdvanceOutstandingAge(state);
    ASSERT_EQUAL(3, state.outstandingAgeTicks, "age should continue increasing while active");
    ASSERT_EQUAL(static_cast<int>(OutstandingState::Stale), static_cast<int>(state.outstandingState), "age above threshold should become stale");
    ASSERT_TRUE(IsOutstandingStale(state), "age above threshold should be stale");
}

FACT(M14b_Model_ShouldSubmitOrder_BlocksNeutralDuplicateAndStale)
{
    HftState state = ModelBaseState();

    ASSERT_FALSE(ShouldSubmitOrder(OrderSide::None, state), "neutral desired side should never submit");

    state.outstandingOrder = OrderSide::Buy;
    ASSERT_FALSE(ShouldSubmitOrder(OrderSide::Buy, state), "matching outstanding buy should block duplicate submit");

    state.outstandingOrder = OrderSide::Sell;
    ASSERT_TRUE(ShouldSubmitOrder(OrderSide::Buy, state), "opposite outstanding side may submit while fresh");

    state.outstandingAgeTicks = 3;
    state.outstandingState = OutstandingState::Stale;
    ASSERT_TRUE(ShouldCancelStaleOrder(state), "stale outstanding order should request cancel path");
    ASSERT_FALSE(ShouldSubmitOrder(OrderSide::Buy, state), "stale outstanding order should block submits until canceled");
}

FACT(M14b_Model_PriceForOrder_UsesExpectedTopOfBookSide)
{
    const MarketEvent event{ .bestBid = 101, .bestAsk = 103, .signal = 0 };

    ASSERT_EQUAL(103, PriceForOrder(event, OrderSide::Buy), "buy submit should use best ask");
    ASSERT_EQUAL(101, PriceForOrder(event, OrderSide::Sell), "sell submit should use best bid");
    ASSERT_EQUAL(0, PriceForOrder(event, OrderSide::None), "hold action should not resolve a price");
}

FACT(M14c_Model_ReentryHysteresisSatisfied_RequiresExtraMarginWhenEnabled)
{
    HftState state = ModelBaseState();
    state.enableReentryHysteresis = true;
    state.reentryHysteresisMargin = 2;

    ASSERT_FALSE(ReentryHysteresisSatisfied(6, state), "signal below threshold plus margin should not pass reentry hysteresis");
    ASSERT_TRUE(ReentryHysteresisSatisfied(7, state), "signal at threshold plus margin should pass reentry hysteresis");
    ASSERT_TRUE(ReentryHysteresisSatisfied(-7, state), "negative boundary should be symmetric");

    state.enableReentryHysteresis = false;
    ASSERT_TRUE(ReentryHysteresisSatisfied(6, state), "disabled hysteresis should allow ordinary actionable reentry checks");
}
