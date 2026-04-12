# Dragon HFT benchmark report (M14d)

## 1) Purpose

This report documents a bounded M14d experiment for the Dragon HFT sample.

The question for this pass is:

> In an oscillating borderline signal regime after stale cancel, do hysteresis and min-commit reduce re-entry churn enough to justify their overhead?

This report does **not** claim profitability, exchange realism, production execution viability, or hardware replacement.

## 2) Scope of the sample

Measured path is still the bounded Dragon HFT sample:

* deterministic market-event ingestion
* bounded buy/sell/hold decision logic
* explicit outstanding-order lifecycle
* stale detection and stale cancel
* bounded re-entry discipline variants

Still out of scope:

* full trading system behavior
* exchange simulator realism
* order-book modeling
* fills / partial fills / rejects
* PnL or profitability claims

## 3) Oscillation scenario used in M14d

M14d uses one fixed mailbox sequence (`BuildReentryOscillationMailbox`) designed to tempt bad re-entry:

1. `+8, +8, +8` to create submit and stale cancel
2. then boundary hover: `+5, +4, +5`
3. then stronger follow-through: `+8, +8`

All prices are fixed (`bestBid=100`, `bestAsk=101`) to isolate control-loop behavior.

## 4) Variants compared

### A. `DragonHft_ReentryOscillationBaselineBench`

Immediate re-entry baseline.

### B. `DragonHft_ReentryOscillationHysteresisBench`

Re-entry requires `abs(signal) >= threshold + reentryHysteresisMargin`.

### C. `DragonHft_ReentryOscillationMinCommitBench`

After re-entry submit, stale cancel is suppressed for a bounded commitment window.

## 5) Counters used for interpretation

The M14d runtime comparison focuses on sample-local counters/facts:

* `reentrySubmitCount`
* `reentryBlockedByHysteresisCount`
* `staleCancelBlockedByMinCommitCount`
* `orderStateFlipCount`
* `lastReentryLatencyTicks`
* `cancelCount`
* final outstanding state

## 6) Runtime comparison facts from the oscillation scenario

Deterministic runtime test (`M14d_Runtime_ReentryOscillationScenario_ExposesChurnTradeoffsAcrossVariants`) shows:

* **Baseline**: eager re-entry churn (`reentrySubmitCount=2`, `cancelCount=2`, `lastReentryLatencyTicks=1`).
* **Hysteresis**: fewer re-entries (`reentrySubmitCount=1`) and fewer flips, but delayed re-entry (`lastReentryLatencyTicks=4`).
* **Min-commit**: repeated stale-cancel attempts blocked (`staleCancelBlockedByMinCommitCount>=2`) and fewer flips than baseline in this bounded window, with delayed stale-cancel progression.

## 7) Benchmark run conditions and output

Run commands:

* build:
  * `g++ -std=c++23 -Wall -Wextra -pedantic -DMARIONETTE_TEST_REPO_ROOT="/workspace/DragonGod" ... -o out/dragon_hft_tests`
* benchmark:
  * `./out/dragon_hft_tests --bench DragonHft_`

Raw output is preserved in `samples/dragon_hft/bench-results.txt`.

| Benchmark                                   | Iterations | Elapsed (ns) | Avg (ns) |
| ------------------------------------------- | ---------: | -----------: | -------: |
| DragonHft_ReentryOscillationBaselineBench   |     10,000 |  248,339,017 |   24,833 |
| DragonHft_ReentryOscillationHysteresisBench |     10,000 |  236,319,157 |   23,631 |
| DragonHft_ReentryOscillationMinCommitBench  |     10,000 |  240,505,551 |   24,050 |

## 8) Bounded interpretation

In this churn-prone bounded sequence, the key reading comes from churn counters first, not raw timing:

* Hysteresis and min-commit both reduce bounded churn indicators versus immediate re-entry baseline.
* Hysteresis reduces borderline re-entry but pays additional latency before re-entry confirmation.
* Min-commit blocks repeated stale-cancel attempts inside the commitment window, reducing flip pressure in-window while delaying cancellation progression.

The benchmark times are close and scenario-sensitive; they should not be over-interpreted as a production speed claim.

## 9) Limitations

* single bounded deterministic sequence
* no exchange realism
* no execution realism (fills/rejects)
* no PnL conclusions
* no machine pinning/isolation metadata in output

This remains a bounded experiment artifact for stale-cancel re-entry discipline tradeoffs.
