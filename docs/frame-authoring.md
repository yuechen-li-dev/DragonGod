# Frame authoring (canonical DragonGod style)

This page is the practical style guide for authoring frame logic against the runtime that exists today.

For runtime semantics details, cross-check [`runtime-truth.md`](./runtime-truth.md).

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
  - suspend current frame execution for tick countdown, then resume phase.
- `Dg::Stay()`
  - continue without changing pc (implemented as continue-with-stay flag).
- `Dg::Push(child, resumePc)`
  - call-like child frame activation; parent resumes later at `resumePc`.
- `Dg::Pop()`
  - return from a child frame.
- `Dg::Replace(frame)`
  - replace top frame atomically.
- `Dg::Complete()`
  - terminal-complete this frame.
- `Dg::Fail(reason)`
  - terminal-fail this frame/runtime.

## Blackboard via `ctx.Bb()`

Canonical usage:

```cpp
ctx.Bb().Set(Keys::Alerted, true);
if (ctx.Bb().GetOr(Keys::Alerted, false)) {
    return Dg::Complete();
}
```

Use typed keys (`BbKey<bool>` / `BbKey<int>`) with stable slot ids.

## Mailbox via `ctx.Mb()`

Canonical usage:

```cpp
dragongod::Message message;
if (!ctx.Mb().ConsumeFront(message)) {
    return Dg::WaitTicks(1, MyPhase::AwaitMessage);
}
```

Use `PeekFront` for inspect-without-consume flows.

## Actuation via `ctx.Act()`

Canonical usage:

```cpp
ctx.Act().Immediate(ActId::OpenDoor);
ctx.Act().Deferred(ActId::RaiseAlarm, 2);
```

Why: preserves deterministic per-tick emission/pending traces and save/restore behavior.

## Utility helpers (`When::...`, `Dg::when`, `Dg::Decide`)

Canonical usage:

```cpp
return Dg::Decide(
    ctx,
    {
        Dg::when(FrameId::UtilityActionCombat, When::Alerted),
        Dg::when(FrameId::UtilityActionReload, When::LowAmmo),
        Dg::when(FrameId::UtilityActionPatrol, When::Always)
    },
    Dg::DecideOptions{ .tieBreak = Dg::TieBreakPolicy::FirstListed });
```

Why: utility commitment/hysteresis/min-commit state is managed in runtime utility memory (not hand-rolled in frame data).

---

## Worked examples aligned to `runtime_nodes.cpp`

### A) Typed phase + `WaitTicks` + mailbox consume

`RootMailboxConsumeFifo` uses:

1. typed phase enum for first vs second consume,
2. `WaitTicks` when mailbox is empty,
3. blackboard writes after each consume,
4. terminal `Complete` after second consume.

This is the canonical “await input over ticks, then commit data” template.

### B) Typed phase + `Stay()` while waiting for later mailbox progression

`RootTypedPhaseMailboxAct` uses:

1. phase 0 consumes a `Signal`, writes blackboard, schedules deferred actuation,
2. phase 1 uses `Dg::Stay()` while no `Alert` is visible,
3. phase 1 completes once `Alert` arrives.

Use this when you want to keep the current logical phase stable while polling for a condition.

### C) Parent/child structure with `Push`/`Pop`

`RootPushChild` + `ChildPop` demonstrates:

- parent pushes child with parent resume pc,
- child performs its local work and pops,
- parent resumes and completes.

Use this as your default subroutine/call-frame structure.

### D) Utility-driven action frame selection

`RootUtilityHighestScore` + utility action frames demonstrates:

- root sets scoring signals,
- root calls `Dg::Decide(...)`,
- chosen action frame runs and pops,
- root completes after expected decision count.

Use this for runtime-managed utility switching instead of ad hoc branching state.

---

## Anti-patterns (explicitly avoid)

- Do **not** hand-roll phase state in blackboard instead of `PcAs<TEnum>()`.
- Do **not** bypass `ctx.Act()` with direct world mutation.
- Do **not** hand-roll deferred timers in frame-local logic when deferred actuation can model it.
- Do **not** manually store utility commitment age/current choice in blackboard.
- Do **not** leave default/unexpected phases without explicit `Dg::Fail(...)` handling.
