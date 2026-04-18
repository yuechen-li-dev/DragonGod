# Frame authoring (canonical DragonGod style)

This page is the practical style guide for authoring frame logic against the runtime that exists today.

For runtime semantics details, cross-check [`runtime-truth.md`](./runtime-truth.md).

## Core runtime vs canonical fixture code

- Runtime core machinery is the generic execution kernel (`FrameCtx`, `Mailbox`, `Blackboard`, actuation, utility, tick/session flow).
- `src/DragonGod/runtime_nodes.cpp` contains canonical proof/demo fixtures used by in-repo scenarios and tests.
- Author-owned domains are expected to define their own frame sets and register them through `FrameRegistry` + explicit root frame (`StackFrameSessionInit`).

Read canonical nodes as examples/proofs, not as a required domain schema.

## Canonical frame shape

```cpp
[[nodiscard]] FrameControl SomeFrame(FrameCtx& ctx)
{
    switch (ctx.PcAs<SomePhase>()) {
    case SomePhase::Start:
        // read/write through ctx surfaces only
        return Dg::Continue(SomePhase::Next);
    case SomePhase::Next:
        return Dg::Complete();
    default:
        return Dg::Fail(1234);
    }
}
```

Why this shape:

- `FrameCtx` gives explicit access to runtime surfaces (`Bb`, `Mb`, `Act`).
- `switch` on `PcAs<TEnum>()` keeps authored control flow deterministic and inspectable.
- explicit `Dg::*` control returns preserve stack/runtime traceability.

## Typed phases (`ctx.PcAs<TEnum>()`)

Use enum-backed phases for multi-step logic.

```cpp
enum class PatrolPhase : std::uint32_t
{
    AwaitSignal,
    React
};
```

Then:

```cpp
switch (ctx.PcAs<PatrolPhase>()) { ... }
```

Why: this avoids “magic pc integers” and prevents ad hoc blackboard phase state.

## Control helpers and when to use each

- `Dg::Continue(next)`
  - move to another phase on next tick.
- `Dg::WaitTicks(ticks, resume)`
  - wait for `ticks` tick boundaries, then run this frame again at `resume`.
  - `WaitTicks(1, resume)` resumes on the immediately following tick (no skipped intervening tick).
  - `WaitTicks(2, resume)` skips one full intervening tick and runs on the tick after that.
  - `WaitTicks(0, resume)` is a sharp-edge do-not: today it still resumes on the next tick in practice, so it is not a meaningful wait duration.
  - if no real wait is intended, use `Continue(...)` instead of `WaitTicks(0, ...)`.
- `Dg::Stay()`
  - continue without changing pc (implemented as continue-with-stay flag).
- `Dg::Push(child, resumePc)`
  - call-like child frame activation; the child becomes the new top frame and begins executing on the next tick.
  - the parent resumes later at `resumePc` after the child pops/completes.
- `Dg::Pop()`
  - return from a child frame.
- `Dg::Replace(frame)`
  - replace top frame atomically in the current tick; replacement frame starts on the next tick (same execution timing as `Push`).
- `Dg::Complete()`
  - terminal-complete this frame.
- `Dg::Fail(reason)`
  - terminal-fail this frame/runtime.
  - `reason` is an author/debugging tag; it is not part of the high-level `finalOutcome` or tick-trace surface.

### Current typed-phase asymmetry (intentional, not a docs bug)

Today, typed phase helpers exist for:

- `ctx.PcAs<TEnum>()`
- `Dg::Continue(TEnum)`
- `Dg::WaitTicks(ticks, TEnum)`

But `Dg::Push(...)` is still numeric for the parent resume point:

- `Dg::Push(FrameId target, std::uint32_t resumePc)`

That means parent frames using typed enums still pass a numeric resume pc on push. Canonical pattern:

```cpp
enum class RootPhase : std::uint32_t
{
    PushChild = 0,
    AfterChild = 1
};

[[nodiscard]] FrameControl Root(FrameCtx& ctx)
{
    switch (ctx.PcAs<RootPhase>()) {
    case RootPhase::PushChild:
        return Dg::Push(
            FrameId::ChildPop,
            static_cast<std::uint32_t>(RootPhase::AfterChild));
    case RootPhase::AfterChild:
        return Dg::Complete();
    default:
        return Dg::Fail(999);
    }
}
```

