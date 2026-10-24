# Limits of the data files

Open1602 reads the original's `haeuser.cod`, `figuren.cod` and `.gad` layouts, and modded copies
of them. A modded file can hold any number, so the loaders check the ones that index tables or
size memory and refuse or cut what lies outside. These are the limits.

| File | Value | Limit | Outside the limit |
|---|---|---|---|
| `haeuser.cod` | `Id` | 20000 to 23500 (as `Haus_GetById` of the original) | the file is refused |
| `haeuser.cod` | `Nummer` | 0 to 65535 | the file is refused |
| `haeuser.cod` | `Size` | 1 to 255 tiles each way (an island of the original is at most 255 wide) | above: the file is refused; 0 or less is read as 1 |
| `haeuser.cod` | `Ware`, `Rohstoff`, `Workstoff`, `BGruppe`, `Erzbergnr`, `Ruinenr` | 0 to 255 (a negative `Ruinenr` is no ruin) | the file is refused |
| `haeuser.cod` | `Figurnr` | 0 to 65535 | the file is refused |
| `haeuser.cod` | `Money` of `HAUS_BAUKOST` | 0 or more (a negative cost would pay for building) | the file is refused |
| `haeuser.cod` | `Maxlager`, `Prozent` (and every amount scaled to 1/32 t) | the range of a 32-bit unsigned number after scaling | saturated |
| `haeuser.cod` | `Wegspeed`, `Randwachs` | 0 to 400 and 0 to 200 before the scaling (no signed overflow) | clamped |
| `haeuser.cod` | `Arbeiter`, `Figuranz`, `RandAnz`, `RandAdd` | the range of the field | clamped |
| `figuren.cod` | `Nummer` | 0 to 1023 | the file is refused |
| both | `ObjFill` | 10000 copies | clamped, with a warning |
| both | `ObjFill`, all statements of a file | 200000 records made, nested records counted | the statement that would pass it and every later `ObjFill` are ignored, with a warning |
| `.gad` | `Nummer` of a gadget | 0 to 4095 (4096 gadgets per file; the original had 300) | gadgets above it are left out, with a warning |
| both | a number with a decimal point | the range of a 32-bit integer | clamped |
| both | times, amounts, health, radius (`Worktime`, `Maxenergy`, ...) | the range of the field | clamped (`core::saturating_cast`) |
| `.gam`, `.szs` | width and height of an island | at least 1 tile | the island is left out, with a warning |
| `.gad` | `Include` | 4 levels deep, 1 MiB of text in all | the rest is left out, with a warning |
| both | diagnostics of a file | 1000 messages | one more says that more follow; the rest are not listed |
| `.bsh`, `.zei` | rows of a sprite | no more than the bytes that follow its header, plus one | the file is refused |
| `.bsh`, `.zei` | pixels of all sprites of a sheet | 256 M | the file is refused |
| `.smk` | time per frame | above 0 and at most 1 s | the video is refused |
| `.smk` | sound of a track | what its frames last plus a second, at most 64 M samples | the sound is refused, the video plays without it |

The rule behind the table (LC-15): a number that indexes a table, names something (a ware, a
figure, a record) or sizes memory is refused when it lies outside, because a cut-down value would
silently mean something else; an amount (stock, cost, time, share) is saturated or clamped to its
field. Settings and command-line options follow their own rule (a bad option stops with the
usage, a bad line of `settings.cfg` falls back to its default), and saves, snapshots and network
messages are refused whole when anything in them is out of range.

Decimal numbers become integers by truncating toward zero and clamping, never by a plain cast:
a cast of a value out of range is undefined in C++ and gives different results on x86 and ARM,
and every computer of a multiplayer game must read the same data the same way.

The game numbers the engine relies on (ware, building kind, production kind and figure numbers)
are named in one place, `engine/assets/include/o1602/assets/game_data.hpp`. A data set that
renumbers them needs that header changed too; a data loader for that is planned
([ADR 0007](../architecture/0007-extensibility-and-modding.md)).

The score of the end screen and the monument allowance are named there too, in
`assets::scoring`: the city and fight curves as tables of straight segments, the money steps, the
monument and defeat points, and the rule that lets a growing city build a monument (every 30
minutes of game time with 200 inhabitants of class Bürger and higher). A mod changes them in that
header.

Limits of the game itself that more than one part checks (players with a start, island numbers
and indexes, cities per island, the world's side) are defined once, in
`engine/sim/include/o1602/sim/limits.hpp`; the network's and the menus' limits derive from them.
When one is exceeded the player sees which ("more than the game holds: 1 to 12 players"), not
"the file is damaged".
