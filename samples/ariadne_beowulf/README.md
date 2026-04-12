# Ariadne Beowulf sample (M15c death / last-words / ending-state path)

This directory is a bounded Ariadne-on-DragonGod narrative kernel experiment using the dragon episode of **Beowulf**.

This pass extends M15b with a bounded tragic-resolution loop:

- barrow approach scene output,
- one bounded pre-battle tone choice,
- deterministic first clash,
- deterministic retainer collapse,
- deterministic Wiglaf-remains loyalty beat,
- explicit fatal-wound beat,
- compact last words,
- bounded legacy ending tone,
- clean deterministic completion.

It is still **not** a full narrative engine or full episode adaptation.

## Implemented narrative beats

1. **Barrow approach**: short tragic opening lines establish kingship, doom, and the barrow.
2. **Pre-battle choice**: one bounded choice set (`Speak proudly`, `Speak grimly`, `Speak as a king`) mapped from mailbox input.
3. **First clash entry**: choice updates tone/state and determines first-clash voice.
4. **Retainer collapse**: thanes fail/flee under dragon-fire pressure; collapse is explicit in output and state.
5. **Wiglaf remains**: one loyalty anchor remains explicit in output and state.
6. **Tragic handoff**: story narrows to king and Wiglaf in the final turn.
7. **Beowulf falls**: fatal state is explicit in both output and state flags.
8. **Last words**: compact final lines mark inheritance and duty.
9. **Legacy ending**: bounded ending image varies by earlier tone/loyalty pressure (`LonelyAsh`, `DoomEmber`, `OathKept`).
10. **Completion**: deterministic terminal state marker closes the run.

## Files

- `beowulf_model.h` / `beowulf_model.cpp`: bounded scene/tone/loyalty/legacy state model plus deterministic text and ending-tone mapping helpers.
- `beowulf_nodes.h` / `beowulf_nodes.cpp`: explicit frame-style progression (`barrow -> choice -> clash -> collapse -> Wiglaf -> handoff -> death -> last words -> legacy ending -> complete`) using DragonGod mailbox + blackboard primitives.
- `beowulf_model_tests.cpp`: model/helper determinism, loyalty-to-legacy mapping, and ending invariants.
- `beowulf_nodes_tests.cpp`: scene progression and failure-path node tests including death, last words, and bounded ending-variation checks.
- `beowulf_runtime_tests.cpp`: replay determinism and end-to-end tragic completion tests.

## Build and run sample-local tests

From repository root:

```bash
mkdir -p out

g++ -std=c++23 -Wall -Wextra -pedantic \
  -DMARIONETTE_TEST_REPO_ROOT='"/workspace/DragonGod"' \
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
