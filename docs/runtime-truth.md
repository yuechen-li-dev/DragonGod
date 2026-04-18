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

Runtime entry points are `StackFrameRuntime` and `StackFrameRuntimeSession`.

- Canonical proof/demo runs can still use `StackScriptScenario` values mapped to built-in fixtures.
- Author-owned domains can now provide their own `FrameRegistry` + root frame via `StackFrameSessionInit`.

---

## 2) Canonical frame authoring shape

The current canonical authored shape (M9 style) is a typed-phase frame:

- signature: `FrameControl SomeFrame(FrameCtx& ctx)`
- switch on typed phase/program counter (`ctx.PcAs<TEnum>()`)
- perform explicit state reads/writes/effects through context (`ctx.Bb()`, `ctx.Mb()`, `ctx.Act()`)
- return a `Dg::*` control helper for every branch

Typical pattern:

1. phase 0 does setup/read, returns `Continue(next)` / `Push(...)` / `WaitTicks(...)`
2. later phase checks result and returns `Complete()` or `Fail(...)`

### Typed phase shape (canonical M9 style)

Use enum-backed typed phases for authored logic:

- `ctx.PcAs<MyPhaseEnum>()`
- `Dg::Continue(MyPhaseEnum::Next)`
- `Dg::WaitTicks(ticks, MyPhaseEnum::CurrentOrNext)`
- `Dg::Stay()` to continue while keeping current `pc`

`Stay()` currently returns `Continue` with `stayOnCurrentPc=true`, so control remains in the same phase on the next tick.

Raw integer `pc` still exists in runtime storage and internal machinery, but authored frame code should lead with typed enum phases unless a narrowly-scoped compatibility case requires otherwise.

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
- `WaitTicks(ticks, resumePc)`: `pc` is set to `resumePc` immediately, and frame execution is skipped for the requested wait window before that `pc` runs again.
- `Push(target, resumePc)`: parent `pc` set to `resumePc`; child frame is pushed immediately onto stack, but executes on the next tick (runtime executes one top frame per tick).
- `Pop()`: current frame exits completed; parent resumes next tick.
- `Replace(target)`: top frame replaced atomically with new frame during the current tick; replacement frame first executes on the next tick.
- `Complete()`: equivalent terminal-complete exit for current frame (pops it).
- `Fail(reason)`: frame exits failed and runtime becomes terminal failed.

If popping/completing empties the stack, runtime outcome becomes `Completed`.

### Terminal rules

- `Failed` is terminal immediately.
- `Completed` is terminal **only when pending deferred actuation queue is empty**.

Drain-period behavior (important operational detail):

- if stack logic has already completed and stack is empty, but deferred actuation is still pending, runtime still executes ticks.
- those ticks still produce normal per-tick artifacts (`tickTrace`, `actuationByTick`, mailbox snapshot, dirty snapshot).
- newly matured deferred requests can still emit during this drain period.
- no frame function executes during drain ticks because stack is empty.
- `IsTerminal()` remains false until pending deferred actuation is empty.

This is why extra tick entries can appear after the logical "frame work is done" moment: those ticks are deferred-actuation drain ticks, not hidden frame re-entry.

Calling `RunForTicks(...)` on an already-terminal `StackFrameRuntimeSession`:

- the run loop exits immediately (no call to `RunSingleTick` succeeds),
- zero new ticks execute,
- per-tick outputs for that call (`tickTrace`, `trace`, `dirtySlotsByTick`, `visibleMailboxByTick`, `actuationByTick`) are empty,
- `finalOutcome` remains the already-terminal outcome and `finalBlackboard` is returned as-is.

Interpret this as “no advancement occurred,” not as silent progression.

### Do / do-not guidance

- Do model subroutines as child frames via `Push` + child `Pop`.
- Do use `Replace` when abandoning current frame state is desired.
- Do not treat `Wait` as terminal.
- Do not assume completion ends runtime if deferred actuation is still pending.
- Do not read empty per-call results from an already-terminal session as hidden execution.

Push timing detail in the current top-of-stack model:

