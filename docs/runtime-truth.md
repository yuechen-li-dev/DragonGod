# DragonGod Runtime Truth (Current Repository State)

This document describes DragonGod runtime behavior as implemented **right now** in this repository.
It is intentionally code-truth-first and test-grounded.

## 1) What DragonGod is

DragonGod currently provides a deterministic, tick-driven stack-frame runtime centered on:

- frame control flow (`Continue`, `Wait`, `Push`, `Pop`, `Replace`, `Complete`, `Fail`)
- per-frame program counters (`pc`) and enter semantics
- typed blackboard storage (`bool`, `int`) with per-tick dirty tracking
- mailbox visibility/consumption staging
- utility decision helpers (`when`, `Decide`) with memory (commit age, hysteresis, min-commit, tie-break)
- actuation emission (`Immediate` + `Deferred`) captured as requests
- chunk-based save/restore across a strict between-ticks boundary
- replay comparison via bounded `TickTraceEntry` data

Runtime entry points are `StackFrameRuntime` and `StackFrameRuntimeSession`; scenarios are currently selected by `StackScriptScenario` and mapped to built-in frame functions in the frame registry.

### Important current-shape limitation

The current repository does **not** yet expose a generic external author-registration API for arbitrary user-defined frame sets. The runtime uses a built-in registry populated by `BuildFrameRegistry()` and scenario-to-root mapping via `ScenarioRootFrame(...)`.

---

## 2) Canonical frame authoring shape

A canonical frame function has shape:

- signature: `FrameControl SomeFrame(FrameCtx& ctx)`
- switch on phase/program counter (`ctx.Pc()` or `ctx.PcAs<TEnum>()`)
- perform explicit state reads/writes/effects through context (`ctx.Bb()`, `ctx.Mb()`, `ctx.Act()`)
- return a `Dg::*` control helper for every branch

Typical pattern:

1. phase 0 does setup/read, returns `Continue(next)` / `Push(...)` / `WaitTicks(...)`
2. later phase checks result and returns `Complete()` or `Fail(...)`

### Typed phase shape (canonical M9 style)

Use enum-backed typed phases instead of raw integers:

- `ctx.PcAs<MyPhaseEnum>()`
- `Dg::Continue(MyPhaseEnum::Next)`
- `Dg::WaitTicks(ticks, MyPhaseEnum::CurrentOrNext)`
- `Dg::Stay()` to continue while keeping current `pc`

`Stay()` currently returns `Continue` with `stayOnCurrentPc=true`, so control remains in the same phase on the next tick.

### Do / do-not guidance

- Do use `PcAs<TEnum>()` + typed `Continue`/`WaitTicks` for multi-phase logic.
- Do use `Stay()` when you intentionally need same-phase re-evaluation.
- Do not hand-roll raw phase counters in blackboard for ordinary intra-frame phase logic.
- Do not return partially-specified controls; always return an explicit `Dg::*` control.

---

## 3) Control and stack semantics

Per tick, runtime operates on the current top stack frame:

1. emit tick/enter traces
2. honor wait countdown if active
3. execute frame function and receive `FrameControl`
4. apply control to stack + pc
5. append bounded tick trace entry

### Semantics by control kind

- `Continue(resumePc)`: frame remains active; `pc` becomes `resumePc`.
- `Stay()`: frame remains active; `pc` remains unchanged.
- `WaitTicks(ticks, resumePc)`: `pc` set to `resumePc`; frame waits across ticks using `remainingWaitTicks` countdown.
- `Push(target, resumePc)`: parent `pc` set to `resumePc`; child frame pushed.
- `Pop()`: current frame exits completed; parent resumes next tick.
- `Replace(target)`: top frame replaced atomically with new frame.
- `Complete()`: equivalent terminal-complete exit for current frame (pops it).
- `Fail(reason)`: frame exits failed and runtime becomes terminal failed.

