# Scripts

Content packages will bring Lua scripts that react to what happens in a game and change it through
the game's commands ([ADR 0007](../architecture/0007-extensibility-and-modding.md) section 6, item
6 of [open-work.md](open-work.md)).

**Status:** Part 1 done on 2026-10-27: Lua 5.4.8 is built in (`third_party/lua`), and
`engine/script` runs a script in a sandbox, the rules of which this page describes. Still open:
loading the scripts of a package, the events that call them, the functions that change the game
through commands, the script state in the savegame and messages posted by scripts.

## The language

Scripts are Lua 5.4 text files. Every computer of a multiplayer game and every replay runs the same
scripts with the same events and has to come to the same results, so a script sees only what is the
same everywhere:

| Available | Not available |
|---|---|
| the base functions (`pairs`, `ipairs`, `select`, `type`, `tonumber`, `tostring`, `pcall`, `error`, `assert`, `setmetatable`, `rawget` ...) | `dofile`, `loadfile`, `load`, `collectgarbage`, `warn` |
| the libraries `string`, `table`, `math`, `utf8` and `coroutine` | the libraries `io`, `os`, `package` (`require`) and `debug` |
| `math.random` with the game's random numbers | `math.randomseed`; a script cannot choose the numbers |
| `print`, into the game's log | `string.dump` and binary chunks |

What differs from plain Lua:

- **No addresses.** `tostring` of a table, function or coroutine without `__tostring` gives just its
  type name (`table`, `function`), and so do `print` and `string.format("%s", ...)`. The format
  `%p` is refused. An address differs between computers and runs.
- **No finalizers and weak tables.** `setmetatable` refuses a metatable with `__gc` or `__mode`:
  when the garbage collector runs differs between systems, and with it what they would do.
- **Random numbers from the game.** `math.random()`, `math.random(m)`, `math.random(m, n)` and
  `math.random(0)` work as in Lua 5.4, but draw from the game's generator, so they are the same on
  every computer.
- **The order of `pairs`** over keys that are strings, numbers or booleans is the same on every
  computer for the same steps (the string hash seed is fixed). Over keys that are tables or functions
  it may differ; keep such things in an array and walk it with `ipairs`.
- **Prefer integers.** Integer arithmetic is exact everywhere. Floating point is too for `+`, `-`,
  `*`, `/` and `math.sqrt`, but `^`, `math.exp`, `math.log`, `math.sin` and the other functions of
  the C library may differ in the last bit between systems. Do not let a decision of the game hang
  on such a bit.

## Limits

| What | Limit | When it is reached |
|---|---|---|
| memory of all a game's scripts | 32 MiB | the script stops with "the script needs more than ... bytes of memory" |
| instructions per call from the game | 10 million | the script stops with "the script ran too long"; a `pcall` in the script cannot catch it for good |
| arguments of a call from the game | 200 | the call fails |

A failed script reports its name, its line and the cause, for example `quests:2: no ships`. The game
goes on; the script's globals keep what they held before the error.

The instruction limit counts the script's own instructions; time spent inside a single library
function, such as a pattern search of `string.find` on a long text, is not counted.
