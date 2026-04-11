# DragonGod architecture

This document explains what DragonGod is, why it is shaped the way it is, and how its parts fit together. It is intended for readers who want to understand the system before writing frames or tests. Practical authoring guidance lives in `frame-authoring.md`; runtime semantics live in `runtime-truth.md`.

---

## What DragonGod is

DragonGod is a deterministic, tick-driven AI behavior runtime written in portable C++23.

Its purpose is to execute agent behavior in a way that is:

- **auditable** — every tick produces a bounded, inspectable record of what happened.
- **replayable** — a saved snapshot plus any external inputs fully reconstruct prior execution.
- **portable** — no platform dependencies, no heap-heavy abstractions, no virtual dispatch as a design habit.
- **testable by construction** — the tick model and trace surface exist specifically so that correctness can be verified without mocking or instrumentation layers.

DragonGod is not a scripting language, not a visual behavior editor, and not an LLM orchestration wrapper. It is a low-level kernel for structured agent execution that other systems can be built on top of.

---

## The core idea: frames on a stack

The fundamental unit of behavior in DragonGod is a **frame** — a plain C++ function with the signature:

```cpp
[[nodiscard]] FrameControl SomeFrame(FrameCtx& ctx);
```

A frame reads state from its context (`ctx`), performs whatever logic is appropriate for its current phase, and returns a **control value** that tells the runtime what to do next.

Frames are organized on a **call stack**. The runtime executes exactly one frame per tick — the one currently at the top of the stack. Control values determine how the stack evolves:

- `Continue` — stay on this frame, advance to a new phase next tick.
- `WaitTicks` — stay on this frame, skip N ticks, then advance to a phase.
- `Push` — schedule a child frame above this one; the child becomes the new top frame and begins executing on the next tick.
- `Pop` — return from this frame; parent resumes.
- `Replace` — swap this frame for another without return.
- `Complete` — this frame is done successfully; pop it.
- `Fail` — this frame has failed; terminate the runtime.

This is deliberately analogous to a call stack in a program. `Push`/`Pop` are call and return. `Replace` is a tail call. `Complete` is a clean return. The model is explicit enough to reason about locally and structured enough to serialize.

---

## Why a function with a program counter, not a coroutine

A natural alternative would be to use C++ coroutines or generators for frame logic, letting the language runtime handle suspension points. DragonGod does not do this.

The reason is serialization. Coroutine state is owned by the compiler — the suspension point, local variables, and continuation are not accessible to author code. Saving and restoring a coroutine across process boundaries (or across save-game boundaries) requires either compiler-specific hacks or a second representation of the same state.

DragonGod instead makes the program counter **explicit and author-visible**. Each frame function receives its current `pc` through `ctx.Pc()` or `ctx.PcAs<TEnum>()` and returns a new `pc` (embedded in the control value) as part of every branch. The runtime stores `pc` as a plain integer in the stack entry. Serializing it is trivial — it is just a number.

The cost is that frame authors manage phase transitions manually. The benefit is that frame state is always fully serializable, inspectable, and reproducible.

---

## The tick model

A **tick** is one unit of runtime advancement. Each tick follows a fixed sequence:

1. Clear blackboard dirty tracking for this tick window.
2. Deliver any externally-scheduled messages whose tick has arrived.
3. Stage new mailbox messages into the visible queue (`BeginTick`).
4. Mature any deferred actuation requests whose due tick has arrived (`FlushMatured`).
5. Execute the top frame — or, if the stack is empty, emit a completed tick entry and check if deferred actuation is still pending.
6. Apply the returned control (updating the stack and frame state).
7. Append a `TickTraceEntry` capturing the full observable state of this tick.
8. Advance the tick counter.

The tick is strictly sequential. There is no concurrency within a tick. Every observable effect of a tick — blackboard writes, actuation emissions, mailbox consumption, stack changes — is captured in the tick trace before the next tick begins.

This fixed sequence is the reason the system is replayable: given the same initial state and the same sequence of external inputs, the same sequence of ticks always produces the same sequence of trace entries.

---

## The four state surfaces

Every frame has access to four runtime state surfaces through its context:

### Blackboard (`ctx.Bb()`)

The blackboard is a typed key-value store. Keys are `BbKey<T>` values with a name and a stable integer slot id. Currently supported value types are `bool` and `int`.

The blackboard serves as the shared working memory for frame logic across ticks. A frame writes values it wants to remember or communicate; later ticks (or child frames) read them.

Dirty tracking marks which slots were written during the current tick. This is available in the tick trace and useful for asserting that only expected writes occurred.

### Mailbox (`ctx.Mb()`)

The mailbox is a FIFO queue of `Message` values (kind + integer payload). It has two internal queues: visible (consumable this tick) and staged (enqueued this tick, visible next tick). This staging prevents frames from seeing messages they themselves enqueued in the same tick, making the flow direction explicit.

The mailbox is the primary input surface for external events arriving from outside the runtime.

### Actuation (`ctx.Act()`)

Actuation is the output surface. Frames do not directly mutate external world state — they emit typed `ActId` requests. The runtime collects these as `ActRequest` records in the tick trace.

Actuation can be immediate (emitted in the current tick) or deferred (scheduled for a future tick). Both forms are explicit runtime data rather than direct calls, which is why outputs remain traceable across the full tick record and why pending deferred effects survive save/restore boundaries correctly.

### Utility memory (implicit, accessed via `Dg::Decide`)

Utility decision state — which action is currently committed, how long it has been committed, what its last scored value was — is stored in a separate utility memory store managed by the runtime. Frames do not access this directly; it is read and updated automatically when `Dg::Decide(...)` is called. This prevents utility commit state from leaking into blackboard keys or frame-local logic.

