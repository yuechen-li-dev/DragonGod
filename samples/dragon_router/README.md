# Dragon Router semantic golden path sample

This sample is a bounded software routing/control-loop experiment.

It is **not** a full router implementation.

Its purpose is to pressure-test whether the blanket assumption "router logic must inherently belong to ASIC-only architecture" is too coarse for DragonGod-oriented deterministic control loops.

## M12a golden-path behavior

The sample implements a deterministic packet control loop with explicit frame-shaped steps:

1. **Ingress / classify**
2. **Route decision**
3. **Forward** for known healthy routes
4. **Drop** for unknown routes
5. **Queue** when egress is unavailable or congested

The sample state is intentionally bounded and explicit:

- packet model (`Packet`)
- route table (`RouteEntry`)
- port state (`PortState`)
- queued packet metadata (`QueueEntry`)
- deterministic actuation (`Actuation`)
- forwarding/drop/queue counters (`RouterState`)

## Sample-local tests

M12a tests are kept with the sample and use `*_tests*` filenames:

- `router_model_tests.cpp`
- `router_nodes_tests.cpp`
- `router_runtime_tests.cpp`

Compile and run from repository root:

```bash
g++ -std=c++23 -Wall -Wextra -pedantic \
  tests/Marionette/test_harness.cpp \
  tests/Marionette/test_doom.cpp \
  tests/Marionette/test_main.cpp \
  samples/dragon_router/router_model.cpp \
  samples/dragon_router/router_nodes.cpp \
  samples/dragon_router/router_model_tests.cpp \
  samples/dragon_router/router_nodes_tests.cpp \
  samples/dragon_router/router_runtime_tests.cpp \
  -o out/dragon_router_tests

./out/dragon_router_tests
```
