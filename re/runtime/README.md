# Runtime analysis

Room for tools that observe the original game under Wine:

- Start script for a 32-bit Wine prefix (see `docs/setup-arch.md`)
- Memory dumps of the global data region (`0x00498000`–`0x0072D000`) at defined points in time
- Savegame diffs (`.GAM`) before/after an action in the game
- later: parity tests (original vs. engine, same starting conditions, N ticks)

Still empty – comes with milestone M3 (core simulation).
