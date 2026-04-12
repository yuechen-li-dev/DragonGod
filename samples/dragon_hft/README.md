# Dragon HFT semantic golden-path sample (M14a)

This sample is a bounded market-reaction / order-decision control-loop experiment.

It is **not** a full trading system.

It is **not** an exchange simulator.

It is **not** a profitability claim.

The purpose of this M14a pass is to prove that DragonGod-style explicit frame logic can express a deterministic HFT-like golden path without polluting `src/DragonGod/`.

## Bounded behavior covered

For each inbound market event (mailbox input), the sample runs explicit frames:

1. `IngestMarketEventFrame`
2. `DecisionFrame`
3. one of:
   - `PlaceBuyFrame`
   - `PlaceSellFrame`
   - `HoldFrame`

The bounded rule is:

- signal `>= threshold` => buy intent
- signal `<= -threshold` => sell intent
- otherwise hold

And duplicate suppression is explicit:

- when the same-side order is already outstanding, hold and emit no submit actuation.

## Files

- `hft_model.h` / `hft_model.cpp`: bounded market event, state, actuation models and helper logic.
- `hft_nodes.cpp`: explicit ingest -> decide -> place/hold frame path and golden-path runner.
- `hft_model_tests.cpp`: model/helper tests.
- `hft_nodes_tests.cpp`: frame-path behavior tests.
- `hft_runtime_tests.cpp`: end-to-end golden-path and deterministic replay tests.

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
