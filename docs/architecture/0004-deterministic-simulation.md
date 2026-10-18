# 0004 – Deterministic, command-based simulation

Status: accepted · 2026-10-16

## Decision
1. The simulation (`engine/sim`) runs in **fixed ticks** and is **bit-exactly deterministic**:
   - integer/fixed-point arithmetic only (no `float`/`double` in the simulation state),
   - its own seedable random generator in the game state,
   - no iteration over unordered containers, no pointers as sort keys,
   - no dependency on system time, frame rate or platform.
2. Every player action is a serialisable **command** (`Command`) that is applied at a defined
   tick. Presentation and UI only read the state.
3. A **hash** is computed over the state every tick.

## Rationale
- **Multiplayer** via lockstep: only commands are transmitted; the hash detects desyncs immediately.
- **Replays, tests, headless servers** come almost for free.
- **Stability over long sessions**: entities in pools with stable IDs instead of scattered allocations;
  no allocation in the tick hot path.

## Consequences
Values with decimal places in `.cod` files (`Nahrung: 1.3`) are converted to fixed point when loaded.
Rendering interpolates between ticks and may run at any frame rate.