- tick N executes the current top frame and receives `FrameControl`.
- if control is `Push`, parent frame `pc` is updated to push `resumePc` during tick N, and the child is appended to stack during tick N.
- tick N+1 then executes that child (now top of stack).
- after child `Pop()`/`Complete()`, parent resumes on a later tick from the stored resume `pc`.


`WaitTicks(...)` timing detail in observable author terms:

- `N` means “wait for N tick boundaries before this frame runs again.”
- `WaitTicks(N, resumePc)` stores `resumePc` now, and runtime resumes frame execution at that `pc` after N ticks have passed.
- Internally, runtime stores `remainingWaitTicks = N - 1` when `N > 0`, then decrements once per tick while returning `Wait`.

Concrete examples (current implementation behavior):

- `WaitTicks(1, X)`:
  - tick N: frame returns wait; no second frame execution that tick.
  - tick N+1: frame runs again at `pc = X`.
- `WaitTicks(2, X)`:
  - tick N: frame returns wait.
  - tick N+1: still waiting (no frame function call).
  - tick N+2: frame runs again at `pc = X`.

- `WaitTicks(0, X)` (edge case, current behavior):
  - tick N: frame returns wait with `resumePc = X`.
  - tick N+1: frame runs again at `pc = X`.
  - operationally, this behaves like "resume next tick" (same practical wait span as `WaitTicks(1, X)` in authored flows).
  - authoring guidance: do not use `WaitTicks(0, ...)` as a meaningful delay primitive; use `Continue(...)` when you do not intend a real wait.

`Replace(...)` timing detail mirrors `Push(...)` timing:

- tick N executes current top frame and receives `Replace(target)`.
- runtime swaps the top-of-stack entry to `target` during tick N.
- tick N+1 is the first tick that actually invokes the replacement frame function.

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
- `ctx.Bb()` is access to the real session blackboard state, not a per-call copy
- multiple `Set(...)` calls in one tick all mutate that same shared state immediately
- if the same slot is written multiple times in one tick, final value wins for persisted state
- dirty tracking is slot-presence for that tick window (written or not), not write-count multiplicity

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

### Message payload shape (current full surface)

`Message` is currently a two-field payload:

```cpp
enum class MessageKind
{
    Signal,
    Alert
};

struct Message
{
    MessageKind kind = MessageKind::Signal;
    int value = 0;
};
```

- `kind` is the coarse message category (`Signal` or `Alert` today).
- `value` is the integer payload associated with that message kind.
- There are no additional message fields in the current runtime; this is the full mailbox payload surface used by frame logic and traces.

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

`DecideOptions` defaults (when omitted, including plain `Decide(ctx, {...})`) are:

- `hysteresis = 0.0f`
- `minCommitTicks = 0`
- `tieBreak = TieBreakPolicy::KeepCurrent`

So default behavior applies no extra challenger margin, no minimum commit-age gate, and uses `KeepCurrent` tie resolution when scores tie and a committed target exists.

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
6. returns `Push(chosen, ctx.Pc())`
7. emits per-decision trace (`UtilityDecisionTraceEntry`)

Score-clamp consequence (author-facing):

- out-of-range consideration returns are silently clamped to `[0,1]`.
- runtime does not treat this as an error or warning.
- treat `[0,1]` as a hard authoring contract for predictable decisions.
- clamp is a defensive safety net, not a signal that `2.0` or `-1.0` has extra meaning.

`Dg::Decide(...)` timing and resume semantics in current runtime:

- structurally, `Decide` is a scoring + tie-break + commitment helper that ends by returning a normal `Push`.
- because it returns `Push(chosen, ctx.Pc())`, it preserves the parent frame's current `pc` as the parent resume point.
- selected action child is pushed in the same tick the decision is made, but starts executing on the next tick (same `Push` timing model as above).
- when the chosen child later `Pop()`s or `Complete()`s, parent resumes at that preserved `pc`, so the decision phase can be revisited deterministically on subsequent ticks.

### Do / do-not guidance

- Do use `Decide` as the only utility switch mechanism in frame logic.
- Do tune tie-break/hysteresis/min-commit through `DecideOptions`.
- Do not manually store commitment state in blackboard.
- Do not bypass decision trace if deterministic comparison is required.

---

## 9) Actuation boundary (immediate vs deferred)

