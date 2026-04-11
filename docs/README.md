# DragonGod docs index

Use this page to pick the right document quickly.

## Read order for new contributors

1. [`getting-started.md`](./getting-started.md) — First frame + first test path against today’s internal registry workflow.
2. [`frame-authoring.md`](./frame-authoring.md) — Canonical authored frame shape, helpers, and anti-patterns.
3. [`trace-and-replay.md`](./trace-and-replay.md) — Tick ordering and bounded replay comparison surface.
4. [`marionette.md`](./marionette.md) — Harness macros, benchmark/doom modes, artifacts, and CLI behavior.
5. [`runtime-truth.md`](./runtime-truth.md) — Current semantic source of truth for runtime behavior.

## Page map (all current docs pages)

- [`getting-started.md`](./getting-started.md): onboarding path for adding one frame and one Marionette test in the current codebase shape.
- [`frame-authoring.md`](./frame-authoring.md): canonical frame authoring patterns grounded in `runtime_nodes.cpp`, including typed phases and utility decisions.
- [`trace-and-replay.md`](./trace-and-replay.md): per-tick lifecycle ordering and what gets compared for deterministic replay.
- [`marionette.md`](./marionette.md): Marionette test harness reference, assertions, artifacts, benchmark execution, and doom module usage.
- [`runtime-truth.md`](./runtime-truth.md): implementation-level runtime semantics and guardrails; prefer this when prose docs disagree.

## Runtime truth vs author guidance

- **Runtime semantic truth:** `runtime-truth.md` (and then code/tests if any mismatch remains).
- **Authoring guidance:** `getting-started.md` + `frame-authoring.md`.
- **Harness usage:** `marionette.md`.
- **Determinism/replay focus:** `trace-and-replay.md`.
