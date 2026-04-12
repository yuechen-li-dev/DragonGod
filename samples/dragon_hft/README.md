# Dragon HFT stale-cancel re-entry discipline sample (M14d)

This sample is a bounded market-reaction / order-lifecycle control-loop experiment.

It is **not** a full trading system.

It is **not** an exchange simulator.

It is **not** a profitability claim.

The purpose of this M14d pass is to test a bounded stale-cancel recovery question:

> in an oscillating borderline signal regime, does immediate re-entry churn, and do explicit hysteresis / min-commit controls reduce that churn?

## Bounded behavior covered

For each inbound market event (mailbox input), the sample runs explicit frames:

1. `IngestMarketEventFrame`
2. `StaleCheckFrame`
3. one of:
   - `CancelStaleOrderFrame`
   - `DecisionFrame` then:
     - `PlaceBuyFrame`
     - `PlaceSellFrame`
     - `HoldFrame`

The bounded baseline signal rules are:

- signal `>= threshold` => buy intent
- signal `<= -threshold` => sell intent
- otherwise hold

Outstanding-order lifecycle is explicit:

- active orders age by one tick per market event
- age `<= staleTickThreshold` => outstanding is fresh
- age `> staleTickThreshold` => outstanding is stale and emits `CancelOrder`
- stale-cancel events stop after cancel (no same-event submit/cancel contradiction)
- after cancel clears outstanding state, later events may submit again

M14c/M14d re-entry discipline variants:

- **Variant A: Immediate re-entry baseline**  
  stale-cancel clears state; the next actionable event may submit immediately.
- **Variant B: Hysteresis-gated re-entry**  
  while waiting for stale-cancel re-entry, signal must satisfy `abs(signal) >= threshold + reentryHysteresisMargin`.
- **Variant C: Min-commit discipline**  
  after a re-entry submit, stale cancel is suppressed for `minCommitTicks` events (bounded commitment window).

All controls are sample-local and explicit in `HftState`.

## M14d oscillation scenario

M14d adds one explicit bounded oscillation mailbox (`BuildReentryOscillationMailbox`) for
stale-cancel recovery comparison:

1. strong actionable signal to create initial submit and stale cancel
2. post-cancel boundary hover: `+5` (barely actionable), `+4` (not actionable), `+5` again
3. later stronger confirmation: `+8`, `+8`

This sequence is deterministic and intentionally churn-prone:

- baseline is tempted to re-enter at boundary points
- hysteresis can block borderline re-entry and wait for stronger confirmation
- min-commit can suppress repeated stale-cancel attempts after re-entry during the oscillation window

Duplicate suppression remains explicit:

- when the same-side order is already outstanding and still fresh, hold and emit no submit actuation.

## Sample-local counters used for interpretation

In addition to M14b counters, M14c/M14d records:

- `reentrySubmitCount`
- `reentryBlockedByHysteresisCount`
- `staleCancelBlockedByMinCommitCount`
- `orderStateFlipCount`
- `lastReentryLatencyTicks`

## Files

- `hft_model.h` / `hft_model.cpp`: bounded market event, lifecycle state, actuation models, and helper logic.
- `hft_nodes.cpp`: explicit ingest -> stale-check -> cancel/decide -> place/hold frame path and lifecycle runner.
- `hft_model_tests.cpp`: model/helper tests (signal mapping, age/stale helpers, submit gating, and oscillation scenario builders).
- `hft_nodes_tests.cpp`: frame-path behavior tests (fresh hold, stale cancel buy/sell, duplicate suppression).
- `hft_runtime_tests.cpp`: end-to-end lifecycle tests, deterministic replay, and M14c/M14d re-entry-discipline tests.
- `hft_benchmarks_tests.cpp`: stale-cancel recovery benchmark variants over the explicit oscillation sequence.
- `bench-results.txt`: captured benchmark output for this pass.

## Build and run sample tests

From repository root:

```bash
mkdir -p out

g++ -std=c++23 -Wall -Wextra -pedantic \
  -DMARIONETTE_TEST_REPO_ROOT="/workspace/DragonGod" \
  tests/Marionette/test_harness.cpp \
  tests/Marionette/test_doom.cpp \
  tests/Marionette/test_main.cpp \
  samples/dragon_hft/hft_model.cpp \
  samples/dragon_hft/hft_nodes.cpp \
  samples/dragon_hft/hft_model_tests.cpp \
  samples/dragon_hft/hft_nodes_tests.cpp \
  samples/dragon_hft/hft_runtime_tests.cpp \
  samples/dragon_hft/hft_benchmarks_tests.cpp \
  -o out/dragon_hft_tests

./out/dragon_hft_tests
```

## Run sample-local benchmarks

```bash
./out/dragon_hft_tests --bench DragonHft_
```

Benchmarks in this pass:

- `DragonHft_ReentryOscillationBaselineBench`
- `DragonHft_ReentryOscillationHysteresisBench`
- `DragonHft_ReentryOscillationMinCommitBench`

## Bounded interpretation of this pass

- Hysteresis lowers borderline re-entry and order-state flips by requiring stronger post-cancel signal, but increases re-entry latency.
- Min-commit lowers stale-cancel/re-entry flip pressure during the commitment window, but can delay stale cancellation and may finish the bounded window with no active order.
- This is a bounded control-loop behavior experiment only; it does not claim trading profitability or exchange realism.