Using a raw integer resume pc is also valid when explicit at the call site.

## Blackboard via `ctx.Bb()`

Canonical usage:

```cpp
ctx.Bb().Set(Keys::HighSignal, true);
if (ctx.Bb().GetOr(Keys::HighSignal, false)) {
    return Dg::Complete();
}
```

Use typed keys (`BbKey<bool>` / `BbKey<int>`) with stable slot ids.

Operational blackboard truth:

- `ctx.Bb()` is the real mutable session blackboard (not a temporary copy).
- multiple `Set(...)` calls in one tick all apply to that same underlying state.
- if the same slot is written multiple times in one tick, the last written value is what later reads and persisted state observe.
- dirty tracking marks whether a slot was written during the tick; it does not count how many times it was written.

> ⚠️ **Hazard: `BbKey` identity is the slot id, not the name.**
>
> In the current runtime, `Blackboard::Set/TryGet/GetOr/IsDirty` index storage by `key.slot`.
> The `key.name` string is descriptive/debugging metadata only.
> If two different keys reuse the same `.slot`, they alias the same stored value silently, even if names differ.
> Treat slot allocation as a global uniqueness constraint for the runtime/session key set.
>
> Current project convention: define keys centrally in `runtime_nodes.cpp` under `nodes::Keys` with explicit integer slots (`1`, `2`, ...), then reuse those constants everywhere.

## Mailbox via `ctx.Mb()`

Canonical usage:

```cpp
dragongod::Message message;
if (!ctx.Mb().ConsumeFront(message)) {
    return Dg::WaitTicks(1, MyPhase::AwaitMessage);
}
```

Use `PeekFront` for inspect-without-consume flows.

`ctx.Mb().Enqueue(...)` is also a legitimate in-frame authoring surface:

```cpp
ctx.Mb().Enqueue(dragongod::Message{
    .kind = dragongod::MessageKind::Signal,
    .value = 42
});
```

Current runtime rule: enqueue during a frame appends to mailbox staged messages, so it is **not** visible to `PeekFront` / `ConsumeFront` in that same tick. It becomes visible on the next tick when runtime calls `BeginTick()` and staged messages move to visible.

This is useful for self-signaling or frame-to-frame mailbox flows where one phase/frame intentionally schedules mailbox work for the next tick.

## Actuation via `ctx.Act()`

Canonical usage:

```cpp
ctx.Act().Immediate(ActId::OpenDoor);
ctx.Act().Deferred(ActId::RaiseAlarm, 2);
```

Why: preserves deterministic per-tick emission/pending traces and save/restore behavior.

Deferred timing sharp edge (current runtime order):

- each tick flushes matured deferred requests before running frame code.
- so deferred requests authored during tick `N` never mature later in tick `N` itself.
- `Deferred(id, 1)` emitted on tick `N` matures on tick `N+1` flush.
- `Deferred(id, 0)` emitted on tick `N` also first matures on tick `N+1` flush (because tick `N` flush already happened before your frame code ran).

## Utility helpers (`When::...`, `Dg::when`, `Dg::Decide`)

### Consideration function shape (`When::...` is just this shape)

A utility consideration is a plain function with signature-compatible shape:

```cpp
[[nodiscard]] float MyConsideration(const FrameCtx& ctx)
{
    // read runtime state through ctx
    const int raw = ctx.Bb().GetOr(MyScoreKey, 0);
    return std::clamp(static_cast<float>(raw) / 100.0f, 0.0f, 1.0f);
}
```

Author expectations:

- reads from `FrameCtx` (usually blackboard, sometimes mailbox-derived state).
- returns a score in `[0, 1]`.
- can be any plain function of that shape; `When::HighSignal`, `When::ResourcePressure`, and `When::Always` are built-in examples, not special language features.
- consideration signatures are `const FrameCtx&`, so they are read-only by design.
- in that context `ctx.Bb()` is const access; mutating calls like `ctx.Bb().Set(...)` are compile-time misuse, not a supported pattern.
- keep consideration logic pure/read-only and perform mutations in frame body code that receives non-const `FrameCtx&`.

Utility score range warning:

- runtime silently clamps consideration scores to `[0,1]`.
- out-of-range returns are not treated as errors.
- treat normalized `[0,1]` output as required for predictable authored behavior; clamp is fallback protection, not recommended signaling.