---

## Utility decisions

DragonGod includes a utility AI decision layer built directly into the frame execution model.

Authors express candidates through `when(target, scorer)` and selection through `Dg::Decide(ctx, candidates, options)`. The `Decide` call scores each candidate against its consideration function, applies stability policies (hysteresis, minimum commit window, tie-break), selects a winner, and pushes that frame as a child. When the child completes and pops, the parent resumes at the same phase — which re-evaluates the decision on the next tick.

This creates a natural "intent loop": the decision phase is the parent frame's long-running state, and each selected action is a short-lived child. The utility layer handles commitment memory; frame authors only write consideration functions and action frames.

Consideration functions are plain `float (*)(const FrameCtx&)` callables that return a score in `[0, 1]`. They receive a const context because scoring must be read-only — scoring a candidate must not change world state.

---

## Persistence: the chunk model

DragonGod saves and restores state through **chunks** — plain data structures that capture a complete runtime snapshot at a tick boundary.

A `RuntimeChunk` contains:

- the stack (each frame's id, current pc, entered flag, remaining wait ticks)
- the full blackboard (all key-value pairs)
- the mailbox (both visible and staged queues)
- utility memory (committed targets, ages)
- pending deferred actuation (scheduled future effects)
- scheduled external messages (future mailbox deliveries)
- the current tick index and last outcome

This is everything needed to restore a session to exactly the state it was in and continue running forward. Notably absent: coroutine state, heap pointers, or any runtime-internal opaque objects. The chunk is pure data.

Restore is cold reconstruction from serialized runtime state: frames resume from their serialized `pc` and persisted entry and wait state, rather than from hidden coroutine state. A frame that had already entered before `Save()` remains marked as entered after restore, so the runtime does not emit a spurious second enter event. Because frame functions are pure functions of their `FrameCtx` arguments and stored `pc`, resuming at a given `pc` with equivalent state produces identical behavior to what would have happened had the session run continuously. This is the structural guarantee that makes save/restore replay-equivalent.

---

## Trace and replay

Every tick appends a `TickTraceEntry` to the run result. A trace entry captures:

- tick index and outcome
- stack snapshot (frame ids, pc values, wait state)
- blackboard dirty slots for this tick
- visible mailbox snapshot
- utility decision records (candidates, scores, commitment state)
- actuation emitted this tick
- pending deferred actuation queue

Two runs with the same initial state and equivalent inputs produce identical `TickTraceEntry` sequences. `CompareTickTraces` checks this precisely. `FormatTraceComparison` produces a bounded diagnostic string suitable for test artifact output.

The trace is the primary test surface for anything beyond terminal-outcome assertions. It provides evidence that the right behavior happened at the right time, not just that execution eventually terminated correctly.

---

## Marionette

DragonGod includes its own test harness, Marionette, rather than depending on an external framework.

Marionette provides:

- `FACT` / `THEORY` for test registration
- `ASSERT_*` macros for inline correctness checks
- `WriteTextArtifact` for materializing diagnostic files alongside test output
- `BENCHMARK` / `BENCHMARK_WITH_ITERATIONS` for performance measurement, separate from correctness tests
- the Doom module for subprocess-isolation of intentional abnormal-termination scenarios

Marionette exists partly to preserve the project's low-dependency, portable shape and partly to keep the full testing contract small enough for humans and LLMs to understand without navigating a large framework.

The artifact system is particularly important for DragonGod testing: when a `CompareTickTraces` call fails, the formatted trace comparison can be written to disk as a text artifact, giving a human or LLM author a concrete, inspectable record of the divergence.

---

## What DragonGod is not (by design)

**Not a scripting VM.** Frame logic is plain C++ compiled into the binary. There is no bytecode, no interpreted language, no embedded parser. Behavior is authored in the same language as the runtime.

**Not a visual graph editor.** Frame relationships are expressed by `Push`/`Pop`/`Replace` control values in code, not by visual node connections. This is a deliberate tradeoff: code is diffable, reviewable, and navigable by LLM and human authors; visual graphs require dedicated tooling.

**Not an async/coroutine system.** `WaitTicks` is not `co_await`. Deferred actuation is not a future or promise. The tick model is synchronous and sequential. Apparent "asynchrony" — waiting for a mailbox message, waiting for a deferred action to mature — is expressed as explicit phase transitions across ticks.

**Not opinionated about what frames do.** DragonGod provides the execution kernel: tick loop, stack, state surfaces, persistence, trace. What frames represent — NPC behavior states, dialogue flows, process control steps, industrial automation logic — is entirely up to the author. The kernel is domain-agnostic.

---

## Current shape limitations

DragonGod's architecture is complete in its core model, but some surfaces are still narrow in their current implementation:

- **Frame registration is internal/static.** There is not yet a public API for registering an external frame pack. Authors currently add frames by editing `runtime.h` and `runtime_nodes.cpp` directly. The architecture supports an external registration surface; it has not been built yet.
- **Blackboard supports `bool` and `int` only.** The type system is intentionally limited for now. The slot-based storage model is designed to accommodate additional types without structural changes.
- **Message payloads are `kind + int value`.** The `Message` struct has two fields. Richer payload types would require extending the struct and updating the serialization surface.

These are current-state constraints, not architectural limits.

---

## Architecture in one sentence

DragonGod runs a stack of explicitly-phased frame functions tick by tick, exposes blackboard/mailbox/actuation as typed state surfaces, serializes everything through a chunk boundary, and records every tick as a bounded trace entry — so that behavior is auditable, replayable, and testable without instrumentation.
