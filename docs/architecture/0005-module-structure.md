# 0005 – Module structure and dependency rules

Status: accepted · 2026-10-16

```
            core            Logging, files, errors (Result), text, fixed point, RNG
              ▲
            assets          Loaders for original formats (no platform/graphics dependency)
              ▲
             sim            Deterministic simulation (world, islands, buildings, economy, AI …)
           ▲     ▲
         net   script       Lockstep multiplayer / modding
           ▲
platform ◄─ render, audio, ui ◄─ app
```

Rules:
- Arrows point to dependencies; cycles are forbidden.
- `sim` **never** depends on `platform`, `render`, `audio`, `ui` (headless capability, determinism).
- Every module is its own CMake library `o1602::<module>` with `include/o1602/<module>/` as its
  public interface; internals live in `src/`.
- Namespace `o1602::<module>`.

Already done: `core`, `assets`, `sim` (island grid), `render` (software renderer). The remaining modules are described in `engine/<module>/README.md`
and will be created with their milestones.
