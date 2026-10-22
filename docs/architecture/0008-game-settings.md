# 0008 – Game settings

Status: accepted · 2026-10-17

## Context
Later titles of the Anno series let you configure the endless game far more precisely than Anno 1602:
starting capital, opponents, disasters, refunds on demolition and much more. Open1602 should be able to do
this as well. Until now, `sim::Rules` only had switches for deviations from the original
(ADR 0006).

## Decision
1. **All settings of a game live in `sim::Rules`**, whether they switch off a quirk of the
   original or offer a new choice. There is no second place for them.
2. Every setting has an **original value**, which `Rules::original()` returns, and a
   default value for Open1602. Where it is a free choice and not a bug of the original,
   the default is the behaviour of the original.
3. Settings belong to the **savegame** and to the **multiplayer session**. They are chosen before the
   game starts; if they change during a game, then only via a command
   (ADR 0004), so that all machines compute the same thing.
4. The simulation reads settings only from `Rules`. The interface, the game start dialog and the
   command line fill in `Rules` and do not know the rules themselves.
5. New settings are documented in the code at the place that evaluates them, and in the
   behaviour description (`docs/re/*.md`) in the section "Open1602 and the original".

## First settings
| Setting | Values | Original | Default |
|---|---|---|---|
| Refund on demolition (`demolish_refund`, `refund_percent`) | none, partial (1–100 %, preset 50 %), full | none | none |
| Ships per player (`max_ships_per_player`, L-001) | number, 0 = unlimited | 33 | 100 |
| Units per player (`max_army`, L-008) | number, 0 = unlimited | 100 | 250 |
| Guarantee raw materials (`guarantee_resources`, S-019) | yes/no | no | yes |
| Highest game speed (`max_game_speed`, L-004) | factor | 4 | 8 |
| Droughts (`droughts`) | as the game (difficulty), off, rare, normal, frequent | as the game | as the game |
| Volcanoes (`volcanoes`) | as the game (setting), off, on | as the game | as the game |
| Free trader prices (`trader_prices`, S-077) | as usual (about half the base price), by the market (as the original intends) | as usual | as usual |
| Computer players (`classic_ai`, S-097 to S-099) | improved, as the original (with its bugs in settling, counting and site search, so they stall after about two hours) | as the original | improved |
| Custom world (`WorldSettings`, new game) | size 200–2000 per side, players, number of islands, island sizes, villages, volcano islands, starting areas separate/shared | the three fixed maps | off; on: 500 × 350, 4 players, normal, mixed, separate |

## Consequences
The Open1602 savegame has its own section for the settings (`RULE` in the chunk
`O1602`, done). Savegames of the original get the original's values for the choice points; this
is satisfied because the default of every free choice is the original. The exception is
`classic_ai` (decided 2026-10-22): the original's computer players are a choice for those who want
its difficulty, but their bugs stay fixed by default, also in savegames of the original. The endless game has a
start dialog after name and banner (`engine/app/src/game_settings_menu.cpp`, since 2026-10-20): the
settings in groups (economy, limits, disasters, world, opponents), as in later Anno titles, each a button that
cycles its values, with presets for the original's rules and Open1602's defaults; the last choice is
kept in `settings.cfg` (`endless.*`). The command line (`--refund`, `--droughts`, `--volcanoes`, `--ai`)
still sets them for games started from it.