### Utility decision call site shape

```cpp
return Dg::Decide(
    ctx,
    {
        Dg::when(FrameId::UtilityActionPrimary, When::HighSignal),
        Dg::when(FrameId::UtilityActionSecondary, When::ResourcePressure),
        Dg::when(FrameId::UtilityActionFallback, When::Always)
    },
    Dg::DecideOptions{ .tieBreak = Dg::TieBreakPolicy::FirstListed });
```

Why: utility commitment/hysteresis/min-commit state is managed in runtime utility memory (not hand-rolled in frame data).

---

## Worked examples aligned to `runtime_nodes.cpp`

These are real current examples grounded in `runtime_nodes.cpp`.
Some excerpts still use raw `ctx.Pc()` integers because that is what the current in-repo implementation uses in those specific frames.
For new multi-phase authored code, typed phases (`PcAs<TEnum>()`, typed `Continue(...)`, typed `WaitTicks(...)`) remain the preferred style.

### A) Typed phase + mailbox + wait/continue (`RootMailboxConsumeFifo`)

Real implementation excerpt (condensed):

```cpp
enum class RootMailboxConsumePhase : std::uint32_t
{
    ConsumeFirst,
    ConsumeSecond
};

[[nodiscard]] FrameControl RootMailboxConsumeFifo(FrameCtx& ctx)
{
    Message message;
    switch (ctx.PcAs<RootMailboxConsumePhase>()) {
    case RootMailboxConsumePhase::ConsumeFirst:
        if (!ctx.Mb().ConsumeFront(message)) {
            return Dg::WaitTicks(1, RootMailboxConsumePhase::ConsumeFirst);
        }

        ctx.Bb().Set(Keys::FirstMessageValue, message.value);
        return Dg::Continue(RootMailboxConsumePhase::ConsumeSecond);
    case RootMailboxConsumePhase::ConsumeSecond:
        if (!ctx.Mb().ConsumeFront(message)) {
            return Dg::WaitTicks(1, RootMailboxConsumePhase::ConsumeSecond);
        }

        ctx.Bb().Set(Keys::SecondMessageValue, message.value);
        return Dg::Complete();
    default:
        return Dg::Fail(400);
    }
}
```

Why this is canonical:

- phase enum makes first/second consume explicit.
- mailbox-empty path is deterministic `WaitTicks`, not ad hoc loops.
- writes happen through typed blackboard keys only.
- default branch fails explicitly on unexpected phase.

### B) Typed phase + `Stay()` + actuation (`RootTypedPhaseMailboxAct`)

Real implementation excerpt (condensed):

```cpp
enum class RootTypedPhaseMailboxActPhase : std::uint32_t
{
    AwaitSignal,
    AwaitAlert
};

[[nodiscard]] FrameControl RootTypedPhaseMailboxAct(FrameCtx& ctx)
{
    Message message;
    switch (ctx.PcAs<RootTypedPhaseMailboxActPhase>()) {
    case RootTypedPhaseMailboxActPhase::AwaitSignal:
        if (!ctx.Mb().ConsumeFront(message)) {
            return Dg::WaitTicks(1, RootTypedPhaseMailboxActPhase::AwaitSignal);
        }

        if (message.kind != MessageKind::Signal) {
            return Dg::Fail(706);
        }

        ctx.Bb().Set(Keys::FirstMessageValue, message.value);
        ctx.Act().Deferred(ActId::RaiseAlarm, 1);
        return Dg::Continue(RootTypedPhaseMailboxActPhase::AwaitAlert);
    case RootTypedPhaseMailboxActPhase::AwaitAlert:
        if (!ctx.Mb().ConsumeFront(message)) {
            return Dg::Stay();
        }

        if (message.kind != MessageKind::Alert) {
            return Dg::Fail(707);
        }

        ctx.Bb().Set(Keys::SecondMessageValue, message.value);
        return Dg::Complete();
    default:
        return Dg::Fail(708);
    }
}
```

Why this is canonical:

- first phase validates typed mailbox input before state changes.
- deferred actuation is requested through `ctx.Act()` at frame boundary.
- second phase keeps same phase with `Stay()` while awaiting later message progression.
- completion is explicit and branch-local.

### C) Parent/child structure with `Push`/`Pop`

Real implementation excerpt (condensed):

