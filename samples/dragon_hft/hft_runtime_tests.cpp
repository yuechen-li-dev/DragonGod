#include "../../tests/Marionette/test_harness.h"
#include "hft_model.h"

namespace
{
    using namespace dragongod_samples::dragon_hft;

    [[nodiscard]] HftState RuntimeBaseState()
    {
        HftState state{};
        state.threshold = 5;
        state.staleTickThreshold = 2;
        return state;
    }
}

FACT(M14b_Runtime_Lifecycle_CoversSubmitHoldCancelAndResubmit)
{
    const std::vector<MarketEvent> mailbox{
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 8 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 8 }
    };

    const HftRunOutput run = RunHftGoldenPath(RuntimeBaseState(), mailbox);

    ASSERT_EQUAL(3, static_cast<int>(run.actuation.size()), "lifecycle should submit, cancel stale, then resubmit");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::SubmitBuy), static_cast<int>(run.actuation[0].kind), "first event should submit buy");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::CancelOrder), static_cast<int>(run.actuation[1].kind), "fourth event should cancel stale buy");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::SubmitBuy), static_cast<int>(run.actuation[2].kind), "fifth event can submit buy after cancel cleared state");
    ASSERT_EQUAL(2, run.finalState.submitCount, "two submits should be recorded");
    ASSERT_EQUAL(1, run.finalState.cancelCount, "one stale cancel should be recorded");
    ASSERT_EQUAL(2, run.finalState.holdCount, "duplicate holds should be recorded before stale cancel");
}

FACT(M14b_Runtime_StaleCancelPath_DoesNotEmitContradictoryActions)
{
    HftState initial = RuntimeBaseState();
    initial.outstandingOrder = OrderSide::Sell;
    initial.outstandingAgeTicks = 2;
    initial.outstandingState = OutstandingState::Fresh;

    const std::vector<MarketEvent> mailbox{
        MarketEvent{ .bestBid = 98, .bestAsk = 99, .signal = -7 }
    };

    const HftRunOutput run = RunHftGoldenPath(initial, mailbox);

    ASSERT_EQUAL(1, static_cast<int>(run.actuation.size()), "stale event should emit exactly one action");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::CancelOrder), static_cast<int>(run.actuation[0].kind), "stale event should only cancel");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::CancelOrder), static_cast<int>(run.outcomes[0].decision), "outcome should only report cancel");
}

FACT(M14b_Runtime_Replay_IsDeterministic)
{
    const std::vector<MarketEvent> mailbox{
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 6 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 6 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 6 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = -6 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = -6 }
    };

    const HftRunOutput runA = RunHftGoldenPath(RuntimeBaseState(), mailbox);
    const HftRunOutput runB = RunHftGoldenPath(RuntimeBaseState(), mailbox);

    ASSERT_TRUE(runA == runB, "same mailbox inputs should replay to identical bounded outputs");
}

FACT(M14c_Runtime_ReentryBaseline_ReentersImmediatelyAfterStaleCancel)
{
    const std::vector<MarketEvent> mailbox{
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 6 }
    };

    const HftRunOutput run = RunHftGoldenPath(RuntimeBaseState(), mailbox);

    ASSERT_EQUAL(3, static_cast<int>(run.actuation.size()), "baseline should submit, cancel stale, and re-submit on next actionable event");
    ASSERT_EQUAL(1, run.finalState.reentrySubmitCount, "reentry submit should be counted");
    ASSERT_EQUAL(1, run.finalState.lastReentryLatencyTicks, "immediate reentry should take one event after stale cancel");
    ASSERT_EQUAL(0, run.finalState.reentryBlockedByHysteresisCount, "baseline should not use hysteresis gating");
}

FACT(M14c_Runtime_ReentryHysteresis_BlocksBorderlineReentryUntilStrongerSignal)
{
    HftState initial = RuntimeBaseState();
    initial.enableReentryHysteresis = true;
    initial.reentryHysteresisMargin = 2;

    const std::vector<MarketEvent> mailbox{
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 6 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 7 }
    };

    const HftRunOutput run = RunHftGoldenPath(initial, mailbox);

    ASSERT_EQUAL(3, static_cast<int>(run.actuation.size()), "hysteresis path should still eventually reenter once stronger signal arrives");
    ASSERT_EQUAL(static_cast<int>(DecisionKind::SubmitBuy), static_cast<int>(run.actuation.back().kind), "last action should be delayed reentry submit");
    ASSERT_EQUAL(1, run.finalState.reentryBlockedByHysteresisCount, "borderline reentry should be explicitly blocked once");
    ASSERT_EQUAL(2, run.finalState.lastReentryLatencyTicks, "hysteresis should delay reentry by one extra event");
}

FACT(M14c_Runtime_MinCommit_BlocksImmediateSecondStaleCancelFlip)
{
    HftState initial = RuntimeBaseState();
    initial.enableMinCommit = true;
    initial.minCommitTicks = 2;
    initial.staleTickThreshold = 0;

    const std::vector<MarketEvent> mailbox{
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 100, .bestAsk = 101, .signal = 8 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 8 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 8 },
        MarketEvent{ .bestBid = 99, .bestAsk = 100, .signal = 8 }
    };

    const HftRunOutput run = RunHftGoldenPath(initial, mailbox);

    ASSERT_EQUAL(5, static_cast<int>(run.actuation.size()), "min-commit path should submit-cancel-submit-hold-hold-cancel-submit pattern");
    ASSERT_EQUAL(2, run.finalState.cancelCount, "second stale cancel should still happen after min-commit window expires");
    ASSERT_TRUE(run.finalState.staleCancelBlockedByMinCommitCount >= 1, "min-commit should block at least one stale cancel attempt");
    ASSERT_TRUE(run.finalState.orderStateFlipCount >= 4, "order-state flip counter should track cancel and submit transitions");
}
