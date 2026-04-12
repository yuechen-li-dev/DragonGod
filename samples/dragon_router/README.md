# Dragon Router semantic golden path sample

This sample is a bounded software routing/control-loop experiment.

It is **not** a full router implementation.

Its purpose is to pressure-test whether the blanket assumption "router logic must inherently belong to ASIC-only architecture" is too coarse for DragonGod-oriented deterministic control loops.

## M12g alternate-path drain lane + M12b behavior

The sample now implements a deterministic packet control loop with explicit frame-shaped steps:

1. **Ingress / classify**
2. **Route decision**
3. **Utility-based path choice** among multiple healthy candidate egress ports
4. **Forward** when a viable candidate exists
5. **Drop** for unknown routes
6. **Queue** when all candidates are unavailable
7. **Deferred retry/drain** for queued packets using deterministic tick-based retry timing

M12f/M12g keep this same bounded runtime shape and now expose explicit queue-recovery controls:

- `BaselineFixedDelay`: constant retry cadence
- `BackoffDelay`: bounded retry spacing after repeated failures
- `ConditionAware`: skip retry passes while no healthy/usable recovery signal exists
- `PreferOriginalPath` drain policy: queued packets wait for the preferred path chosen at queue-time
- `AllowAlternatePath` drain policy: when retry is eligible, queued packets can drain via a currently viable alternate path

The sample state is intentionally bounded and explicit:

- packet model (`Packet`)
- route table (`RouteEntry`) with multiple candidates per destination
- port state (`PortState`) including availability plus bounded congestion score
- queued packet metadata (`QueueEntry`) with retry timing and retry count
- deterministic actuation (`Actuation`) including queue/retry/drain outcomes
- forwarding/drop/queue/drain counters (`RouterState`)
- retry-effort counters (`retryAttempts`, `retrySkippedCount`) for interpreting heavy recovery cost
- drain-path counters (`drainedPreferredPathCount`, `drainedAlternatePathCount`) for interpreting recovery behavior shifts


## Sample-local benchmarks

M12g keeps the bounded candidate-scaling lane and extends heavy recovery comparison with alternate-path drain:

- candidate-count scaling for utility path selection (`1`, `2`, `4`, `8` candidates)
- queue/retry pressure scaling (`light` blocked-then-recovered case)
- queue/retry heuristic variants on the heavy blocked-then-recovered case (baseline, backoff, condition-aware)
- alternate-path drain variants on the heavy blocked -> alternate-recovers -> preferred-recovers case

Each benchmark still exercises `RunRouterGoldenPath` and the real route decision + utility + queue/retry/drain logic.

- `DragonRouter_ForwardKnownRouteBench`
  - stresses straightforward known-route forwarding through the real runtime entrypoint
- `DragonRouter_UtilityCandidates1Bench`
- `DragonRouter_UtilityCandidates2Bench`
- `DragonRouter_UtilityCandidates4Bench`
- `DragonRouter_UtilityCandidates8Bench`
  - stresses deterministic utility path-selection overhead as candidate set size grows
- `DragonRouter_QueueRetryLightBench`
  - stresses queue + one recovery drain pass
- `DragonRouter_QueueRetryBaselineBench`
- `DragonRouter_QueueRetryBackoffBench`
- `DragonRouter_QueueRetryConditionAwareBench`
- `DragonRouter_QueueRetryAlternateDrainBench`
- `DragonRouter_QueueRetryBackoffAlternateDrainBench`
  - stress the same heavy queued backlog shape with different deterministic retry heuristics
  - compare waiting for preferred-path drain vs opportunistic alternate-path drain when only the alternate recovers first

These are timing measurements only; they do not replace semantic correctness tests.

## Benchmark report artifact (M12g)

- Benchmark report: [`report.md`](./report.md)
- Raw benchmark capture: [`bench-results.txt`](./bench-results.txt)

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
  samples/dragon_router/router_benchmarks_tests.cpp \
  -o out/dragon_router_tests

./out/dragon_router_tests

# run all registered benchmarks
./out/dragon_router_tests --bench

# run only router benchmarks
./out/dragon_router_tests --bench DragonRouter_
```
