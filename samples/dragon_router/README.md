# Dragon Router semantic golden path sample

This sample is a bounded software routing/control-loop experiment.

It is **not** a full router implementation.

Its purpose is to pressure-test whether the blanket assumption "router logic must inherently belong to ASIC-only architecture" is too coarse for DragonGod-oriented deterministic control loops.

## M12b congestion + utility behavior

The sample now implements a deterministic packet control loop with explicit frame-shaped steps:

1. **Ingress / classify**
2. **Route decision**
3. **Utility-based path choice** among multiple healthy candidate egress ports
4. **Forward** when a viable candidate exists
5. **Drop** for unknown routes
6. **Queue** when all candidates are unavailable
7. **Deferred retry/drain** for queued packets using deterministic tick-based retry timing

The sample state is intentionally bounded and explicit:

- packet model (`Packet`)
- route table (`RouteEntry`) with multiple candidates per destination
- port state (`PortState`) including availability plus bounded congestion score
- queued packet metadata (`QueueEntry`) with retry timing and retry count
- deterministic actuation (`Actuation`) including queue/retry/drain outcomes
- forwarding/drop/queue/drain counters (`RouterState`)

## Sample-local tests

M12b tests are kept with the sample and use `*_tests*` filenames:

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
