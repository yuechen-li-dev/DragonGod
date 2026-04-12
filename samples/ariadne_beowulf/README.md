# Ariadne Beowulf sample scaffold (Pre-AriadneDG)

This directory is a bounded Ariadne-on-DragonGod narrative kernel experiment scaffold using the dragon episode of **Beowulf** as the motivating sample domain.

This pass is scaffold-only.

It is **not**:

- a full narrative engine,
- a full Beowulf implementation,
- a claim that long-form story authoring should permanently live in raw C++.

The purpose is to pressure-test DragonGod as a compact narrative execution kernel: deterministic progression, explicit choices, bounded memory/state, and rollback-friendly flow.

The expected growth path is a minimal golden path first, then narrow expansions only if the architecture proves clean under pressure.

## Files in this scaffold

- `beowulf_model.h` / `beowulf_model.cpp`: placeholder sample-local state model and narrow validation helpers.
- `beowulf_nodes.cpp`: scaffold runtime-link smoke path that runs a tiny DragonGod scenario from sample-local code.
- `beowulf_tests.cpp`: sample-local smoke tests proving this sample compiles and links against DragonGod.

## Build and run (sample-local smoke path)

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
  samples/ariadne_beowulf/beowulf_model.cpp \
  samples/ariadne_beowulf/beowulf_nodes.cpp \
  samples/ariadne_beowulf/beowulf_tests.cpp \
  -o out/ariadne_beowulf_tests

./out/ariadne_beowulf_tests
```

This proves scaffold wiring only; narrative semantics are intentionally deferred.
