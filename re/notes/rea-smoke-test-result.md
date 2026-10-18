# rea feasibility test – result

Date: 2026-10-16 · rea 6.5.0 · Ghidra 12.1.2 · JDK 21.0.12 · CachyOS (Linux x64)

**Result: passed.** rea fully analyses `1602.exe` (PE32, x86) through the Ghidra backend.
All check steps of `re/scripts/rea-smoke-test.sh` ran without errors (exit 0).

| Check | Expectation (from objdump analysis) | rea/Ghidra |
|---|---|---|
| Format/architecture | PE32 / i386 | `format: pe`, `architecture: x86` ✅ |
| Size | ~2000 functions estimated | 1896 functions, 1101 defined strings |
| WinMain `0x004081D0` | called from the entry point, calls `Game_Init` | Caller `entry` (`0x00495CE4`, call at `0x00495E13`); callees include `GetMessageA`, `PeekMessageA`, `TranslateAcceleratorA`, `DispatchMessageA` and `0x00494870` ✅ |
| Text.cod loader `0x0048F8D0` | read file, negate every byte | Decompilation shows `sprintf("%sText.cod")`, `ReadFile`, negation loop, globals `0x005B6FC0`/`0x005B6050`; then a parser for `[section]` blocks ✅ |
| xrefs `haeuser.cod`, search `stadtfld`, instruction `0x0048F950` | | Exit 0 (details in the local log) |

## Consequences

- rea is the main tool for reverse engineering (see `docs/re/README.md`).
- The rea tools are also available via Claude Desktop in Claude sessions that are connected to the
  computer.
- rea works with an ephemeral Ghidra database. Names and comments therefore stay in
  `re/symbols/*.csv` as the source of truth. Renaming in the session (`set_addresses_names`) failed
  over the desktop connection with a format error; `annotate_native_function` is the alternative.
- The log (`rea-smoke-test-*.log`) contains decompiled code and is not checked in.
