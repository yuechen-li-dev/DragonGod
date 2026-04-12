#include "../../tests/Marionette/test_harness.h"
#include "hft_model.h"

namespace
{
    using namespace dragongod_samples::dragon_hft;

    [[nodiscard]] HftState RuntimeBaseState()
    {
        HftState state{};
        state.threshold = 5;
        return state;
    }
}

FACT(M14a_Runtime_GoldenPath_CoversHoldBuySellAndDuplicate)
{
    const std::vector<MarketEvent> mailbox{
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 1 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = -7 }
    };

    const HftRunOutput run = RunHftGoldenPath(RuntimeBaseState(), mailbox);

    ASSERT_EQUAL(2, static_cast<int>(run.actuation.size()), "golden path should only submit one buy then one sell");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::SubmitBuy), static_cast<int>(run.actuation[0].kind), "second event should submit buy");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::SubmitSell), static_cast<int>(run.actuation[1].kind), "final event should submit sell");
    ASSERT_EQUAL(2, run.finalState.submitCount, "two submits should be recorded in bounded state");
    ASSERT_EQUAL(2, run.finalState.holdCount, "non-edge and duplicate cases should both hold");
    ASSERT_TRUE(run.outcomes[2].blockedDuplicate, "third event should be duplicate-suppressed");
}

FACT(M14a_Runtime_Replay_IsDeterministic)
{
    const std::vector<MarketEvent> mailbox{
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 6 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 6 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = -6 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 0 }
    };

    const HftRunOutput runA = RunHftGoldenPath(RuntimeBaseState(), mailbox);
    const HftRunOutput runB = RunHftGoldenPath(RuntimeBaseState(), mailbox);

    ASSERT_TRUE(runA == runB, "same mailbox inputs should replay to identical bounded outputs");
}