```cpp
[[nodiscard]] FrameControl RootPushChild(FrameCtx& ctx)
{
    switch (ctx.Pc()) {
    case 0:
        return Dg::Push(FrameId::ChildPop, 1);
    case 1:
        return Dg::Complete();
    default:
        return Dg::Fail(100);
    }
}

[[nodiscard]] FrameControl ChildPop(FrameCtx& ctx)
{
    switch (ctx.Pc()) {
    case 0:
        return Dg::Pop();
    default:
        return Dg::Fail(200);
    }
}
```

Why this is canonical:

- parent pushes child with explicit parent resume pc.
- after `Push`, the child becomes the new top frame and starts executing on the next tick.
- child terminates with `Pop()` instead of parent bookkeeping.
- after `Pop`, the parent resumes at the declared continuation point on a later tick and finishes.

### D) Replace to abandon current frame state (`RootReplace` -> `RecoveryComplete`)

Real implementation excerpt (condensed):

```cpp
[[nodiscard]] FrameControl RootReplace(FrameCtx& ctx)
{
    switch (ctx.Pc()) {
    case 0:
        return Dg::Replace(FrameId::RecoveryComplete);
    default:
        return Dg::Fail(101);
    }
}

[[nodiscard]] FrameControl RecoveryComplete(FrameCtx& ctx)
{
    switch (ctx.Pc()) {
    case 0:
        return Dg::Complete();
    default:
        return Dg::Fail(106);
    }
}
```

Why this uses `Replace` (instead of `Pop` + `Push`):

- intent is to abandon `RootReplace` as the active top frame immediately and continue with a different top frame.
- no parent resume-point bookkeeping is needed (`Push` requires a parent resume pc; `Replace` does not).
- the replaced frame state (its `pc`, wait countdown, and future progression path) is discarded in practice; execution continues in the replacement frame (`RecoveryComplete`) as a fresh top-of-stack frame.

### E) Utility-driven action frame selection

This example explicitly overrides the default tie-break behavior with `FirstListed`.
If you omit `DecideOptions`, the runtime uses the documented `DecideOptions` defaults from `runtime-truth.md`.


Real implementation excerpt (condensed):

```cpp
[[nodiscard]] FrameControl RootUtilityHighestScore(FrameCtx& ctx)
{
    switch (ctx.Pc()) {
    case 0:
        ctx.Bb().Set(Keys::HighSignalScore, 10);
        ctx.Bb().Set(Keys::ResourcePressureScore, 80);
        ctx.Bb().Set(Keys::UtilityDecisionsMade, 0);
        return Dg::Continue(1);
    case 1:
        if (ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) >= 1) {
            return Dg::Complete();
        }

        return Dg::Decide(
            ctx,
            {
                Dg::when(FrameId::UtilityActionPrimary, When::HighSignal),
                Dg::when(FrameId::UtilityActionSecondary, When::ResourcePressure),
                Dg::when(FrameId::UtilityActionFallback, When::Always)
            },
            Dg::DecideOptions{ .tieBreak = Dg::TieBreakPolicy::FirstListed });
    default:
        return Dg::Fail(500);
    }
}

[[nodiscard]] FrameControl UtilityActionSecondary(FrameCtx& ctx)
{
    switch (ctx.Pc()) {
    case 0:
        ctx.Bb().Set(Keys::UtilityChoice, 2);
        ctx.Bb().Set(
            Keys::UtilityDecisionsMade,
            ctx.Bb().GetOr(Keys::UtilityDecisionsMade, 0) + 1);
        return Dg::Pop();
    default:
        return Dg::Fail(507);
    }
}
```

Why this is canonical:

- root writes scoring signals to blackboard, then delegates selection to `Dg::Decide(...)`.
- selected utility action runs as a child frame and exits with `Pop()`.
- root completion condition stays explicit (`UtilityDecisionsMade` threshold).

---

## Anti-patterns (explicitly avoid)

- Do **not** hand-roll phase state in blackboard instead of `PcAs<TEnum>()`.
- Do **not** bypass `ctx.Act()` with direct world mutation.
- Do **not** hand-roll deferred timers in frame-local logic when deferred actuation can model it.
- Do **not** manually store utility commitment age/current choice in blackboard.
- Do **not** leave default/unexpected phases without explicit `Dg::Fail(...)` handling.
