# Samples

DragonGod samples live outside `src/DragonGod/` so production runtime code and pressure-testing experiments stay separated.

Samples are where we stress assumptions, measure friction, and validate integration paths without casually reshaping runtime semantics.

Current samples:

- `dragon_router/`: scaffold for a bounded router/control-loop experiment.
- `dragon_hft/`: scaffold for a bounded market-reaction/order-decision/stale-recovery experiment.
