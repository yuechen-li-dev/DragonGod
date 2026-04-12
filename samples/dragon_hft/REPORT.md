# Dragon HFT benchmark report (M14c)

## 1) Purpose

This report documents a bounded M14c experiment for the Dragon HFT sample.

The question for this pass is:

> After stale cancel, does immediate re-entry create churn, and do explicit hysteresis / min-commit controls reduce that churn in a useful way?

This report does **not** claim profitability, exchange realism, production execution viability, or hardware replacement.

## 2) Scope of the sample

The measured code path is the bounded Dragon HFT sample as implemented through M14a–M14c:

* deterministic market-event ingestion
* bounded buy/sell/hold decision logic
* explicit outstanding-order lifecycle
* stale detection and stale cancel
* bounded re-entry discipline variants after stale cancel

The sample is **not**:

* a full trading system
* an exchange simulator
* an order book
* a profitability claim
* a production execution engine
* a low-latency performance claim

## 3) Experiment variants

This pass compares three bounded re-entry policies after stale cancel:

### A. `DragonHft_ReentryBaselineBench`

Immediate re-entry baseline.

If the signal remains actionable after stale cancel, the sample may re-submit again as soon as the ordinary bounded rules permit.

This is the lowest-discipline, most churn-prone comparison point.

### B. `DragonHft_ReentryHysteresisBench`

Hysteresis-gated re-entry.

After stale cancel, the signal must exceed the ordinary threshold by an additional hysteresis margin before re-entry is allowed.

This is intended to reduce borderline/noisy re-entry.

### C. `DragonHft_ReentryMinCommitBench`

Min-commit discipline.

After re-entry submit, the sample enforces a bounded commitment window before allowing another rapid stale-cancel / re-entry flip.

This is intended to reduce rapid order-state churn after re-entry.

## 4) Execution environment / run conditions

Observed run conditions for this report:

* build command:

  * `g++ -std=c++23 -Wall -Wextra -pedantic -DMARIONETTE_TEST_REPO_ROOT="/workspace/DragonGod" ... -o out/dragon_hft_tests`
* benchmark command:

  * `./out/dragon_hft_tests --bench DragonHft_`
* benchmark harness mode:

  * Marionette benchmark mode via `--bench`

Machine-specific details such as CPU model, frequency policy, pinning/isolation, and OS scheduler state were **not** captured in the benchmark output. This report does not infer them.

## 5) Results

Raw benchmark output is preserved in `samples/dragon_hft/bench-results.txt`.

| Benchmark                        | Iterations | Elapsed (ns) | Avg (ns) |
| -------------------------------- | ---------: | -----------: | -------: |
| DragonHft_ReentryBaselineBench   |     10,000 |  289,841,470 |   28,984 |
| DragonHft_ReentryHysteresisBench |     10,000 |  328,314,249 |   32,831 |
| DragonHft_ReentryMinCommitBench  |     10,000 |  333,326,257 |   33,332 |

## 6) Bounded interpretation

Bounded reading from this run:

* Immediate re-entry is the cheapest policy in raw benchmark time.
* Hysteresis adds a modest overhead relative to baseline.
* Min-commit also adds a modest overhead, slightly above hysteresis in this run.
* The overhead of the disciplined variants is real, but not extreme.

Approximate relative change versus baseline:

* Hysteresis: about **+13%**
* Min-commit: about **+15%**

This is consistent with the nature of the variants: both introduce additional control discipline, so they should cost somewhat more than immediate re-entry.

## 7) What this does and does not mean

This benchmark does **not** mean baseline immediate re-entry is automatically the best policy.

It only means baseline is the cheapest in raw runtime for this bounded sample.

The real evaluation of hysteresis and min-commit depends on the behavioral counters and tradeoffs captured elsewhere in the sample, such as:

* whether hysteresis meaningfully blocks noisy re-entry
* whether min-commit reduces rapid order-state flip-flop
* whether the extra discipline produces cleaner recovery behavior
* whether the extra delay is justified by reduced churn

So the useful interpretation is:

> Better stale-cancel re-entry discipline is not free, but the cost increase in this bounded sample is moderate rather than catastrophic.

That is the real result of this benchmark lane.

## 8) Connection to the larger Dragon HFT question

The Dragon HFT sample is not trying to prove “fast trading” or “profitability.”

The more interesting architectural question is:

* does the expensive/problematic part come from baseline decision-making?
* or from unstable recovery and re-entry behavior?

This benchmark suggests that adding bounded discipline to the stale-cancel recovery path costs something, but not enough to make such discipline obviously impractical in this sample.

That makes the next question a behavioral one, not just a timing one:

* do hysteresis and min-commit reduce churn enough to justify their overhead?

## 9) Limitations

This report has explicit limitations:

* bounded sample only
* no exchange simulator
* no order book
* no fills / partial fills / reject realism in this pass
* no profitability or PnL interpretation
* single benchmark capture in one environment
* no captured machine metadata beyond visible benchmark output

Because of these limits, this report should be treated as a small experiment artifact, not a production trading-performance claim.
