# Trace and replay

This page centralizes per-tick runtime order and replay equivalence checks.

For full semantics, see [`runtime-truth.md`](./runtime-truth.md).

## One runtime tick, in order

For `StackFrameRuntimeSession::RunSingleTick(...)`, the current order is:

1. stop immediately if session is terminal.
2. clear blackboard dirty slots for new-tick write tracking.
3. enqueue scheduled messages matching `nextTick_`.
4. `mailbox.BeginTick()` (staged -> visible).
5. `actRuntime.BeginTick(nextTick_)`.
6. `actRuntime.FlushMatured()` (mature deferred into emitted-now).
7. snapshot visible mailbox for per-tick result output.
8. if stack empty, emit completed tick trace entry and advance tick.
9. otherwise process top frame:
   - tick trace event,
   - enter trace event (first activation only),
   - wait countdown handling if `remainingWaitTicks > 0`, else invoke frame function.
10. apply returned `FrameControl` (`Continue`, `Wait`, `Push`, `Pop`, `Replace`, `Complete`, `Fail`).
11. append per-tick outputs:
    - dirty slots,
    - actuation emitted-now,
    - bounded `TickTraceEntry` (including pending deferred queue).
12. increment `nextTick_`.

## How trace entries relate to ticks

- There is one `TickTraceEntry` per executed tick.
- `TickTraceEntry` includes:
  - tick index,
  - step outcome,
  - stack snapshot,
  - dirty slots,
  - visible mailbox snapshot,
  - utility decision trace entries,
  - emitted actuation this tick,
  - pending deferred actuation queue.

This is the bounded deterministic replay surface.

## Concrete tiny example (serialized tick trace)

`SerializeTickTrace(...)` emits one compact string per `TickTraceEntry`. A tiny run can look like:

```text
tick=0|outcome=0|stack=13,1,1,0;|dirty=4;|mailbox=0,11;|utility=|act=|pending=
tick=1|outcome=2|stack=|dirty=5;|mailbox=1,22;|utility=|act=|pending=
```

How to read this quickly:

- `tick` is tick index.
- `outcome` is numeric `StackRunOutcome` value for that tick entry.
- `stack` entries are `frameId,pc,entered,remainingWaitTicks;`.
- `dirty` is the set of blackboard slot ids written that tick.
- `mailbox` entries are `kind,value;` visible at tick processing.
- `utility`, `act`, and `pending` carry bounded utility and actuation evidence when present.

The exact ids vary by scenario, but this is the shape you assert against in replay/determinism tests.

## Replay comparison

Use:

- `CompareTickTraces(expected, actual)` to detect first divergence.
- `FormatTraceComparison(comparison)` to create bounded human-readable mismatch text.

Comparison behavior:

- checks entry-by-entry equality,
- reports first mismatched index and mismatch reason,
- if lengths differ, reports entry-count mismatch.

## Save/restore and replay equivalence

`Save()` is a between-ticks boundary snapshot. On restore via `StackFrameRuntimeSession(const RuntimeChunk&)`, replay-equivalent runs should match uninterrupted runs when inputs are equivalent.

Persisted state includes stack, blackboard, mailbox, utility memory, deferred actuation queue, scheduled messages, tick index, and prior outcome.

This includes each persisted stack frame's `entered` flag. After restore, a frame that had already entered before `Save()` remains entered, so the runtime does not emit a second enter event just because restore happened.

That is why deferred actuation maturity timing, mailbox staged/visible state, and utility commitment age can remain replay-equivalent across chunk boundaries.
