# Ariadne Beowulf sample (M15a golden path)

This directory is a bounded Ariadne-on-DragonGod narrative kernel experiment using the dragon episode of **Beowulf**.

This pass implements a compact, deterministic golden path:

- barrow approach scene output,
- one bounded pre-battle choice,
- deterministic transition into first clash.

It is still **not** a full narrative engine or full episode adaptation.

## Implemented narrative beats

1. **Barrow approach**: short tragic opening lines establish kingship, doom, and the barrow.
2. **Pre-battle choice**: one bounded choice set (`Speak proudly`, `Speak grimly`, `Speak as a king`) mapped from mailbox input.
3. **First clash entry**: choice updates tone/state and deterministically alters first-clash poetic lines.

## Files

- `beowulf_model.h` / `beowulf_model.cpp`: bounded scene/tone/state model plus deterministic text and choice mapping helpers.
- `beowulf_nodes.h` / `beowulf_nodes.cpp`: explicit frame-style progression (`barrow -> choice -> first clash`) using DragonGod mailbox + blackboard primitives.
- `beowulf_model_tests.cpp`: model/helper determinism and bounded-shape tests.
- `beowulf_nodes_tests.cpp`: scene progression and choice-branch node tests.
- `beowulf_runtime_tests.cpp`: replay determinism and end-to-end golden-path tests.

## Build and run sample-local tests

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
  samples/ariadne_beowulf/beowulf_model_tests.cpp \
  samples/ariadne_beowulf/beowulf_nodes_tests.cpp \
  samples/ariadne_beowulf/beowulf_runtime_tests.cpp \
  -o out/ariadne_beowulf_tests

./out/ariadne_beowulf_tests
```