If popping/completing empties the stack, runtime outcome becomes `Completed`.

### Terminal rules

- `Failed` is terminal immediately.
- `Completed` is terminal **only when pending deferred actuation queue is empty**.

### Do / do-not guidance

- Do model subroutines as child frames via `Push` + child `Pop`.
- Do use `Replace` when abandoning current frame state is desired.
- Do not treat `Wait` as terminal.
- Do not assume completion ends runtime if deferred actuation is still pending.

---

## 4) Blackboard and dirty tracking semantics

Blackboard supports typed keys (`BbKey<T>`) with current supported value types:

- `bool`
- `int`

Operations:

- `Set` upserts by slot and marks slot dirty
- `TryGet` reads if present
- `GetOr` returns fallback when missing
- `IsDirty` checks current dirty set

Dirty semantics:

- dirty slots are cleared at the **start** of each tick
- writes during that tick accumulate unique dirty slots
- tick traces and run results expose per-tick dirty slot snapshots
- snapshot (`Save`) persists dirty slots from the **most recently completed tick**

### Do / do-not guidance

- Do use typed keys with stable slot ids.
- Do rely on runtime dirty set instead of custom “was written” flags.
- Do not expect dirty state to persist across new tick start unless restored from chunk boundary.
- Do not store unsupported types in blackboard without extending runtime types explicitly.

---

## 5) Mailbox semantics

Mailbox has two queues:

- `visible_`: consumable this tick
- `staged_`: newly enqueued, becomes visible next tick

Rules:

- `Enqueue` always appends to `staged_`
- `BeginTick` moves all staged messages into visible in FIFO order, then clears staged
- `PeekFront` reads without consuming
- `ConsumeFront` consumes FIFO front from visible

Runtime also supports scheduled inputs by tick; messages scheduled for `nextTick_` are enqueued before `BeginTick`, so they become visible that tick.

### Do / do-not guidance

- Do assume FIFO order for both visibility and consumption.
- Do use `PeekFront` when data must be inspected without removal.
- Do not assume during-frame `Enqueue` is visible immediately.
- Do not assume hidden mailbox side effects; visibility is only through documented begin-tick staging.

---

## 6) Persistence and restore boundary

Persistence uses `RuntimeChunk` and is explicitly chunk-based (not object graph dump).

Persisted fields include:

- scenario, `nextTick`, `lastOutcome`
- scheduled messages
- stack frames (`id`, `pc`, `entered`, `remainingWaitTicks`)
- utility memory entries (committed target + age)
- pending deferred actuation
- blackboard values + dirty slots
- mailbox visible and staged queues

Boundary contract implemented in code:

- call `Save()` between ticks, after tick N effects are complete and before tick N+1 begins

### Do / do-not guidance

- Do treat `RuntimeChunk` as the canonical persistence unit.
- Do persist/restore through session constructor that accepts a chunk.
- Do not treat persistence as serialization of arbitrary in-memory object graphs.
- Do not call `Save()` mid-tick by inventing custom execution hooks (not supported by current architecture).

---

## 7) Trace and replay comparison model

Each executed tick appends `TickTraceEntry` containing bounded runtime truth:

- tick index + outcome
- stack snapshot
- dirty slots
- visible mailbox snapshot
- utility decision trace entries
- actuation emitted now + pending deferred queue

Replay comparison:

- `CompareTickTraces(expected, actual)` checks per-entry equality then entry count
- reports first mismatch index, reason, and expected/actual payload entries
- `FormatTraceComparison(...)` produces compact diagnostic string

### Do / do-not guidance

- Do compare deterministic runs via `CompareTickTraces` for replay safety.
- Do use formatted artifact output when mismatch appears.
- Do not compare only final blackboard/outcome when validating determinism-critical paths.

---

## 8) Utility layer semantics (`when`, `Decide`, hysteresis, min-commit, tie-break)

Utility authoring surface:

