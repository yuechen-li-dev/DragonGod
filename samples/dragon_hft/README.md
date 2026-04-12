# Dragon HFT order lifecycle / stale recovery sample (M14b)

This sample is a bounded market-reaction / order-lifecycle control-loop experiment.

It is **not** a full trading system.

It is **not** an exchange simulator.

It is **not** a profitability claim.

The purpose of this M14b pass is to prove that DragonGod-style explicit frame logic can express deterministic outstanding-order aging and stale-order recovery without polluting `src/DragonGod/`.

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

The bounded rules are:

- signal `>= threshold` => buy intent
- signal `<= -threshold` => sell intent
- otherwise hold

Outstanding-order lifecycle is explicit:

- active orders age by one tick per market event
- age `<= staleTickThreshold` => outstanding is fresh
- age `> staleTickThreshold` => outstanding is stale and emits `CancelOrder`
- stale-cancel events stop after cancel (no same-event submit/cancel contradiction)
- after cancel clears outstanding state, later events may submit again

Duplicate suppression remains explicit:

- when the same-side order is already outstanding and still fresh, hold and emit no submit actuation.

## Files

- `hft_model.h` / `hft_model.cpp`: bounded market event, lifecycle state, actuation models, and helper logic.
- `hft_nodes.cpp`: explicit ingest -> stale-check -> cancel/decide -> place/hold frame path and lifecycle runner.
- `hft_model_tests.cpp`: model/helper tests (signal mapping, age/stale helpers, submit gating).
- `hft_nodes_tests.cpp`: frame-path behavior tests (fresh hold, stale cancel buy/sell, duplicate suppression).
- `hft_runtime_tests.cpp`: end-to-end lifecycle tests and deterministic replay tests.

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
  -o out/dragon_hft_tests

./out/dragon_hft_tests
```
