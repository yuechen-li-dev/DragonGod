# Dragon HFT scaffold sample (Pre-HFT)

This sample is a bounded market-reaction / order-decision / stale-recovery experiment scaffold.

It is **not** a full trading system.

It is **not** a profitability claim.

It is **not** a production exchange execution engine.

The purpose of this Pre-HFT pass is only to establish a clean, deterministic, inspectable sample home that can pressure-test whether bounded trading/control loops fit DragonGod without polluting `src/DragonGod/`.

This scaffold starts from a minimal golden path and should only expand if follow-on milestones earn the architectural complexity.

## Files

- `hft_model.h` / `hft_model.cpp`: placeholder sample-local state types and smoke-path entrypoint.
- `hft_nodes.cpp`: placeholder frame/node lane for future bounded HFT control steps.
- `hft_tests.cpp`: smoke-path test that proves this sample compiles and links against DragonGod runtime.

## Build and run scaffold smoke test

From repository root:

```bash
mkdir -p out

g++ -std=c++23 -Wall -Wextra -pedantic \
  -DMARIONETTE_TEST_REPO_ROOT="/workspace/DragonGod" \
  tests/Marionette/test_harness.cpp \
  tests/Marionette/test_doom.cpp \
  tests/Marionette/test_main.cpp \
  src/DragonGod/runtime_state.cpp \
  src/DragonGod/runtime_nodes.cpp \
  src/DragonGod/runtime_session.cpp \
  src/DragonGod/runtime_compat.cpp \
  src/DragonGod/tick_loop.cpp \
  samples/dragon_hft/hft_model.cpp \
  samples/dragon_hft/hft_nodes.cpp \
  samples/dragon_hft/hft_tests.cpp \
  -o out/dragon_hft_tests

./out/dragon_hft_tests
```
