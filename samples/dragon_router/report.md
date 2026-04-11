# Dragon Router benchmark report (M12e)

## 1) Purpose

This report documents a bounded M12e scaling/sensitivity experiment for the Dragon Router sample.

The question for this pass is narrower than M12d:

> Within this sample, which cost drivers appear to dominate: baseline forwarding, candidate-count utility selection, or queue/retry/drain behavior?

This report does **not** claim production-router throughput, protocol completeness, or hardware replacement.

## 2) Scope and experiment dimensions

The measured code path remains sample-local and unchanged in semantics:

- deterministic route decision
- utility-based candidate selection
- forward/drop/queue outcomes
- deferred retry/drain behavior

M12e varies only two bounded dimensions:

1. **Candidate-count scaling** for utility selection:
   - `1`, `2`, `4`, and `8` healthy route candidates
2. **Queue-pressure scaling** for deferred behavior:
   - `light`: one queued packet then recovery drain
   - `heavy`: three queued packets, one blocked retry pass, then recovery drain

No new router features or protocol families were introduced in this pass.

## 3) Benchmark scenarios

The benchmark lane now contains seven sample-local scenarios:

1. **`DragonRouter_ForwardKnownRouteBench`**
   - One packet, known healthy route, direct forward path.
   - Baseline forward/reference cost.

2. **`DragonRouter_UtilityCandidates1Bench`**
3. **`DragonRouter_UtilityCandidates2Bench`**
4. **`DragonRouter_UtilityCandidates4Bench`**
5. **`DragonRouter_UtilityCandidates8Bench`**
   - One packet with increasing candidate-set size for utility path choice.
   - Isolates cost sensitivity to candidate enumeration/scoring in the real route decision path.

6. **`DragonRouter_QueueRetryLightBench`**
   - Blocked pass queues one packet, then unblocked pass drains.
   - Exercises queue + recovery behavior with modest deferred work.

7. **`DragonRouter_QueueRetryHeavyBench`**
   - Blocked pass queues three packets, second blocked pass performs deferred retries, third pass unblocks and drains.
   - Exercises a heavier deferred-work shape without adding new semantics.

## 4) Execution environment / run conditions

Observed run conditions for this report:

- build command:
  - `g++ -std=c++23 -Wall -Wextra -pedantic -DMARIONETTE_TEST_REPO_ROOT="/workspace/DragonGod" ... -o out/dragon_router_tests`
- benchmark command:
  - `./out/dragon_router_tests --bench DragonRouter_`
- benchmark harness mode:
  - Marionette benchmark mode via `--bench`
- compiler observed:
  - `g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0`

Machine-level controls (CPU pinning/isolation/frequency policy) were not captured by harness output.

## 5) Results

Raw benchmark output is preserved in `samples/dragon_router/bench-results.txt`.

### Run 1

| Benchmark | Iterations | Elapsed (ns) | Avg (ns) |
|---|---:|---:|---:|
| DragonRouter_ForwardKnownRouteBench | 10000 | 87,420,864 | 8,742 |
| DragonRouter_UtilityCandidates1Bench | 10000 | 81,611,942 | 8,161 |
| DragonRouter_UtilityCandidates2Bench | 10000 | 105,838,184 | 10,583 |
| DragonRouter_UtilityCandidates4Bench | 10000 | 114,997,255 | 11,499 |
| DragonRouter_UtilityCandidates8Bench | 10000 | 145,067,990 | 14,506 |
| DragonRouter_QueueRetryLightBench | 5000 | 78,237,653 | 15,647 |
| DragonRouter_QueueRetryHeavyBench | 3000 | 218,053,648 | 72,684 |

### Run 2

| Benchmark | Iterations | Elapsed (ns) | Avg (ns) |
|---|---:|---:|---:|
| DragonRouter_ForwardKnownRouteBench | 10000 | 83,474,142 | 8,347 |
| DragonRouter_UtilityCandidates1Bench | 10000 | 110,830,437 | 11,083 |
| DragonRouter_UtilityCandidates2Bench | 10000 | 100,104,940 | 10,010 |
| DragonRouter_UtilityCandidates4Bench | 10000 | 120,045,734 | 12,004 |
| DragonRouter_UtilityCandidates8Bench | 10000 | 151,038,779 | 15,103 |
| DragonRouter_QueueRetryLightBench | 5000 | 91,955,196 | 18,391 |
| DragonRouter_QueueRetryHeavyBench | 3000 | 198,567,513 | 66,189 |

## 6) Bounded interpretation

Bounded interpretation from these two runs:

- **Queue-pressure dominates** this lane: `QueueRetryHeavy` is consistently the most expensive scenario by a large margin, and `QueueRetryLight` is materially above baseline forwarding.
- **Candidate-count sensitivity appears real**: moving from `2` to `4` to `8` candidates trends upward in both runs, consistent with more utility-candidate work.
- **Low-end candidate variance is noisy** in this environment (`1` vs `2` swapped ordering across runs), so only broad trends should be claimed.

Within this bounded sample, the strongest supported reading is that deferred queue/retry/drain behavior is the primary cost driver, while utility candidate growth contributes a secondary, measurable cost trend.

## 7) What this does and does not conclude

This experiment supports only sample-local conclusions:

- it helps explain cost shape inside the Dragon Router sample
- it does not establish production line-rate feasibility
- it does not compare against ASIC dataplanes
- it does not validate real-world protocol-stack performance

## 8) Limitations

This report still has explicit limits:

- bounded sample only, not a full router
- no real packet parsing pipeline
- no NIC/kernel-bypass/DPDK/XDP dataplane integration
- no production routing protocols
- only two runs in one environment
- no captured machine metadata beyond visible commands/compiler
