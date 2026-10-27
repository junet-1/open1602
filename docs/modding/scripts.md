# Scripts

Content packages bring Lua scripts that react to what happens in a game, and later change it through
the game's commands ([ADR 0007](../architecture/0007-extensibility-and-modding.md) section 6, item
6 of [open-work.md](open-work.md)).

**Status:** Done on 2026-10-27: Lua 5.4.8 is built in (`third_party/lua`), every package may bring a
`script.lua`, which the game runs in a sandbox at the end of every tick with what happened in it, a
script reads the players and posts the package's messages, and its table `state` is saved with the
game. Still open: changing the game through commands (gold, goods, victory and defeat) and more
events.

## The script

```
<user data folder>/mods/<package>/script.lua
```

A package has at most one script, a UTF-8 text file. When a game starts or is loaded, the game runs
the top level of each package's script, in the packages' load order, each in a sandbox of its own:
what one package's script defines, another's does not see. Then the game calls the functions the
script defines when something happens:

| Function | When |
|---|---|
| `on_start()` | at the first tick of a new game (or of a game that ran without the script so far) |
| `on_second(seconds)` | every second of game time; `seconds` since the game began |
| `on_building_placed(e)` | a building was placed: `e.building` (its number in `haeuser.cod`), `e.island`, `e.x`, `e.y` and `e.player`, the owner of its city (nil without one) |
| `on_house_changed(e)` | a house rose, fell back or was abandoned: `e.kind` (`"upgraded"`, `"downgraded"`, `"abandoned"`), `e.class` (the class after the change), `e.island`, `e.x`, `e.y` |
| `on_player_won(player)` | a player accomplished the mission |
| `on_player_defeated(player, by)` | a player is out; `by` is who defeated them (nil: nobody) |
| `on_game_over(player)` | the game ended with `player`'s mission |

The game calls them inside the tick, on every computer of a multiplayer game alike, so a script
needs nothing of its own to stay in step with the others.

## What a script reads and does

The table `game` holds what the game offers a script:

| Function | What it gives or does |
|---|---|
| `game.package` | the package's id |
| `game.time()` | the game time in whole seconds |
| `game.players()` | the players' numbers, a list starting with 0 |
| `game.player(n)` | player `n` as a table: `id`, `name`, `role` (`"human"`, `"computer"`, `"neutral"`), `money`, `inhabitants`, `playing` (still in the game), `won`; nil for no such player |
| `game.message(id, values, player)` | posts a message of the package's `messages.toml` ([messages.md](messages.md)); `id` is its name there, or `"<package>:<name>"` for another package's; `values` fills its placeholders (a table of names, may be nil); `player` is the only player to see it (nil: everyone) |

```lua
-- mods/lighthouse/script.lua
state = state or {lit = 0}

function on_building_placed(e)
  if e.building == 20311 and e.player then  -- a building's number in haeuser.cod
    state.lit = state.lit + 1
    game.message("lit", {name = game.player(e.player).name, count = state.lit})
  end
end
```

## Saved with the game

The game saves the table `state` of each script with the game, and the script's random numbers go
on where they stopped. When the game is loaded, the game runs the script's top level again and then
sets `state` to what was saved; `on_start` is not called again. So everything a game has to remember
belongs in `state`; other globals are made anew from the top level, and a computer that joins a
multiplayer game later starts from `state` too.

`state` keeps booleans, numbers, strings and tables of them (keys that are booleans, numbers or
strings). Functions and coroutines in it are not saved, nor tables nested deeper than 32. The
fingerprint that computers of a multiplayer game compare includes `state`.

## Errors

A failed script is reported in the status line and in the log with its package, line and cause; the
game goes on. A script whose top level fails is not called for the rest of the game, and neither is
one that failed 10 times. What a script prints goes to the log.

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
| memory of each package's script | 32 MiB | the script stops with "the script needs more than ... bytes of memory" |
| instructions per call from the game (the top level, each `on_...`) | 10 million | the script stops with "the script ran too long"; a `pcall` in the script cannot catch it for good |
| arguments of a call from the game | 200 | the call fails |

A failed script reports its name, its line and the cause, for example `quests:2: no ships`. The game
goes on; the script's globals keep what they held before the error.

The instruction limit counts the script's own instructions; time spent inside a single library
function, such as a pattern search of `string.find` on a long text, is not counted.
