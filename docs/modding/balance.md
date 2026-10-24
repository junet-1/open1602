# Balance

Some numbers of the game's balance are values a content package can change instead of constants in
the code ([ADR 0007](../architecture/0007-extensibility-and-modding.md): a mod changes data, not
code). Their defaults are the original's.

**Status:** Done are the values below, saving them with the game and the file `balance.toml` that
changes them. More values follow as they are needed; prices, production times and the needs of the
inhabitants are in the original's `.cod` files and stay there.

## The values

| Table | Field | Default | Limits | Meaning |
|---|---|---|---|---|
| `trader` | `max_held` | 50 | 1–200 | tons of a ware the free trader holds at most |
| `trader` | `max_buy` | 10 | 1–200 | tons of a ware he buys at most in one visit |
| `trader` | `restock_seconds` | 60 | 1–3600 | seconds until he takes on tools or iron ore again when he has few |
| `castle` | `interval_seconds` | 10 | 1–600 | seconds of a castle's training step (one more when it has nothing to do) |
| `castle` | `unload_range` | 6 | 1–16 | tiles from a ship within which soldiers land |
| `disasters` | `fire_spread` | 19 | 0–128 | chance of 128 per epoch that a fire spreads to a building nearby; 0 never |
| `disasters` | `plague_radius` | 4 | 0–16 | tiles a plague reaches to infect a neighbouring house; 0 never |

A game keeps the values it started with: they are saved with it (section `BALA` of
[o1602-save.md](../formats/o1602-save.md), written only when they differ from the original's), so a
mod installed or removed later does not change a running game, and every computer of a multiplayer
game plays with the host's.

## Where a mod's changes are

```
<user data folder>/mods/<package>/balance.toml
```

The user data folder and the package's name are as for [messages](messages.md#where-a-mods-messages-are).
Packages are read in the order of their names; a later one can change what an earlier one set. The
changes apply to the games started afterwards: new games and scenarios or savegames of the original.
Problems are written to the log (standard error) with the file and the line.

## The file

`balance.toml` is the same part of [TOML](https://toml.io) as `messages.toml`. Each table changes
single fields; a field that is not given keeps its value:

```toml
# A harder game: the trader buys less, fires spread more easily.
[trader]
max_buy = 5

[disasters]
fire_spread = 40
```

A value outside its limits or that is no whole number is passed over with a warning, as are unknown
tables and fields. A file that is no valid data file as a whole changes nothing.

## In the code

`engine/sim/include/o1602/sim/balance.hpp`: `sim::Balance` with the values, `kBalanceFields` with
their names and limits, `apply_balance_file`; `World::balance()` is what the simulation reads.
