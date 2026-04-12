# Dragon Router benchmark report (M12f)

## 1) Purpose

This report documents a bounded M12f experiment focused on the Dragon Router queue/retry/drain path.

Question for this pass:

> Can small deterministic retry heuristics reduce wasted deferred work on the heavy recovery path while preserving recovery behavior?

This report does **not** claim production-router throughput, protocol completeness, or hardware replacement.

## 2) Scope and experiment dimensions

The runtime path remains sample-local and deterministic:

- route candidate selection and utility scoring
- forward/drop/queue outcomes
- deferred retry/drain behavior

M12f compares three retry heuristics on the heavy recovery shape:

1. **BaselineFixedDelay**
   - Existing fixed retry cadence (`retryDelayTicks`).
2. **BackoffDelay**
   - Retry delay grows with retry count and is capped (`maxBackoffDelayTicks`).
3. **ConditionAware**
   - Retry pass is skipped when no healthy usable recovery signal exists (link-up, queue-not-full, and congestion at/below configured bound).

## 3) Metrics used for interpretation

The benchmark lane still reports elapsed/average time, and the runtime now also tracks bounded deferred-work counters in state:

- `retryAttempts`: number of failed retry attempts that actually executed
- `retrySkippedCount`: number of deferred retry passes skipped by condition-aware gating
- `drainedCount`: number of queued packets eventually forwarded
- `queuedPackets.size()` at end: deferred backlog still pending

These counters are validated in sample-local runtime tests and are intended to make tradeoffs visible beyond wall-clock timing.

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

The three heavy queue/retry benchmarks run the same blocked->blocked->recovered shape, changing only retry heuristic.

## 5) Execution environment / run conditions

Observed run conditions for this report:

- build command:
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
| DragonRouter_ForwardKnownRouteBench | 10,000 | 69,436,082 | 6,943 |
| DragonRouter_UtilityCandidates1Bench | 10,000 | 68,676,622 | 6,867 |
| DragonRouter_UtilityCandidates2Bench | 10,000 | 81,687,927 | 8,168 |
| DragonRouter_UtilityCandidates4Bench | 10,000 | 96,289,787 | 9,628 |
| DragonRouter_UtilityCandidates8Bench | 10,000 | 119,709,797 | 11,970 |
| DragonRouter_QueueRetryLightBench | 5,000 | 71,504,111 | 14,300 |
| DragonRouter_QueueRetryBaselineBench | 3,000 | 175,845,271 | 58,615 |
| DragonRouter_QueueRetryBackoffBench | 3,000 | 120,402,542 | 40,134 |
| DragonRouter_QueueRetryConditionAwareBench | 3,000 | 160,553,355 | 53,517 |

## 7) Bounded interpretation

Bounded reading from this run:

- Heavy queue/retry remains the dominant cost lane relative to direct forwarding.
- Backoff reduced heavy-lane timing most in this run, consistent with fewer immediate retry attempts under persistent blockage.
- Condition-aware gating also improved heavy-lane timing vs baseline, but less than backoff in this particular run.
- Utility candidate scaling trend still rises as candidate count increases.

Tradeoff note:

- A lower heavy-path cost here comes from doing less immediate retry work, not from any protocol expansion.
- This pass does not claim that these heuristics are universally better under all traffic/recovery timelines.

## 8) What this does and does not conclude

This experiment supports sample-local conclusions only:

- deterministic retry heuristics can materially change heavy deferred-work cost in this sample
- measured gains are bounded to this setup and single observed environment
- this does not establish production dataplane throughput or system-level router behavior

## 9) Limitations

- bounded sample only
- no NIC/kernel-bypass dataplane integration
- no production routing protocol stack
- single benchmark capture in one environment
- no machine metadata beyond visible command/compiler output
