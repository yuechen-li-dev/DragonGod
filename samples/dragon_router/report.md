# Dragon Router benchmark report (M12d)

## 1) Purpose

This report documents the bounded benchmark question for the Dragon Router sample:

> Does this sample provide credible evidence that **some router-like control logic** can be structured as deterministic software loops, making the blanket "ASIC-only" framing too coarse for this scope?

This report does **not** claim that software replaces production router ASIC dataplanes.

## 2) Scope of the sample

The measured code path is the existing bounded Dragon Router sample from M12a/M12b/M12c:

- deterministic software control-loop behavior
- route decision + utility-based path choice + queue/retry/drain behavior
- sample-local packet/state model and actuation outputs

The sample is **not**:

- a full router
- a real dataplane or protocol stack
- a line-rate forwarding claim
- a production network stack implementation

## 3) Benchmark scenarios

The benchmark lane contains three sample-local scenarios:

1. **`DragonRouter_ForwardKnownRouteBench`**
   - Runs `RunRouterGoldenPath` on one packet with a known healthy route and available port.
   - Measures baseline known-route forward path cost.

2. **`DragonRouter_UtilityPathChoiceBench`**
   - Runs `RunRouterGoldenPath` on one packet with multiple healthy route candidates and different congestion scores.
   - Measures additional work from utility-based path selection before forwarding.

3. **`DragonRouter_QueueRetryDrainBench`**
   - Runs a blocked-port pass that queues the packet, then a retry/drain pass after unblocking the port.
   - Measures the heaviest queue + deferred retry/drain behavior in this sample lane.

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
- benchmark iteration counts (from benchmark definitions/output):
  - ForwardKnownRoute: 10000
  - UtilityPathChoice: 10000
  - QueueRetryDrain: 5000

Machine-specific details such as CPU model, frequency policy, and OS scheduling state were **not** captured by the benchmark output. This report does not infer them.

## 5) Results

Raw benchmark output is preserved in `samples/dragon_router/bench-results.txt`.

Two runs were captured in this environment.

### Run 1

| Benchmark | Iterations | Elapsed (ns) | Avg (ns) |
|---|---:|---:|---:|
| DragonRouter_ForwardKnownRouteBench | 10000 | 81,710,058 | 8,171 |
| DragonRouter_UtilityPathChoiceBench | 10000 | 80,945,212 | 8,094 |
| DragonRouter_QueueRetryDrainBench | 5000 | 81,641,071 | 16,328 |

### Run 2

| Benchmark | Iterations | Elapsed (ns) | Avg (ns) |
|---|---:|---:|---:|
| DragonRouter_ForwardKnownRouteBench | 10000 | 70,292,924 | 7,029 |
| DragonRouter_UtilityPathChoiceBench | 10000 | 81,488,315 | 8,148 |
| DragonRouter_QueueRetryDrainBench | 5000 | 70,078,942 | 14,015 |

## 6) Interpretation

Bounded interpretation from these runs:

- `QueueRetryDrain` is consistently the heaviest scenario in average nanoseconds, which is intuitive for a two-stage queue then drain path.
- `ForwardKnownRoute` and `UtilityPathChoice` are both lighter than queue/retry/drain and are in the same order of magnitude.
- `UtilityPathChoice` is above `ForwardKnownRoute` in run 2 and very close in run 1, indicating that path-choice overhead exists but is small relative to run-to-run noise in this environment.

Within this limited sample, the results are compatible with the claim that structured software control behavior for bounded router-like logic is plausible.

They do **not** establish that software can replace production ASIC dataplanes or line-rate systems.

## 7) Limitations

This benchmark report has explicit limitations:

- bounded sample only, not a full router
- no real packet parsing pipeline
- no NIC/kernel-bypass/DPDK/XDP-style dataplane integration
- no production routing protocols
- no line-rate throughput comparison against hardware dataplanes
- only two captured runs in one environment
- no CPU pinning/isolation controls documented
- no captured machine metadata beyond visible compiler and commands

Because of these limits, this report should be treated as a small experiment artifact, not a production performance claim.
