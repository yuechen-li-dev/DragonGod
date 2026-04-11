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
- can be any plain function of that shape; `When::Alerted`, `When::LowAmmo`, and `When::Always` are built-in examples, not special language features.

### Utility decision call site shape

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
