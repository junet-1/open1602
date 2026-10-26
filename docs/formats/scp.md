# SCP – island templates

677 files in `NORD/`, `SUED/` (climate zones), `NORDNAT/`, `SUEDNAT/` (with natives?) and
`NOKLIMA/`. File names: `lit` (little), `mit` (middle), `med` (medium), `big`, `lar` (large) plus
a running number – assembled by the game via `%s%s%s%02d.SCP` (string @ `0x0049AE1C`).

Chunks: `INSEL3`, `INSEL4` or `INSEL5` (header; 73 / 66 / 538 of the templates), followed by `INSELHAUS` (tiles), sometimes `HIRSCH2`.

## Island header (INSEL3: 40 bytes, INSEL4/INSEL5: 116 bytes)

The header is a packed copy of the first fields of the runtime island record (`0x005E6B20` + 0xB00 · n).
The writer is `0x00468740` (it fills a zeroed buffer), the reader the `INSEL3/4/5` handler `0x00469900`.
Both were read completely (2026-10-26): every byte below is either mapped or proven unused.

### INSEL5 (116 bytes)

| Offset | Type | Field | Island record |
|---|---|---|---|
| `0x00` | u8 | island number | +0x00 |
| `0x01`, `0x02` | u8, u8 | width, height in tiles (all `INSELHAUS` x < width, y < height) | +0x0C, +0x10 |
| `0x03` | u8 | flags: bit 0 curse of the natives, becomes a drought in the next island tick ([growth.md](../re/growth.md#drought)); bit 1 = record +0x44 bit 2 (below); bit 2 a volcano of the island has erupted | +0x44 bits 5, 2, 6 |
| `0x04`, `0x06` | u16, u16 | position x, y on the world map (values < 500 / < 350) | +0x04, +0x08 |
| `0x08` | u16 | island ticks since the last deer count, 0–3 ([deer.md](../re/deer.md)) | +0x68 |
| `0x0A` | u16 | phase of the island tick in which the island last had its turn; if it differs from `TIMERS` +0x01, the island has its turn immediately ([newgame.md](../re/newgame.md#comparison-with-the-original)) | +0x64 |
| `0x0C` | u8 × 8 | **owners of the eight city slots**: the writer stores the owner of each city (`city + 0x1A`) or 7; the reader creates a city for every slot below 7 (`City_Alloc 0x00468E10`). Templates also use 15 for "no city" | cities +0xAC |
| `0x14`–`0x16` | – | never written or read (0) | |
| `0x17` | u8 | number of volcano craters (list at `0x3C`, at most 2) | +0x40 |
| `0x18` | u8 | **treasure flag**: set when the world plan buries a treasure on the island (`0x0048D217`); only then does building on a tile look for a treasure (`0x0046C500`). It stays set after the treasure is found | +0x1D |
| `0x19` | u8 | **number of plantation crops** the world plan gave the island; the plan's limit per island (`SZENE` +0x60) is compared with it ([worldgen.md](../re/worldgen.md)) | +0x47 |
| `0x1A` | u8 | number of deposits (at most 2) | +0x41 |
| `0x1B` | u8 | bit p = player p has **explored** the growing options ([ships.md](../re/ships.md#exploring)) | +0x42 |
| `0x1C` | 2 × 8 | deposits: ware (2 iron ore, 3 gold, 0x3A treasure of 1000 gold), x, y (origin of the mountain), byte 3 (bit p = player p knows the deposit), kind (0 large, 1 small vein), 0, u16 quantity in 1/32 t (treasure: gold) ([logistics.md](../re/logistics.md#mines-and-deposits)) | +0x20 |
| `0x2C`–`0x3B` | – | never written or read (0) | |
| `0x3C` | 2 × 8 | volcano craters: byte 1 x, byte 2 y = origin of mountain 21108, bytes 6–7 remaining eruptions ([volcanoes.md](../re/volcanoes.md)) | +0x30 |
| `0x4C`–`0x5B` | – | never written or read (0) | |
| `0x5C` | u32 | fertility: bit (ware − 45) for grain, tobacco, …, grass, tree, fish; the reader ORs in 0x1181 (grain, grass, tree, fish) | +0x5C |
| `0x60` | u16 | template number (`0xFFFF` = none) | +0x1A |
| `0x62` | u16 | size class: 0 `lit`, 1 `mit`, 2 `med`, 3 `big`, 4 `lar` (table `0x0049A890`). **The reader replaces it by the class of the width** if the width is below 111: below 33 `lit`, 43 `mit`, 56 `med`, 76 `big`, else `lar`. Only templates store a different value (65 of them store 0, e.g. `NORD/big01.scp`; `NORDNAT/med15.SCP` is the only one that also refers to a template, which therefore is `Nord/med15`) | +0x18 |
| `0x64` | u8 | climate: 0 `Nord`, 1 `Sued`; the name table also maps 2 to `Nord` and 3 to `Sued` | +0x1C |
| `0x65` | u8 | ≠ 0: island is stored completely, do not load a template | +0x44 bit 0 |
| `0x66` | u8 | strength of a running drought: 0 none, 0x40 random, 0x26 curse | +0x45 |
| `0x67` | u8 | read into +0x46, never written and never used (0 in all files) | +0x46 |
| `0x68` | u32 | bit p: player p has discovered the island (only the low byte is kept) | +0x43 |
| `0x6C` | u32 | end of the drought as game time in 1/10 s | +0x60 |
| `0x70`–`0x73` | – | never written or read (0) | |

Record +0x44 bit 1 marks an island whose world plan entry asked for a native village; while a
template is loaded into such an island, its stored template reference is used even if the template
says "complete" (that is how `NORDNAT` village templates pull in their `NORD` base island). Bit 2 is set
by the world plan for every island it generates (`0x0048C970`) and saved as bit 1 of `0x03`; while it
is set, the status line over the map does not show the crop suitability of the island
(`Hud_MapStatusLine 0x0040A4EE`). The endless-game savegames all store it as 0; where the game clears
it was not found.

### INSEL4 and INSEL3

**INSEL4** (116 bytes, 66 templates) is read like INSEL5 except: the size class is taken as stored,
the flags `0x03`, the craters, the drought (`0x66`, `0x6C`), `0x67` and the discovery mask `0x68` are not
read, a city slot creates a city for owner 0 if its byte is not 0, and the island is always marked
as stored completely.

**INSEL3** (40 bytes, 73 templates): number, width, height, position (`0x00`–`0x07`) and the eight city
slots at `0x0C` (≠ 0: a city of owner 0); bytes `0x14`–`0x27` are not read. The fertility gets 0x1181,
the island is marked as stored completely.

The world map is **500 × 350** tiles: `Game_Init` passes `0x1F4, 0x15E` to an
initialisation function (`0x004860C0`); Open1602's own worlds can be larger
([worldgen.md](../re/worldgen.md)). Open1602 reads the decoded fields and keeps the whole header as
raw bytes (`IslandHeader::raw`), so that a savegame written back keeps every byte.

### Loader switches

Two globals redirect the loader. `0x0060912C`, if set, is the island record into which the
island chunks of the file are loaded, whatever island number they carry: the world plan sets it while
it lays a template on a planned island (`0x0048C8C4`), so does a new template on an existing island
(`0x00469770`) and loading a single island file (`0x0040E3xx`, island `0x005DBAB4`). `0x005E6AA8`
would send them into the current island `0x006C305C`; `1602.exe` never writes it (always 0), it is a
leftover of the editor.

## INSELHAUS (8 bytes per entry)

```c
struct IslandTile {
    uint16_t id;      // + 20000 = "Id" of a HAUS record in haeuser.cod (verified)
    uint8_t  x, y;    // position on the island
    uint32_t bits;    // packed attributes, see below
};
```

| Bits | Contents | Evidence |
|---|---|---|
| 0–1 | rotation 0–3 | `Build_PlaceBuildings` `0x004084D0`, `Haus_GetRotatedSize` `0x00463830` |
| 2–5 | animation phase (→ bits 15–18 in the island grid) | passed to `0x004653A0` |
| 6–13 | island number | matches the island header (e.g. 19 in `NORD/big.scp`) |
| 14–16 | city (7 = none) | `0x004084D0` sets `0x1C000` if there is no city |
| 17–21 | random value | passed to `0x00481450` |
| 22–25 | player | index into the player table (`0x005B7680`, stride `0x280`) |
| 26–31 | unused | 0 in every tile of every original file (templates, scenarios, savegames); the loader ignores them |

**One entry per building**: multi-tile buildings appear only once at their origin position;
on loading they are spread over all tiles with the placement algorithm from [rendering.md](../re/rendering.md).

**`id + 20000` = `Id` in `haeuser.cod` – verified:** all 23 distinct IDs from `NORD/big.scp`
exist as the `Id` of a `HAUS` record (test `data: island tile ids ...`). If you draw each
tile isometrically with the `Gfx` index of that record from `GFX/STADTFLD.BSH` (64 × 31, offset
`Posoffs`), you get a consistent island of grassland, forest, coast and water.

**Ground layer on loading** (`0x00468550`): every entry goes into the upper layer. If it has no city
(city bits 7), is not a ruin (Kind 12, 29) and does not belong to a city's land
(`0x00480B70`: roads, piers, fields, towers, production buildings, houses, warehouses), it also goes into the
ground layer (`0x00463E10`, island +0xAFC). Savegames therefore write two layers: first the whole
ground layer, all entries with city and owner 7, then the tiles of the upper layer that differ from the
ground ([savegame-writer.md](../re/savegame-writer.md)). Open1602 loads the same way.

Curiosity: `NORD/big.scp` (without a number) has gaps in the shape of "???" – entries are simply missing there.
The file is apparently a placeholder template; the real islands are called `big00`–`big66`.


## Scenarios and savegames

`.SZS` (single player), `.SZM` (multiplayer) and `.GAM` (savegame) use the same chunks.
Per island there follows a set `INSEL5, INSELHAUS, HIRSCH2, PRODLIST2, WERFT, MILITAR, SIEDLER, ROHWACHS2,
STADT4, KONTOR2, MARKT2`, plus global chunks (see [chunks.md](chunks.md)).

**Islands in scenarios and savegames only store their differences from an island template.**
If a template number is set in the header and the flag `0x65` = 0, the original first loads
`INSELHAUS` from `<climate>\<size><number:02>.SCP` (e.g. `Nord\big12.SCP`) and then lays the
entries of the scenario on top. Example "Das Atoll", island 0: 1488 own entries on the template
`NORD/big12.scp` (4200 tiles). All 34 islands of `Das Atoll.szs` and all 31 of `.szm` can
be built this way without gaps (test `data: islands of a scenario ...`).

The files in `NORDNAT/`/`SUEDNAT/` use this as well: they contain only a native village and
refer to the matching base island (e.g. `NORDNAT/BIG01.SCP` → `NORD/big01.scp`).
Islands with 0 entries are unchanged templates.

> Correction: an earlier version of this document claimed that scenarios contained the complete
> terrain except the sea. That was wrong – the rendering showed cut-off island parts.

Savegames (`.GAM`) have the same format as scenarios; players and ships are described in
[gam.md](gam.md).

The world map (500 × 350) is created by placing each island built this way at the `pos_x/pos_y` of its header
(`sim::load_world`; templates are cached, since many islands share one).
Everything outside the islands is open sea, see [rendering.md](../re/rendering.md#5-world-map-open-sea-and-animation-clock).

## Tools

```sh
o1602-tool islands NORD/big.scp
o1602-tool islands "Szenes/Das Atoll.szs"
o1602-tool render-world "Szenes/Das Atoll.szs" world.png
```
