# DragonGod getting started (current repo workflow)

## Read this first: current registration limitation

DragonGod frame registration is currently **internal/static**.

That means your first authored frame today requires editing core runtime files directly:

- `src/DragonGod/runtime.h` (add `FrameId` and usually a `StackScriptScenario` value)
- `src/DragonGod/runtime_nodes.cpp` (add frame function(s), scenario root mapping, and `BuildFrameRegistry()` entry)

There is not yet an external “register my own frame pack” public API.

---

## Header include guidance (today)

For new authored tests/call sites, include:

- `#include "../../../src/DragonGod/m1_single_frame.h"`

Why this is the current default:

- `m1_single_frame.h` is the compatibility umbrella used by the existing DragonGod tests.
- It currently includes `runtime.h`, so runtime types (`StackFrameRuntime`, `FrameRunResult`, enums, context-facing types) are still available through that include path.
- Using the same include as in-repo tests keeps new code aligned with current production/compatibility wiring while the repo still carries milestone-era file naming.

You can include `runtime.h` directly in runtime-internal code, but for new author-facing tests/examples in this repository, prefer `m1_single_frame.h` to match the active test suite pattern.

---

## Minimal, complete first-frame path

This is the smallest copy-pasteable path aligned with current code.

### 1) Add a frame id and scenario enum values

In `src/DragonGod/runtime.h`, add one `FrameId` and one `StackScriptScenario` entry.

If this new frame will emit actuation (`ctx.Act().Immediate(...)` or `ctx.Act().Deferred(...)`), also add the needed `ActId` enum entry in the same file. This extension step is analogous to adding a new `FrameId`; otherwise `ActId::YourNewAction` will not exist at compile time.

```cpp
enum class FrameId
{
    // ...existing ids...
    RootHelloTwoPhase
};

enum class ActId
{
    // ...existing ids...
    YourNewAction
};

enum class StackScriptScenario
{
    // ...existing scenarios...
    HelloTwoPhaseComplete
};
```

### 2) Map scenario -> root frame

In `ScenarioRootFrameImpl(...)` inside `src/DragonGod/runtime_nodes.cpp`:

```cpp
if (scenario == StackScriptScenario::HelloTwoPhaseComplete) {
    return FrameId::RootHelloTwoPhase;
}
```

### 3) Add a typed phase enum + tiny frame

In `src/DragonGod/runtime_nodes.cpp` (inside the `nodes` namespace):

```cpp
enum class RootHelloTwoPhasePhase : std::uint32_t
{
    Start,
    Finish
};

[[nodiscard]] FrameControl RootHelloTwoPhase(FrameCtx& ctx)
{
    switch (ctx.PcAs<RootHelloTwoPhasePhase>()) {
    case RootHelloTwoPhasePhase::Start:
        return Dg::Continue(RootHelloTwoPhasePhase::Finish);
    case RootHelloTwoPhasePhase::Finish:
        return Dg::Complete();
    default:
        return Dg::Fail(9000);
    }
}
```

### 4) Register the frame in the internal registry

In `BuildFrameRegistry()` inside `src/DragonGod/runtime_nodes.cpp`:

```cpp
registry.Add(FrameId::RootHelloTwoPhase, &nodes::RootHelloTwoPhase);
```

### 5) Run it for ticks

From any test (or other C++ callsite):

```cpp
const dragongod::StackFrameRuntime runtime;
const dragongod::FrameRunResult run =
    runtime.RunForTicks(dragongod::StackScriptScenario::HelloTwoPhaseComplete, 4);
```

Expect terminal completion in 2 ticks for this minimal frame.

What to inspect from `run` first:

- `run.finalOutcome` for completion/failure/wait status,
- `run.finalBlackboard` for authored state results,
- `run.tickTrace` / `run.actuationByTick` for timing-sensitive deterministic assertions,
- `run.trace` only when you need lower-level frame event sequence details.

If you need continuation/stepping or save/restore, switch from `StackFrameRuntime` to `StackFrameRuntimeSession` and call `RunForTicks(...)` in legs with `Save()`/restore between legs.

### 6) Add one Marionette FACT

Create a test in `tests/DragonGod.Tests/...`:

```cpp
#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/m1_single_frame.h"

FACT(HelloTwoPhase_Completes)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run =
        runtime.RunForTicks(dragongod::StackScriptScenario::HelloTwoPhaseComplete, 4);

    ASSERT_TRUE(
        run.finalOutcome == dragongod::StackRunOutcome::Completed,
        "hello two-phase frame should complete");
}
```

---

## Where to look for canonical in-repo examples

- Typed phase + mailbox + `Stay()`: `RootTypedPhaseMailboxAct`.
- Blackboard set/read flow: `RootSetThenReadBlackboard`.
- Parent/child push/pop: `RootPushChild` and `ChildPop`.
- Utility decisions: `RootUtilityHighestScore` and related utility roots.
- Actuation immediate/deferred: `RootActImmediateDeferred`.

All are in `src/DragonGod/runtime_nodes.cpp` and exercised by tests in `tests/DragonGod.Tests/`.

---

## Common first-day mistakes (avoid these)

- Don’t store your own phase counter in blackboard; use `ctx.PcAs<TEnum>()`.
- Don’t bypass frame registration in `BuildFrameRegistry()`; unregistered frames fail at runtime.
- Don’t mutate external world state directly from frame logic; use `ctx.Act()` request emission.
- Don’t treat `Wait` as terminal; it is a non-terminal outcome.