Frame code requests actuation through `ctx.Act()`:

- `Immediate(id)`: emits request in current tick
- `Deferred(id, delayTicks)`: schedules pending request; matures when `dueTick <= currentTick` during a later tick's flush phase (`FlushMatured`).

Tick lifecycle for actuation:

1. `BeginTick(nextTick)` clears current emitted list
2. `FlushMatured()` moves mature pending requests into emitted-now list
3. frame logic may emit immediate or schedule deferred requests
4. trace stores both emitted-now and pending queue snapshots

Ordering guarantees from implementation/tests:

- immediate requests preserve authored call order inside a tick
- deferred requests with same due tick mature in scheduling order
- deferred queue persists through save/restore

Deferred maturity timing examples (author-facing, current runtime order):

- Runtime tick order is `BeginTick` -> `FlushMatured` -> frame function execution.
- So a deferred request created by frame code on tick `N` is always created **after** tick `N`'s `FlushMatured` has already run.
- Therefore, deferred requests never mature later in the same tick they are emitted, even when `delayTicks == 0`.

Concrete examples:

- `ctx.Act().Deferred(id, 1)` emitted during tick `N`:
  - at tick `N`, request is added to pending with `dueTick = N + 1` (not emitted this tick).
  - at tick `N+1`, `FlushMatured` sees `dueTick <= currentTick` and emits it.

- `ctx.Act().Deferred(id, 0)` emitted during tick `N`:
  - at tick `N`, request is added to pending with `dueTick = N` (still not emitted this tick because flush already happened).
  - at tick `N+1`, next tick's `FlushMatured` sees `dueTick <= currentTick` and emits it.

Operationally, both `Deferred(id, 0)` and `Deferred(id, 1)` are first observable no earlier than the next tick's flush in the current runtime architecture.

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

## 11) Runtime entry surfaces: `StackFrameRuntime` vs `StackFrameRuntimeSession`

Both are valid runtime entry points, but with different intent:

- `StackFrameRuntime` is the stateless convenience surface for fresh runs.
  - call `RunForTicks(scenario, ticks[, mailboxInput])`
  - each call constructs a fresh session from scenario + optional mailbox input
  - use when tests/authors only need single-shot run results.
- `StackFrameRuntimeSession` is the stateful continuation surface.
  - keeps evolving runtime state across calls to `RunForTicks(...)`
  - exposes `NextTick()`, `LastOutcome()`, `IsTerminal()`, and `Save()`
  - supports split execution, save/restore, and deterministic replay across chunk boundaries.

Choose `StackFrameRuntime` for simple scenario assertions and `StackFrameRuntimeSession` when you need stepping or persistence boundaries as part of test/runtime behavior.

---

## 12) `FrameRunResult` as a test-facing inspection surface

`FrameRunResult` is the primary post-run inspection object for authored tests.

High-level result fields:

- `finalOutcome`: terminal/non-terminal outcome after the run slice.
  - assert this when validating completion/failure/wait progression.
- `finalBlackboard`: blackboard snapshot at end of run slice.
  - assert this for authored state outcomes.

Per-tick deterministic surfaces:

- `tickTrace`: bounded per-tick runtime truth (`tick`, `outcome`, stack, dirty slots, visible mailbox, utility decisions, emitted/pending actuation).
  - assert this (often via `CompareTickTraces`) when replay determinism matters.
- `actuationByTick`: emitted actuation requests per executed tick.
  - assert this when validating immediate/deferred effect timing.
- `visibleMailboxByTick` and `dirtySlotsByTick`:
  - convenience per-tick projections of mailbox/dirty data also present inside each `tickTrace` entry.
  - assert these when mailbox staging or blackboard write timing is the only behavior under test and you want narrower assertions.

Relationship between these surfaces (current implementation):

- `tickTrace[i].visibleMailbox` and `visibleMailboxByTick[i]` are captured from the same tick-time mailbox snapshot.
- `tickTrace[i].dirtySlots` and `dirtySlotsByTick[i]` are captured from the same tick-time dirty-slot snapshot.
- so these are not independent alternate truths; they are convenience slices of the same per-tick runtime evidence.

Practical test-author choice:

