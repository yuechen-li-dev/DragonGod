#include "../../tests/Marionette/test_harness.h"
#include "hft_model.h"

namespace
{
    using namespace dragongod_samples::dragon_hft;
}

FACT(M14a_Model_IsActionableSignal_UsesSymmetricThreshold)
{
    ASSERT_FALSE(IsActionableSignal(4, 5), "below threshold should not be actionable");
    ASSERT_TRUE(IsActionableSignal(5, 5), "positive threshold boundary should be actionable");
    ASSERT_TRUE(IsActionableSignal(-5, 5), "negative threshold boundary should be actionable");
}

FACT(M14a_Model_DesiredOrderSide_MapsSignalToBuySellOrNone)
{
    ASSERT_EQUAL(static_cast<int>(OrderSide::Buy), static_cast<int>(DesiredOrderSide(8, 5)), "positive actionable signal should map to buy");
    ASSERT_EQUAL(static_cast<int>(OrderSide::Sell), static_cast<int>(DesiredOrderSide(-8, 5)), "negative actionable signal should map to sell");
    ASSERT_EQUAL(static_cast<int>(OrderSide::None), static_cast<int>(DesiredOrderSide(0, 5)), "neutral signal should map to hold");
}

FACT(M14a_Model_ShouldSubmitOrder_BlocksDuplicatesAndNeutral)
{
    ASSERT_FALSE(ShouldSubmitOrder(OrderSide::None, OrderSide::None), "neutral desired side should never submit");
    ASSERT_FALSE(ShouldSubmitOrder(OrderSide::Buy, OrderSide::Buy), "matching outstanding buy should block duplicate submit");
    ASSERT_FALSE(ShouldSubmitOrder(OrderSide::Sell, OrderSide::Sell), "matching outstanding sell should block duplicate submit");
    ASSERT_TRUE(ShouldSubmitOrder(OrderSide::Buy, OrderSide::Sell), "opposite outstanding side should allow bounded submit");
}

FACT(M14a_Model_PriceForOrder_UsesExpectedTopOfBookSide)
{
    const MarketEvent event{ .bestBid = 101, .bestAsk = 103, .signal = 0 };

    ASSERT_EQUAL(103, PriceForOrder(event, OrderSide::Buy), "buy submit should use best ask");
    ASSERT_EQUAL(101, PriceForOrder(event, OrderSide::Sell), "sell submit should use best bid");
    ASSERT_EQUAL(0, PriceForOrder(event, OrderSide::None), "hold action should not resolve a price");
}
