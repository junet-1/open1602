# Values of buildings and figures

A content package can change single values of the original's buildings (`haeuser.cod`) and
figures (`figuren.cod`: carriers, soldiers, ships) without shipping changed `.cod` files
([ADR 0007](../architecture/0007-extensibility-and-modding.md) section 2, item 2 of
[open-work.md](open-work.md)).

**Status:** Done are the fields below, the files `buildings.toml` and `figures.toml` and saving
the values with the game. Buildings and figures of a package's own, and names instead of numbers
for the original's, follow with the next steps of the content registry.

## The files

```
<user data folder>/mods/<package>/buildings.toml
<user data folder>/mods/<package>/figures.toml
```

Each table is named after an entry of the original, `["anno1602:<number>"]`: for a building its
`HAUS` Id from `haeuser.cod` (`20503` is the bakery), for a figure its `FIGUR` number from
`figuren.cod`. The number may be decimal or `0x` hex. A field that is not given keeps its value.

```toml
# buildings.toml: a bakery that makes twice the bread and reaches further.
["anno1602:20503"]
produce = 2
radius = 10
```

```toml
# figures.toml: tougher swordsmen, bigger ships.
["anno1602:1"]
health = 30

["anno1602:21"]
cannons = 10
```

Packages are read in their [load order](packages.md#load-order); a later one can change what an
earlier one set. A value outside its limits or that is no whole number is passed over with a
warning, as are unknown fields and tables of another package or of a number the data does not
have. A file that is no valid data file as a whole changes nothing. Problems go to the log with
the file and the line.

The buildings' tables may also stand in `balance.toml`, as they did before `buildings.toml`
existed ([balance.md](balance.md#the-buildings-values)).

## The buildings' fields

| Field | `haeuser.cod` | Limits | Meaning |
|---|---|---|---|
| `produce` | `Prodmenge` | 1–64 | tons made in one cycle |
| `raw` | `Rohmenge` | 1–64 | tons of the first raw material one cycle uses |
| `raw2` | `Workmenge` | 1–64 | tons of the second raw material one cycle uses |
| `interval` | `Interval` | 1–3600 | length of a cycle, in the original's units |
| `max_stock` | `Maxlager` | 1–255 | tons the building stores at most |
| `running_cost` | `Kosten` (first) | 0–1000 | gold per minute while it works |
| `idle_cost` | `Kosten` (second) | 0–1000 | gold per minute while it is idle |
| `cost_money` | `Money` | 0–100000 | gold to build it |
| `cost_tools` | `Werkzeug` | 0–255 | tons of tools to build it |
| `cost_wood` | `Holz` | 0–255 | tons of wood to build it |
| `cost_bricks` | `Ziegel` | 0–255 | tons of bricks to build it |
| `cost_cannons` | `Kanon` | 0–255 | tons of cannons to build it |
| `radius` | `Radius` | 1–32 | tiles its carriers and workers reach |
| `workers` | `Arbeiter` | 0–1000 | workers in the city's statistics |
| `max_idle` | `Maxnorohst` | 1–1000 | cycles without raw material until it reports a shortage |
| `health` | `Maxenergy` | 0–10000 | hit points against attacks; 0: the first hit destroys it |

## The figures' fields

`figuren.cod` gives soldiers and ships the same fields; each value below applies to whichever the
figure is.

| Field | `figuren.cod` | Limits | Meaning |
|---|---|---|---|
| `speed` | `Speed` | 1–10000 | walking or sailing speed, as the number in `figuren.cod` |
| `max_carry` | `Maxtrag` | 1–64 | tons a carrier or cart takes at once |
| `work_time` | `Worktime` | 1–600000 | milliseconds of one work step (harvesting a field, felling a tree) |
| `health` | `Maxenergy` | 1–10000 | hit points of a soldier or ship |
| `hit` | `Hitpoint` | 0–1000 | damage of one blow or shot |
| `range` | `Shotradius` | 1–32 | tiles a shot reaches |
| `shot_time` | `Shottime` | 1–60000 | milliseconds between two shots of a ship; for a soldier from the shot to the hit |
| `cannons` | `Maxkanon` | 0–100 | cannons a ship carries at most |
| `price` | `Preis` | 0–100000 | what a ship is worth (sales, computer players) |
| `material` | `Rohmenge` | 0–255 | tons of material a shipyard builds the ship from; of weapons a castle needs for the unit |

## In the game

The values apply to the games started afterwards: new games and scenarios or savegames of the
original. A game keeps the values it started with (sections `BLDV` and `FIGV` of
[o1602-save.md](../formats/o1602-save.md)), so a package installed or removed later does not change
it, and every computer of a multiplayer game plays with the host's. Each game starts from the
original's tables and sets its own values on them; the next game starts from the original's again.

## In the code

`engine/sim/include/o1602/sim/balance.hpp`: `Balance::buildings` with `kBuildingFields` and
`Balance::figures` with `kFigureFields`; `apply_buildings_file` and `apply_figures_file` read the
files, `apply_building_values` and `apply_figure_values` write a game's values into its tables.
