# GAM / SZS / SZM – savegames and scenarios

Savegames (`.GAM`, in the original under `SAVEGAME\`), single-player scenarios (`.SZS`) and
multiplayer maps (`.SZM`) have **the same format**. It is a chunk container
(see [chunks.md](chunks.md)). `1602.exe` knows exactly one table of chunk names
(`0x00498244`), used by the load and save routines of both file kinds:

```
CUSTOM  NAME  RANDTAB  SZENE  AUFTRAG4 (AUFTRAG2, AUFTRAG)  PLAYER4 (PLAYER3, PLAYER2)  TIMERS  TURM
SOLDATINSEL  SOLDAT3 (SOLDAT2)  HANDLER  SHIP4  HIRSCH2  STADT4 (STADT3)  MARKT2  KONTOR2  MILITAR
WERFT  SIEDLER  ROHWACHS2  PRODLIST2  INSEL5 (INSEL4, INSEL3)  INSELHAUS
```

Names in brackets are older versions that are only still loaded. The islands are described in
[scp.md](scp.md). Open1602 therefore loads savegames and scenarios with the same code
(`sim::load_world`). The viewer opens savegames with `open1602 --savegame NAME` (or `--scenario FILE`) and
writes them with F5 or `--save FILE` ([savegame-writer.md](../re/savegame-writer.md)).

## Differences between a savegame and a scenario

Checked on a savegame that was saved directly after starting "Das Atoll"
(`SAVEGAME/game00.gam`, 153 chunks instead of 387):

- **Two `INSELHAUS` per island.** The first contains the complete base island (the entries of the
  template, with cleaned-up attributes: island number set, owner 7, random bits cleared), the
  second the changes – in this savegame exactly the entries of the scenario. Template number
  and flag `0x65` = 0 stay as in the scenario; so the original loads the template here too, and the first
  layer covers it completely. The rendering is pixel-identical to the scenario.
- **Island-related chunks only once:** `PRODLIST2`, `WERFT`, `MILITAR`, `SIEDLER`, `ROHWACHS2`,
  `STADT4`, `KONTOR2`, `MARKT2` appear once for all islands (in the scenario once per island).
- **`NAME`**: name of the scenario ("Das Atoll"). **`WIFF`** (4 bytes): sum of all 32-bit words of the data before it, never read by the original ([savegame-writer.md](../re/savegame-writer.md)).
- No `SZENE`, `RANDTAB` and no `SZENE_*` chunks.

## Savegame names (Game.dat)

The file is always called `SAVEGAME/gameNN.gam` (NN = slot 00–…). The name entered when saving
is written by the game to the text file `Game.dat` in the game folder:

```
  Objekt:   SPIELNAME
    Name:      0, "Merlin01", 1
    Name:      1, "Leer01", 1
```

`Game.dat` also contains settings (music, volume, videos) and the last saved
path (`Lastfile`, `lastgame.gam`). So the game writes into its installation folder (also
`Szenes/*.hss`, presumably high-score lists) – see bug T-005. Open1602 reads `Game.dat` only to
find savegames by their name (`open1602 --savegame Merlin01`, `o1602-tool savegames`).

## PLAYER4 – players (7 × 0x430 bytes)

Seven slots: four players (human or computer), the free trader, the pirates and the
natives. The slot number is also the **owner number** in the island tiles
(attribute bits 22–25) and for ships. 7 means "nobody".

| Offset | Type | Contents |
|---|---|---|
| 0x000 | int32 | money |
| 0x004 | u8 | control: 0 human, 0x0B natives, 0x0C computer, 0x0D free trader, 0x0E pirates; set during play: 9 defeated, 10 absent ([missions.md](../re/missions.md)) |
| 0x005 | u8 | slot number (0–6) |
| 0x006 | u8 | always 1 (meaning open) |
| 0x007 | u8 | **player colour** (see below) |
| 0x00A | u16 | counter of the defeat rules (bankruptcy, in multiplayer without city and ship; runtime +0x9E) |
| 0x00C | u8 | **mission**: slot in `AUFTRAG4`, 0xFF none (runtime +0x7C) |
| 0x00D | u8 | evaluated in network games (`0x00473C50`) |
| 0x00E | u8 | palaces built (runtime +0x87, at most one) |
| 0x00F | u8 | cathedrals built (+0x88) |
| 0x010, 0x012 | u16, u16 | triumphal arches: allotments, built (+0x8E, +0x90) |
| 0x014, 0x016 | u16, u16 | score of the fights: soldiers killed, soldiers lost |
| 0x018, 0x01A | u16, u16 | score of the fights: own ships lost, enemy ships sunk |
| 0x01C | u32 | scenario switches (runtime +0x70), only for humans and computers, only set in scenarios: bit 1 no further cities, 2 no free trader, 3 quiet while at peace, 4 minds a human above 0x020 inhabitants, 5 fixed army, 6 no supply or patrol ships, 7 minds its own size below 0x022, 8 minds its own development below 0x024, 9 minds a human above 0x025 ([ai.md](../re/ai.md), [diplomacy.md](../re/diplomacy.md#mood)); Open1602 reads and writes them (`Player::scenario`) |
| 0x020, 0x022 | u16 × 2 | inhabitants: of a rival (bit 4), the computer's own (bit 7) |
| 0x024, 0x025 | u8 × 2 | development (0..159, see `city_development`): own (bit 8), of a rival (bit 9) |
| 0x034 | u32 | **build levels reached**: bit n − 1 = INFRA level n (runtime +0x6C, [construction.md](../re/construction.md#unlocking)) |
| 0x008 | u8 | **culprit**: who last destroyed a warehouse or a marketplace of this player (runtime +0x81, 7 = nobody; curse of the natives) |
| 0x038, 0x03A | u16, u16 | monuments: allotments, built (+0x8A, +0x8C) |
| 0x040 + 2b | u16 × 7 | tribute received from player b, scaled, fading (+0xB0) ([diplomacy.md](../re/diplomacy.md)) |
| 0x060 + 2b | u16 × 7 | trade with b, fading (+0xC0) |
| 0x080 + 4b | u32 × 7 | damage suffered from b, 1/32 HP, fading (+0xD0) |
| 0x0C0 + 8b | u8, u32 | **peace** towards b: 0 war, 1 offered, 2 received, 3 treaty; followed by the time of the last declaration of war (1/10 s, only the player's own entry) (+0xF0, +0xF4) |
| 0x140 + 8b | u8 | **trade treaty** towards b, same values (+0x130) |
| 0x1C0 + 8i | 7 × {u8 kind, u8 player, u16, u32 time} | the first 7 of the 32 events the player remembers (+0x170, S-085) |
| 0x3C0 | char[64] | name of the player (empty in scenarios, in the savegame the name of the human) |
| 0x400 | char[64] | second name (network) |

Evidence: the multiplayer list function `0x00473C50` (record size 0x430; it reads from
record + 4) and all scenarios. Example "Das Atoll": colours 0, 1, 2, 3, 6, 4, 5 for slots 0–6.

**Confirmed on a savegame:** if the player chooses the yellow flag at the start, the savegame has
colour 2 for slot 0, and the computer opponents move up (slot 1 red, slot 2 blue, slot 3 white). So the
colour does not belong permanently to a slot; the owner numbers on islands and ships remain the
slots.

## Player colours

The colour is an index 0–6. In the original it only becomes visible through **separate sprite variants**,
not through a recolouring of the palette. The game chooses the flag of a ship via the table
`0x00498F2C` (colour → figure number in `figuren.cod`):

| Colour | Meaning | Flag figure | Sprites in `SHIP.BSH` |
|---|---|---|---|
| 0 | red | FAHNE1 (106) | 192–199 |
| 1 | blue | FAHNE2 (107) | 200–207 |
| 2 | yellow | FAHNE3 (108) | 208–215 |
| 3 | white | FAHNE4 (109) | 216–223 |
| 4 | pirates | FAHNEPIRAT (110) | 224–231 (skull) |
| 5 | natives | – (no flag) | |
| 6 | free trader | – (no flag) | |

**In Open1602**, slot, control and colour are translated on loading (`sim::load_world`): slots
0–6 become `sim::PlayerId`, owner 7 becomes `sim::kNoPlayer`, the control becomes `sim::PlayerRole`
and the colour becomes the banner of the same name in the base game (`render::BannerTable::original`). The number of players
and the banner list are open in the engine model ([ADR 0007](../architecture/0007-extensibility-and-modding.md)).

`FAHNEWEISS` (111, sprites 232–239) is the white flag and not a player colour. The eight sprites
per colour are the animation phases of the waving. Likewise there are separate variants for the building flags
(`FAHNEMARKT`, `ANIM` 0–5 with 8 images each, the same sprites; see [rendering.md](../re/rendering.md#6-display-figures-on-buildings-flags-smoke-status-symbols)) and presumably for the soldiers (`SOLDAT.BSH`, not yet examined).

> Correction: an earlier version of [rendering.md](../re/rendering.md) suspected player colours behind the
> 256-byte tables from `0x005B6360`. They are **brightness and colour-tint tables**
> computed from the palette at startup (`0x00494870`: factors 1.5 / 0.62 / 0.62 / 0.9 /
> 0.55 and a table with a blue tint 0 / 0.4 / 1.7). Index 6 means "unchanged".

## SHIP4 – ships (0x1B4 bytes each)

Written by `0x004600C0` from the ship table `0x004CF358` (150 × 0x218 bytes). The same
routine saves `HANDLER`, `SOLDAT3` and `TURM`. Every ship has a **display figure** (pointer
at ship + 4) that carries position, direction and figure number.

| Offset | Type | Contents |
|---|---|---|
| 0x000 | char[28] | name |
| 0x01C | u16 | x (world tile): floating-point position of the display figure (+0x28), truncated |
| 0x01E | u16 | y (world tile), likewise (+0x2C) |
| 0x024 | u16 | ship + 0x1AE: mission of the computer AI (0 none; 1, 2 return home, 5, 6, 9 …) ([ships.md](../re/ships.md#open)) |
| 0x02C | u32 | **ordered destination** (reference, [ships.md](../re/ships.md#data)) |
| 0x030 | u32 | **current destination** (reference) |
| 0x034 | u32 | **last destination** (reference, e.g. the warehouse the trader last visited) |
| 0x03C | u16 | **hit points** in 1/32 |
| 0x03E | u16 | ship + 0x1AC: parameter of the AI mission (e.g. island · 32 + city) or waiting time |
| 0x040 | u8 | ship + 0x1A7: counter of the AI mission (10 at the mission, − 1 per loading or unloading) |
| 0x041 | u8 | planning radius (0x50) |
| 0x042 | u8 | **cannons** on board |
| 0x043 | u8 | flags: bit 1 trade route on, bit 3 one-way route, bit 2 for sale, bit 4 under repair, bit 0 UI marker |
| 0x044 | u16 | sale price in 1/128 of the price (0 = 100 %) |
| 0x046 | u16 | running number (slot in the ship table) |
| 0x048 | u16 | figure number (HANDEL1 = 21, HANDEL2 = 23, KRIEG1 = 25, KRIEG2 = 27, HANDLER = 29, PIRAT = 31) |
| 0x04A | u8 | figure kind: 1 ship, 2 free trader, 3 pirate, 5 trader's cart on land (uses the ship table) |
| 0x04B | u8 | owner (slot in PLAYER4, 7 = nobody) |
| 0x04C | u8 | record of the free trader in `HANDLER` |
| 0x04D | u8 | **island at the ship's position** (`0x0046A570`, e.g. at the warehouse), 0xFF = open sea |
| 0x04F | u8 | **current stop** of the trade route |
| 0x050 | u8 | **direction 0–7** (byte 3 of the display figure, 45° steps) |
| 0x051 | u8 | ship + 0x003: only read and written, otherwise unused (0 in all savegames) |
| 0x052 + 0x24·s | | **trade route**, 4 stops of 0x24 bytes: u32 reference, then 3 entries of 8 bytes each (+0 u16 ware as Id of the producer − 20000, +2 u16 quantity in 1/32 t, +4 u8 bit 0 deliver, +5 to +7 free), rest of the stop free |
| 0x174 | 8 × 8 bytes | **cargo**: u16 ware as Id of the producer − 20000 (soldiers: ware + 9876), u16 bits 0–13 quantity in 1/32 t, u32 bits 0–9 **average price** per ton; empty slot = ware 0 |

`HANDLER` (0x25C bytes per trader ship): byte 0 occupied, bytes 1–2 slot of the ship, byte 3 number of the
record; from +4 50 entries of 12 bytes (u32 bits 0–9 average price, bits 10–14 age in
visits; u32 quantity in 1/32 t; u16 ware as Id of the producer − 20000), the first 25 are occupied in the
order of the goods.

> Correction: an earlier version took byte 0x04D for the direction. It is the island at which the
> ship lies. The direction is in 0x050. This was noticed on the savegame played on further, in which
> almost all ships at sea had 0xFF.

## How ships are drawn

According to the figure drawing routine `0x0044FA90` (cases 1–3 = ships):

- **Sprite** = `Gfx + ((direction + 2 · view rotation) & 7) · Rotate` (`0x0044FA60`; ships have `Rotate: 1`).
- **Position:** figure point → screen as for the tiles (`0x0044D940`:
  x = (fx − fy) · width/2, y = (fx + fy) · (height+1)/2). Like every figure, the ship lies at
  Y = y + `Posoffs[1]` · width/64 (`0x00451CC5`, [figures.md](../re/figures.md#drawing)). The sprite
  is centred horizontally, its bottom edge lies at Y + `Posoffs[0]` · width/64 (field +0x0C of the
  figure definition); for trade ships (−19, 34) that is 15 pixels below the figure point.
- **Flag:** figure from the colour table, image `Gfx + animation phase (0–7)`. Its distance from the
  figure point is `Fahnoffs[1]` along the ship's axis, rotated by (direction + 2) · 45°
  (`0x00443180`), projected as above, starting from Y. It is raised by `Fahnoffs[2]` · width/2 (`0x00451220`).
  The flag hangs down from this point.
- **Order:** figures are drawn between the two passes of a screen row
  (`0x00489A60`), sorted by Y. Because the bottom edge of the hull lies above that, flat
  sea tiles never cover the hull.

Open points: animation phase of the flag (byte +0x0B of the
runtime figure; Open1602 uses 90 ms per image like `FAHNEMARKT`), bow waves (`BUGH`). The savegame
stores only whole tiles; the fractional part of moving ships is lost on saving.

## Game state in the savegame

All chunks of a savegame and their save routines. The runtime tables have fixed sizes;
only occupied entries are saved (first byte ≠ 0xFF or owner ≠ 7). The number in brackets
is the count in Merlin01 after a few minutes of play.

| Chunk | Record | Writer | Runtime table | Contents |
|---|---|---|---|---|
| `INSEL5`, `INSELHAUS` | [scp.md](scp.md) | `0x00468740` | islands `0x005E6B20` (0xB00 each) | island header, two tile layers |
| `HIRSCH2` | 8 | `0x00468740` | island + 0x6C, 8 per island | deer: island number, x, y, –, u32 timer in ms (153) |
| `PLAYER4` | 0x430 | `0x00478363` | – | players (above) |
| `SHIP4` | 0x1B4 | `0x004600C0` | `0x004CF358`, 150 × 0x218 | ships (above, 14) |
| `HANDLER` | 0x25C | `0x004600C0` | `0x004A0DC8`, 30 × 0xD0 | goods of the free trader per ship: price, age, quantity (above, 7) |
| `SOLDAT3` | 0x44 | `0x004600C0` | `0x0051C678`, 400 × 0x1B0 | soldiers: position in half tiles, hit points, figure, slot, home and destination, owner, animation, planning radius, direction, patrol ([figures.md](../re/figures.md#soldiers-in-the-savegame), 9) |
| `TURM` | 0x24 | `0x004600C0` | `0x004EDE60`, 350 × 0x118 | cannons of the watchtowers: +0 world tile x/y, +4 hit points (never reduced), +6 figure (38/40), +8 slot, +0x0A reference to the tower building (0x33), +0x17 island, +0x18 owner, +0x1B direction (Merlin01: 61 pirate towers, owner 5) |
| `STADT4` | 0xA8 | `0x00485020` | cities `0x005DBAE0`, 75 × 0x258 | cities (below, 20) |
| `KONTOR2` | 0x3EC | `0x00485020` | the same city table | goods store of a city (below, 20) |
| `MARKT2` | 0x104 | `0x00485020` | the same city table | consumption of a city: u32 head (bits 0–7 island, bits 8–11 city), then 16 slots of 16 bytes, of which the 11 consumer goods use the first: +0x0 demand A, +0x4 supply B (u32 each, 1/8192 t), +0x8 meaning open, +0xC ware as Id of the producer − 20000 (u16, 0 = empty slot), +0xE supply level C (u8), +0xF free; 4 + 16 × 16 = 0x104 ([consumption.md](../re/consumption.md)) (20) |
| `PRODLIST2` | 0x18 | `0x00485530` | `0x0054A3B8`, 4144 × 0x14 | production buildings and warehouses at the origin tile: state, stock, progress (below, 126) |
| `WERFT` | 0x18 | `0x00485530` | `0x005A6C18`, 160 × 0x12 | shipyards: island, x, y, bit 27 switched on; +4 seconds until the end of the tick, +6 order (ship type 0–3, 0xFF none), +8 build points, +0x0A wood, +0x0C ship under repair (0xFFFF none) ([ships.md](../re/ships.md#shipyard)) (1) |
| `MILITAR` | 0x70 | `0x00485530` | `0x0055EB80`, 288 × 0x30 | castles and warrior huts (loader `0x00483C60`): island, x, y, epoch (+3 bits 0–2), +4 seconds until the next training step, +8/+0x0A/+0x0C weapons in stock (swords, muskets, cannons, 1/32 t), from +0x10 nine slots of 8 bytes: figure (bits 0–11), in training (bit 12), armed (bit 13); hit points (bits 0–11 of the second word), progress 0–320 (bits 16–31) (3: the huts of the natives with eight finished spearmen each) |
| `SIEDLER` | 0x10 | `0x00485530` | `0x005A77E8`, 4160 × 10 | houses: island, x, y, epochs, class, market distance, inhabitants · 64, time group, market and service bits ([houses.md](../re/houses.md)) (132) |
| `ROHWACHS2` | 8 | `0x00485C20` | `0x00562DC8`, 32860 × 8 | growth of fields, trees, plantations, fishing grounds: island, x, y, class, counter value seen, stage, 2 bytes 0 ([growth.md](../re/growth.md)) |
| `TIMERS` | 0x298 | `0x00485D10` | scattered counters (`0x005A77E0`, `0x005DBAB0`, …) | global counters and timers; +0x00: phase of the city tick (10 s), +0x01: phase of the island tick (30 s), +0x02–0x04: second phases (shipyard, military, production), +0x84/+0x88/+0x8C/+0x90/+0x94: their time accumulators in ms; +0x64: 32 counters of the growth classes (0–7), +0x218: 32 class clocks in ms, +0x44: 16 epochs of the house time groups, +0x198: 16 house timers in ms, +0xA0: game time in 1/10 s, +0xA6: difficulty 0–3 (`0x005B6304`, drought table), +0xA8: game settings (`0x005B706C`; bit 2 no droughts, bit 8 no volcanoes), +0xB0: next city name from `[STAEDTE]` (`0x005B3168`), +0xB4: earliest next drought roll in 1/10 s (new game 54000; a saved 0 becomes 36000), +0xC4: running volcanic eruption, +0xC8: last one (references `{0x33, island, x, y}`), +0xCC: earliest next volcanic eruption in 1/10 s, +0xD0: craters of the world, at most 20 ([volcanoes.md](../re/volcanoes.md)); +0xA5: tutorial (`0x005B62FC`; 1 only in `Einführungsspiel0–4.szs`, the computers stay idle, [ai.md](../re/ai.md#tutorial)), +0xA7: mission number (endless game 1), +0xAC: game id (`sim::new_game_id`), +0xB8: interval of new pirate ships in 1/10 s, +0xBC: difficulty (as the new game sets it), +0xC0: fleet limit per player (0 = 33, L-001); +0x98/+0x9C and the rest: meaning open |
| `AUFTRAG4` | – | `0x00478558` | – | missions of the scenario, 0x8C4 bytes per slot; structure, goals and flow in [missions.md](../re/missions.md) |
| `WIFF` | 4 | `0x00485EE0` | u32 `0x005B3160` | checksum: sum of the 32-bit words of all data before it (`0x00490730`), never read |

All coordinates in `STADT4` … `ROHWACHS2` are island tiles; the island is given as the island number in the first
byte. The bit fields of the records are described exactly in the write routines; their meaning is
only partly known. Open1602 reads the known fields (`assets::load_game_state`) and keeps the
rest as raw values so that nothing is lost when saving later.

### STADT4 – cities (0xA8 bytes)

A city is the settlement of a player on an island. Native villages and pirate nests are
cities too.

| Offset | Type | Contents |
|---|---|---|
| 0x00 | u8 | island number |
| 0x01 | u8 | city number on the island (= attribute bits 14–16 of the island tiles) |
| 0x02 | u8 | owner |
| 0x04 | bit 0 | city + 0x257 bit 0 |
| 0x05 | u8 | the last population message (city + 0x253): 0x11 = 100 inhabitants … 0x1D = 9000 ([consumption.md](../re/consumption.md#population-messages)) |
| 0x08, 0x0C | u32 | city + 0x1F0, + 0x1F4: trade statistics of the warehouse (purchases, sales; + 32 × value per trade, × 89/90 every 10 s; / 480 = per minute, the city window) |
| 0x10 | u32 | city + 0x1FE: number of running fires and plagues, blocks growth; the loader skips it (S-001, [disasters.md](../re/disasters.md)) |
| 0x18 | 7 × u32 | city + 0x206 … (natives: 49 per value, players: 0) |
| 0x58 | u32 | city + 0x1D4 |
| 0x5C | 5 × u32 | **inhabitants** per class (city + 0x220) |
| 0x78 | 5 × u8 | **satisfaction** per class, 128 = satisfied (city + 0x248) |
| 0x7F | 5 × u8 | **tax rate** per class, 128 = normal (city + 0x24D) |
| 0x86 | u8 | **food supply level** (city + 0x255) ([consumption.md](../re/consumption.md)) |
| 0x87 | char[] | **name** of the city (e.g. "Lemonhall", "Nova Semlja") |

### KONTOR2 – a city's goods store (0x3EC bytes)

| Offset | Type | Contents |
|---|---|---|
| 0x00 | u32 | bits 0–7 island number, 8–15 **x**, 16–23 **y** of the warehouse, 24–27 city number |
| 0x2C | 23 × 20 bytes | goods 2–24 (iron ore … bricks) in the order of the goods numbers |

A goods slot (20 bytes):

| Offset | Contents |
|---|---|
| +0x00 | u32: bits 0–9 **sale price** per ton, 10–19 **purchase price**, bit 20 sells, bit 21 buys ([ships.md](../re/ships.md#trading-at-the-warehouse)) |
| +0x04 | u16 reserved for carriers (city + 0x24 + 12 · i; the loader skips it) |
| +0x08 | u16 **lower limit** when selling, u16 **upper limit** when buying, in 1/32 t (city + 0x26/0x28) |
| +0x0C | u16 **stock in 1/32 t**: Lemonhall 817 food (25.5 t), 400 cloth (12.5 t), 32 tools (1 t), 800 bricks (25 t), wood 0 – confirmed in a running game (25 t, 12 t, 1 t, 25 t, no wood; the warehouse shows whole tons) |
| +0x10 | u16 ware as Id of the first producer − 20000 (e.g. 513 → 20513 brickworks); by this the game recognises the ware |

The position is confirmed: in all settler cities there is a building at (x, y) with
`HAUS_PRODTYP Kind` = `KONTOR`, and its tile belongs to the city (data test). The unit of the
stocks was confirmed by a look into the warehouse in a running game. Open1602 also computes internally
in 1/32 t (`assets::kStockUnitsPerTon`), i.e. with integers and deterministically.

### PRODLIST2 – production state (0x18 bytes)

One entry per building with `HAUS_PRODTYP` (workshops, farms, mines, hunters, fishermen, marketplace, warehouse
…), always at the **origin tile** of the building (data test: all entries of all savegames). The
write routine packs the 20-byte runtime entry; the mapping:

| Offset | Contents | Runtime (`0x0054A3B8` + 20 · i) |
|---|---|---|
| 0x00, 0x01, 0x02 | island number, x, y | +0, +1, +2 |
| 0x05 | u16 **stock** in 1/32 t: finished goods waiting for the carrier (compared with `Maxlager`) | +0x0C |
| 0x08 | u16 | +0x04 (counter, counted down each tick) |
| 0x0A, 0x0C | u16, u16 | +0x08, +0x0A |
| 0x0F | u8 **working** (0x80) – has raw material and produces | +0x0E |
| 0x10 | u16 **progress** of the running cycle, 512 = finished | +0x10 |
| 0x12 | u16 | +0x12 |
| 0x14 | bit 0 **switched on**; bit 1 → +0x0F bit 6; bits 2–5 **animation phase**; bit 6 → +0x0F bit 4 | +3 bit 3, +3 bits 4–7 |
| 0x15 | bits 0–3 **cycles without raw material** (stops at 15; compared with `Maxnorohst`) | +0x0F bits 0–3 |

Bit 7 of +0x0F (special state, status symbol ANIM 2) is not saved. Merlin01: 6 of 18
production buildings on island 0 are working, stock in whole tons, progress 0–514. Scenarios start with
all production buildings idle. What the state means for rendering:
[production.md](../re/production.md).