- prefer `tickTrace` for full deterministic replay assertions or multi-surface timing checks.
- prefer `visibleMailboxByTick` / `dirtySlotsByTick` for focused mailbox/dirty expectations where full trace matching would add noise.

Lower-level/raw trace surface:

- `trace` (`std::vector<FrameTraceEvent>`) is event-level frame trace data (enter/step/push/pop/terminal markers).
  - use this when debugging or asserting specific control-path event sequences.
  - prefer `tickTrace` for stable whole-tick deterministic comparison and use `trace` when you need finer control-event detail.

Current raw `FrameTraceKind` values:

- `Tick`
- `Enter`
- `Step`
- `Push`
- `Pop`
- `Replace`
- `ExitCompleted`
- `ExitFailed`
- `TerminalCompleted`
- `TerminalFailed`

---

## 13) `Dg::Fail(reason)` observability semantics (current truth)

`Dg::Fail(reason)` is an explicit author failure marker that returns `FrameControlKind::Fail` and carries an integer `failReason` in `FrameControl`.

What `reason` means today:

- it is author-supplied debugging/tagging data (for example `Fail(100)` vs `Fail(706)` branches),
- it is not interpreted by runtime control logic beyond marking control kind `Fail`,
- runtime terminal outcome is simply `StackRunOutcome::Failed`.

Where it is visible today:

- directly at the frame return/control object boundary (`FrameControl.failReason`).

Where it is not surfaced today:

- `FrameTraceEvent` does not include a fail-reason field,
- `TickTraceEntry` does not include a fail-reason field,
- `FrameRunResult` final outcome does not carry a fail-reason field.

So high-level trace/replay surfaces expose that a failure happened, but not the integer reason code.

Current stack/failure propagation behavior when a frame returns `Dg::Fail(reason)`:

- the failing top frame emits `Step` (`control=Fail`), then `ExitFailed`, then `TerminalFailed` in raw `trace`.
- runtime marks outcome failed immediately for that tick and does not pop-and-resume parent frames.
- frames below the failing frame remain in the in-memory stack snapshot for that terminal tick, but they do not execute again because the session is terminal failed.
- blackboard writes that already happened earlier in that same tick are not rolled back; `finalBlackboard` preserves those writes.

This is implementation truth in the current runtime, not a generic stack-machine convention; do not infer alternate parent-unwind or rollback semantics that are not present in this codebase.

Unregistered frame failure behavior (current runtime truth):

- if control flow lands on a frame id that is not present in `BuildFrameRegistry()`, `registry.Find(...)` returns null,
- runtime treats that as terminal failure for the active tick (not a soft no-op),
- event trace emits failed-exit/terminal-failed markers for that tick,
- per-tick `tickTrace` entry for that tick records failed outcome, and run `finalOutcome` is `Failed`.

Debugging hint:

- if you hit terminal failure and no authored `Dg::Fail(reason)` path explains it, verify frame registration first (especially after adding/changing `Push(...)` or `Replace(...)` targets).

---

## 14) Cross-cutting anti-patterns (authoring guardrails)

1. **Do not hand-roll phase state in blackboard** when typed phase helpers already express it.
2. **Do not bypass `ctx.Act()`** with direct effect mutation; that breaks trace/persistence boundaries.
3. **Do not hand-roll deferred timing in frame logic** if deferred actuation already models it.
4. **Do not maintain custom utility commit memory** in frame-local state/blackboard.
5. **Do not assume mailbox immediate visibility** for during-tick enqueue.
6. **Do not treat persistence as object graph dumping**; use `RuntimeChunk` contract only.
7. **Do not validate determinism using only terminal outcome**; compare full tick traces.
8. **Do not rely on undocumented side channels**; use frame context surfaces explicitly.

---

## 15) Known clarity gaps called out explicitly

These are current-state clarity gaps in repository ergonomics (not hidden assumptions):

- Public docs for runtime are currently minimal (`src/DragonGod/README.md` is placeholder), so this file is the first substantial runtime truth layer.
- Frame extensibility is presently internal/static (built-in frame registry), so external “author your own frame pack” workflow is not yet documented because it is not yet implemented as a stable public API.
