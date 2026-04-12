# Dragon Router benchmark report (M12g)

## 1) Purpose

This report documents a bounded M12g experiment focused on the Dragon Router heavy queue/retry/drain lane.

Question for this pass:

> Once queued packets become retry-eligible, is it better to wait for the original preferred path, or drain through a currently viable alternate path?

This report does **not** claim production-router throughput, protocol completeness, or hardware replacement.

## 2) Scope and experiment dimensions

The runtime path remains sample-local and deterministic:

- route candidate selection and utility scoring
- forward/drop/queue outcomes
- deferred retry/drain behavior

M12g keeps M12f retry heuristics and adds one bounded drain policy dimension:

1. **Retry heuristics**
   - `BaselineFixedDelay`
   - `BackoffDelay`
   - `ConditionAware`
2. **Drain policy**
   - `PreferOriginalPath`: queued packets wait for queue-time preferred egress.
   - `AllowAlternatePath`: queued packets may drain through another currently viable candidate when preferred remains blocked.

The heavy M12g alternate-path scenario shape is fixed and deterministic:

- two candidate ports are blocked at queue time
- first deferred pass remains blocked
- alternate path recovers before preferred path
- final pass restores preferred path

## 3) Metrics used for interpretation

The benchmark lane reports elapsed/average time. Runtime state also tracks bounded deferred-work and behavior counters:

- `retryAttempts`: failed retry attempts that executed
- `retrySkippedCount`: deferred retries skipped in condition-aware mode
- `drainedCount`: total queued packets forwarded later
- `drainedPreferredPathCount`: queued drains forwarded on queue-time preferred path
- `drainedAlternatePathCount`: queued drains forwarded on non-preferred alternate path
- `queuedPackets.size()` at end: deferred backlog still pending

These counters are validated in sample-local tests and make behavioral tradeoffs visible beyond wall-clock timing.

## 4) Benchmark scenarios

Benchmarks in this pass:

- `DragonRouter_ForwardKnownRouteBench`
- `DragonRouter_UtilityCandidates1Bench`
- `DragonRouter_UtilityCandidates2Bench`
- `DragonRouter_UtilityCandidates4Bench`
- `DragonRouter_UtilityCandidates8Bench`
- `DragonRouter_QueueRetryLightBench`
- `DragonRouter_QueueRetryBaselineBench`
- `DragonRouter_QueueRetryBackoffBench`
- `DragonRouter_QueueRetryConditionAwareBench`
- `DragonRouter_QueueRetryAlternateDrainBench`
- `DragonRouter_QueueRetryBackoffAlternateDrainBench`

Heavy queue/retry comparisons are intentionally bounded:

- baseline/backoff: same heavy alternate-recovery timeline, but queue waits for preferred path
- alternate-drain: same timeline, but queue may drain on alternate path when it recovers first

## 5) Execution environment / run conditions

Observed run conditions for this report:

- build command:
  - `mkdir -p out`
  - `g++ -std=c++23 -Wall -Wextra -pedantic -DMARIONETTE_TEST_REPO_ROOT="/workspace/DragonGod" ... -o out/dragon_router_tests`
- benchmark command:
  - `./out/dragon_router_tests --bench DragonRouter_`
- compiler observed:
  - `g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0`

Machine-level CPU controls were not captured by harness output.

## 6) Results

Raw benchmark output is preserved in `samples/dragon_router/bench-results.txt`.

| Benchmark | Iterations | Elapsed (ns) | Avg (ns) |
|---|---:|---:|---:|
| DragonRouter_ForwardKnownRouteBench | 10,000 | 71,411,939 | 7,141 |
| DragonRouter_UtilityCandidates1Bench | 10,000 | 74,825,105 | 7,482 |
| DragonRouter_UtilityCandidates2Bench | 10,000 | 88,422,710 | 8,842 |
| DragonRouter_UtilityCandidates4Bench | 10,000 | 106,586,270 | 10,658 |
| DragonRouter_UtilityCandidates8Bench | 10,000 | 124,923,157 | 12,492 |
| DragonRouter_QueueRetryLightBench | 5,000 | 75,480,307 | 15,096 |
| DragonRouter_QueueRetryBaselineBench | 3,000 | 215,151,172 | 71,717 |
| DragonRouter_QueueRetryBackoffBench | 3,000 | 145,907,669 | 48,635 |
| DragonRouter_QueueRetryConditionAwareBench | 3,000 | 162,194,281 | 54,064 |
| DragonRouter_QueueRetryAlternateDrainBench | 3,000 | 210,550,388 | 70,183 |
| DragonRouter_QueueRetryBackoffAlternateDrainBench | 3,000 | 150,102,105 | 50,034 |

## 7) Bounded interpretation

Bounded reading from this run:

- Heavy queue/retry remains the dominant cost lane relative to direct forwarding.
- In this environment, alternate-path drain with baseline retry cadence did **not** materially beat preferred-path waiting (`70,183 ns` vs `71,717 ns` avg).
- Backoff remained the strongest timing reducer in this bounded setup.
- Combining backoff + alternate drain stayed near backoff-only timing, showing no clear additive win here.

Behavior tradeoff exposed by counters/tests:

- `PreferOriginalPath` keeps queue semantics stable but may defer forwarding even when an alternate becomes viable first.
- `AllowAlternatePath` can drain sooner through an alternate candidate and increments `drainedAlternatePathCount`, which explicitly marks behavioral shift.

## 8) What this does and does not conclude

This experiment supports sample-local conclusions only:

- alternate-path drain is implementable as a small deterministic policy toggle
- alternate drain can change recovery behavior (which path drains queued packets)
- timing benefit is scenario- and heuristic-dependent in this bounded setup

This does **not** establish production dataplane throughput, global multipath policy quality, or universal routing behavior.

## 9) Limitations

- bounded sample only
- no NIC/kernel-bypass dataplane integration
- no production routing protocol stack
- single benchmark capture in one environment
- no machine metadata beyond visible command/compiler output
