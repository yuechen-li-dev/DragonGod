# Dragon Router sample scaffold

This sample is a bounded software routing/control-loop experiment.

It is **not** a full router implementation.

Its purpose is to pressure-test whether the blanket assumption "router logic must inherently belong to ASIC-only architecture" is too coarse for DragonGod-oriented control loops.

This scaffold starts with a minimal golden path and should only grow when the architecture earns additional complexity.

## Smoke wiring

A minimal smoke binary is provided by `router_tests.cpp`.

Compile and run from repository root:

```bash
g++ -std=c++23 -Wall -Wextra -pedantic \
  src/DragonGod/runtime_state.cpp \
  src/DragonGod/runtime_compat.cpp \
  src/DragonGod/runtime_nodes.cpp \
  src/DragonGod/runtime_session.cpp \
  samples/dragon_router/router_model.cpp \
  samples/dragon_router/router_nodes.cpp \
  samples/dragon_router/router_tests.cpp \
  -o out/dragon_router_smoke

./out/dragon_router_smoke
```