- candidates built via `when(targetFrame, considerationFn)`
- decision executed by `Decide(ctx, { ...candidates... }, DecideOptions)`

Decision behavior:

1. scores each candidate via consideration fn (null fn => 0), clamped to `[0,1]`
2. finds highest score set
3. resolves ties by policy:
   - `FirstListed`
   - `LastListed`
   - `KeepCurrent` (prefers already committed target if tied and present)
4. applies commitment guards when challenger beats current target:
   - `minCommitTicks`: blocks switch until age threshold reached
   - `hysteresis`: challenger must exceed committed score by margin
5. updates utility memory (`committed`, `age`)
6. pushes chosen target frame with `Push(chosen, ctx.Pc())`
7. emits per-decision trace (`UtilityDecisionTraceEntry`)

### Do / do-not guidance

- Do use `Decide` as the only utility switch mechanism in frame logic.
- Do tune tie-break/hysteresis/min-commit through `DecideOptions`.
- Do not manually store commitment state in blackboard.
- Do not bypass decision trace if deterministic comparison is required.

---

## 9) Actuation boundary (immediate vs deferred)

Frame code requests actuation through `ctx.Act()`:

- `Immediate(id)`: emits request in current tick
- `Deferred(id, delayTicks)`: schedules pending request; matures when `dueTick <= currentTick`

Tick lifecycle for actuation:

1. `BeginTick(nextTick)` clears current emitted list
2. `FlushMatured()` moves mature pending requests into emitted-now list
3. frame logic may emit immediate or schedule deferred requests
4. trace stores both emitted-now and pending queue snapshots

Ordering guarantees from implementation/tests:

- immediate requests preserve authored call order inside a tick
- deferred requests with same due tick mature in scheduling order
- deferred queue persists through save/restore

### Do / do-not guidance

- Do express side effects through explicit actuation requests.
- Do use deferred actuation instead of hand-rolled timer counters when timing can be represented as request maturity.
- Do not bypass `ctx.Act()` with direct world mutation in frame functions.
- Do not assume runtime completion means no future actuation unless pending queue is empty.

---

## 10) MarionetteTests as the proof harness

The repository uses a custom test harness (Marionette) with:

- `FACT(...)` registration macros
- deterministic assertions (`ASSERT_*`, sequence checks)
- artifact writing for diagnostics (`WriteTextArtifact`)
- replay/restore scenario tests across M0–M9 directories

For DragonGod runtime truth, Marionette tests currently serve as the semantic proof harness for:

- stack/control behavior
- blackboard + dirty tracking
- mailbox visibility and consumption
- chunk save/restore determinism
- replay trace comparison model
- utility decision semantics
- actuation timing/ordering/persistence
- typed phase and `Stay()` behavior

---

## 11) Cross-cutting anti-patterns (authoring guardrails)

1. **Do not hand-roll phase state in blackboard** when typed phase helpers already express it.
2. **Do not bypass `ctx.Act()`** with direct effect mutation; that breaks trace/persistence boundaries.
3. **Do not hand-roll deferred timing in frame logic** if deferred actuation already models it.
4. **Do not maintain custom utility commit memory** in frame-local state/blackboard.
5. **Do not assume mailbox immediate visibility** for during-tick enqueue.
6. **Do not treat persistence as object graph dumping**; use `RuntimeChunk` contract only.
7. **Do not validate determinism using only terminal outcome**; compare full tick traces.
8. **Do not rely on undocumented side channels**; use frame context surfaces explicitly.

---

## 12) Known clarity gaps called out explicitly

These are current-state clarity gaps in repository ergonomics (not hidden assumptions):

- Public docs for runtime are currently minimal (`src/DragonGod/README.md` is placeholder), so this file is the first substantial runtime truth layer.
- Frame extensibility is presently internal/static (built-in frame registry), so external “author your own frame pack” workflow is not yet documented because it is not yet implemented as a stable public API.

