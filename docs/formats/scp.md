# SCP – island templates

677 files in `NORD/`, `SUED/` (climate zones), `NORDNAT/`, `SUEDNAT/` (with natives?) and
`NOKLIMA/`. File names: `lit` (little), `mit` (middle), `med` (medium), `big`, `lar` (large) plus
a running number – assembled by the game via `%s%s%s%02d.SCP` (string @ `0x0049AE1C`).

Chunks: `INSEL3`, `INSEL4` or `INSEL5` (header; 73 / 66 / 538 of the templates), followed by `INSELHAUS` (tiles), sometimes `HIRSCH2`.

## Island header (INSEL3: 40 bytes, INSEL4/INSEL5: 116 bytes)

Confirmed fields (common to both versions):

| Offset | Type | Field | Evidence |
|---|---|---|---|
| 0 | u8 | island number | |
| 1 | u8 | width (tiles) | all `INSELHAUS` x < width |
| 2 | u8 | height (tiles) | all `INSELHAUS` y < height |
| 3 | u8 | flags (curse, volcano; see `0x03` below) | 0 in all templates |
| 4 | u16 | position x on the world map | values < 500 |
| 6 | u16 | position y on the world map | values < 350 |

Additionally in `INSEL4`/`INSEL5` (116 bytes) – deposits, fertility and the reference to the
island template:

| Offset | Type | Field | Evidence |
|---|---|---|---|
| `0x03` | u8 | bit 0: curse of the natives, becomes a drought in the next island tick (island +0x44 bit 5); bit 2: a volcano of the island has erupted (+0x44 bit 6) | `0x0046B3E0`, [growth.md](../re/growth.md#drought) |
| `0x17` | u8 | number of volcano craters (list at `0x3C`, 8 bytes per crater: byte 1 x, byte 2 y = origin of mountain 21108, bytes 6–7 remaining eruptions; at most 2) | writer `0x00468740`, [volcanoes.md](../re/volcanoes.md) |
| `0x08` | u16 | island ticks since the last deer count, 0–3 (island +0x68) | `0x0046B3E0`, [deer.md](../re/deer.md) |
| `0x0A` | u16 | phase of the island tick in which the island last had its turn (island +0x64); if it differs from `TIMERS` +0x01, the island has its turn immediately | [newgame.md](../re/newgame.md#comparison-with-the-original) |
| `0x1A` | u8 | number of deposits (at most 2) | writer `0x00468740` |
| `0x1B` | u8 | island +0x42: bit p = player p has **explored** the growing options ([ships.md](../re/ships.md#exploring)) | `0x0046ABF0` |
| `0x1C` | 2 × 8 | deposits: ware, x, y (origin of the mountain), byte 3 (bit p = player p knows the deposit: explored or claimed by the computer), kind (0 large, 1 small vein), 0, u16 quantity in 1/32 t ([logistics.md](../re/logistics.md#mines-and-deposits)) | `0x0046C010` |
| `0x5C` | u32 | fertility: bit (ware − 45) for grain, tobacco, …, grass, tree, fish | `0x0046AFF0` |
| `0x68` | u32 | bit p: player p has discovered the island (island +0x43) | [ships.md](../re/ships.md) |
| `0x66` | u8 | strength of a running drought (island +0x45): 0 none, 0x40 random, 0x26 curse | `0x0046B3E0` |
| `0x6C` | u32 | end of the drought as game time in 1/10 s (island +0x60) | `0x0046A630` |
| `0x60` | u16 | template number (`0xFFFF` = none) | loader at `0x00469BCA`, path building `0x00469690` |
| `0x62` | u16 | size class: 0 `lit`, 1 `mit`, 2 `med`, 3 `big`, 4 `lar` | table `0x0049A890` |
| `0x64` | u8 | climate: 0 `Nord`, 1 `Sued` | table `0x0049A8A4` |
| `0x65` | u8 | ≠ 0: island is stored completely, do not load a template | jump at `0x00469BB0` |

The world map is **500 × 350** tiles: `Game_Init` passes `0x1F4, 0x15E` to an
initialisation function (`0x004860C0`); Open1602's own worlds can be larger
([worldgen.md](../re/worldgen.md)). The remaining bytes of the header (among them the owners of the eight
city sites from `0x0C`, [savegame-writer.md](../re/savegame-writer.md)) are not all decoded
and are carried along raw (`IslandHeader::raw`).

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
| 26–31 | unknown | |

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
