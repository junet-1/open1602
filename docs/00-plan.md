# Anno 1602 – Open-Source Engine: Project Plan

Status: 2026-10-18 · Basis: GOG installation „Anno 1602 – Königsedition“ ("Anno 1602 – King's Edition") (de-DE)

## 1. Goal

A standalone, cross-platform engine that runs with the **original data** of a legally acquired copy
(approach as in OpenRCT2, OpenTTD, Julius/Augustus, OpenMW). No original assets are distributed.
The engine should be playable faithfully to the original, fix known bugs of the original and provide a clean basis for
new features, QoL and mods.

### Requirements for extensibility and modding

Set on 2026-10-16, binding for all new code. The architecture for this is in
[ADR 0007](architecture/0007-extensibility-and-modding.md).

- New **buildings, colours, production chains, islands, landscapes, factions, animations,
  graphics, music and sounds** can be added.
- Custom **campaigns and scenarios** can be written.
- New **island types and island sizes** are possible, including huge islands.
- The **island generator** is more flexible and more varied than in the original.
- The **number of players** is not fixed at 4 (+ traders, pirates, natives). For
  example, five people can play in multiplayer, and neutral parties such as pirates and travelling
  traders scale with it.
- **Colours and flags** are extensible, with a flag selection menu as in Anno 1503.
- The **endless game** can be configured in more detail than in the original, as in later Anno titles
  (for example the refund on demolition: none, partial or full). The settings
  live in `sim::Rules` ([ADR 0008](architecture/0008-game-settings.md)).

## 2. Inventory (first analysis of the game folder)

| File / folder | Finding | Significance |
|---|---|---|
| `1602.exe` (632 KB) | PE32, Intel 386, MSVC 6 (linker 6.0), **not packed**, no copy protection | Ideal for static analysis |
| `.text` 0x94E70 (~595 KB code) | estimated 2,000–3,000 functions | manageable scope |
| `.data` ends at 0x49D000, `.rsrc` only at 0x72D000 | **~2.9 MB uninitialised globals (BSS)** | Game state lives largely in global arrays → memory layout easy to reconstruct |
| Imports `DDRAW`, `Maxsound.dll` (→ DSOUND), `Maxnet.dll` (→ DPLAYX), `smackw32.dll`, `Language.dll` | clear subsystem boundaries | Entry points for graphics, sound, network, video, texts |
| `haeuser.cod`, `figuren.cod`, `Text.cod`, `Editor.cod` | Plain-text scripts, **every byte negated** (`b = -b & 0xFF`) | Buildings, units, costs, texts are **data-driven** – huge advantage |
| `GADDATA/*.GAD`, `Gadedit/*.GAD` | Plain-text UI definitions (gadgets) | UI layouts can be taken over directly |
| `GFX/`, `MGFX/`, `SGFX/` `*.BSH` | Sprite containers (`BSH\0` header), 3 zoom levels | Graphics |
| `TOOLGFX/*.ZEI`, `*.COL` | Fonts (`ZEI\0`), palette | Fonts, 8-bit palette |
| `NORD/`, `SUED/`, `*NAT/`, `NOKLIMA/` `*.SCP` (677 files) | Island templates, chunk-based | Map generation |
| `Szenes/*.SZS`, `*.SZM` (138) | Scenarios / multiplayer maps | Same chunk format as savegames (`.GAM`) |
| `VideoSmk/*.SMK` (51) | Smacker videos | own decoder based on the public description (`engine/video`, [smk.md](formats/smk.md)) |
| `SAMPLES/`, `Speech8/`, `MUSIC8/` | WAV | usable directly |
| `1602Edit.exe` + `Editor.dll` | Map editor | later, same code base |

Existing community work (format knowledge, reference – check licences before reuse):
`stwe/MDCII` (C++, Anno 1602 world rebuild, BSH/SCP parser), `stwe/Benno4j` (Java),
a Python grammar/parser generator for the `.cod` scripts.

## 3. Reverse-engineering methodology

### 3.1 Principles
- **Document, don't copy**: Findings from the binary are recorded as a specification (formats, structures,
  algorithms, formulas). The engine is **rewritten** on the basis of this documentation – no
  copy-paste of decompiled code. This keeps the code readable and legally cleaner.
- **Symbols are source code**: Function/variable names, structs and comments are regularly exported from the
  analysis database and versioned (`re/symbols/`). rea works with an *ephemeral*
  Ghidra database – without an export, annotations are lost.
- **Every claim with evidence**: address, decompiled excerpt, test case or memory dump.

### 3.2 Toolchain
| Purpose | Tool |
|---|---|
| Agent-assisted analysis | **rea** (`rea-agents`, MCP + CLI) with Ghidra backend |
| Disassembler/decompiler | **Ghidra 12.1.x** + JDK 21 (headless + GUI) |
| Run the original as reference | **Wine** (32-bit prefix) on Linux, possibly dgVoodoo/cnc-ddraw for DirectDraw |
| Runtime observation | `winedbg`/x32dbg under Wine, memory dumps of the BSS region, savegame diffs |
| Format analysis | Python 3 (+ `construct`/Kaitai Struct), ImHex with pattern files |
| Quick static checks | objdump, rizin (optional) |

**Risk**: The rea documentation does not explicitly mention 32-bit PE (Ghidra itself supports it fully).
The first technical step is therefore a feasibility test "rea opens `1602.exe`". Fallback: Ghidra headless with
our own scripts, rea for the remaining targets.

### 3.3 Procedure
1. **Assets first** (quick wins, independent of the binary): `.cod` decoder, BSH+COL → PNG, ZEI fonts,
   SCP/SZS/GAM chunks, GAD parser. Result: asset viewer and static map renderer.
2. **Function inventory** of `1602.exe`: record all functions, assign them to subsystems via imports, strings
   (`gaddata\*.gad`, `%sSavegame\%s.gam`, …) and cross-references.
3. **Reconstruct global data structures**: map/island fields, building, figure, ship, player and
   storage tables. Compare with savegame chunks and runtime dumps under Wine.
4. **Understand the simulation**, subsystem by subsystem: game loop & tick, building, production chains,
   population/needs/taxes, trade & routes, ships/pathfinding, AI, military, pirates, events,
   mission goals.
5. **Parity tests**: load savegame → N ticks in the original (Wine) and in the new engine → compare state.
6. **Bug catalogue** (`docs/re/bugs.md`): document bugs of the original with their cause; fix in the engine,
   switchable via an "original behaviour" option, so that parity stays measurable.

## 4. Technology proposal

| Area | Choice | Rationale |
|---|---|---|
| Engine language | **C++20** | The original is MSVC 6 C++ → structures/classes map directly; proven in comparable projects; maximum performance; large ecosystem |
| Build | **CMake** + presets, **vcpkg** (manifest) | Linux/Windows/macOS from one build |
| Platform | **SDL3** (window, input, audio, timer) | one abstraction for all systems |
| Rendering | SDL3 GPU API (Vulkan/Metal/D3D12) with 2D sprite batching; software fallback | high resolutions, zoom, scaling |
| Audio | SDL3 audio + own software mixer, own Smacker decoder for videos | |
| Network | Deterministic **lockstep** over UDP (**ENet** or GameNetworkingSockets) | replaces DirectPlay; low bandwidth |
| Modding (later) | Lua 5.4 (sol2) + own data format (TOML/JSON) as an overlay to the `.cod` files | Extensibility without recompiling |
| Debug UI | Dear ImGui | Inspectors for the sim state |
| Tests | Catch2 / doctest, golden-file and parity tests | |
| RE/asset tools | **Python 3** | fast prototyping of format parsers, Ghidra scripts |
| CI | GitHub Actions (Linux, Windows, macOS), sanitizer builds (ASan/UBSan) | Stability |

Alternative **Rust**: stronger memory safety, but a weaker mapping of the original C++ structures and
a smaller game-development ecosystem. The recommendation remains C++20 with strict rules (RAII, no raw
owning pointers, sanitizers in CI).

### Architecture principles (performance, stability, multiplayer)
- **Simulation strictly separated** from rendering/platform: `sim/` knows neither SDL nor graphics.
- **Deterministic**: fixed tick, integer/fixed-point arithmetic only, own seedable RNG, no
  dependence on the iteration order of unordered containers.
- **Command-based**: player inputs become serialised commands → at the same time the basis for
  multiplayer lockstep, replays, undo in the editor and headless tests.
- **Desync detection**: state hash per tick in multiplayer.
- **Memory**: pools/arenas for entities, no allocations in the tick hot path → stable in long sessions.
- **Data-driven**: buildings/goods/units from `.cod` data, not hard-coded.
- **No fixed limits of the original in the engine model** (ADR 0007): number of players, banners (colours and
  flags), world size, island size and number of islands are open. The values of the original apply only when
  loading and saving the original formats.

## 5. Project structure (`Projekt Anno/`)

```
Projekt Anno/
├── README.md, LICENSE, CMakeLists.txt, CMakePresets.json, vcpkg.json
├── docs/
│   ├── 00-plan.md              ← this document
│   ├── architecture/           architecture decisions (ADRs)
│   ├── formats/                bsh.md, col.md, zei.md, cod.md, gad.md, scp.md, szs-gam.md, smk.md
│   └── re/                     subsystems.md, structs.md, game-loop.md, economy.md, ai.md, bugs.md
├── re/                         reverse-engineering workspace
│   ├── symbols/                functions.csv, globals.csv, types.h  (source of truth, versioned)
│   ├── scripts/                Ghidra scripts, rea workflows, export/import of the symbols
│   ├── runtime/                Wine harness, memory dumper, savegame diff
│   └── notes/                  working notes per function/subsystem
├── tools/                      Python: cod-decode, bsh2png, scp-dump, gad-parse, save-diff
├── engine/
│   ├── core/                   logging, file system, config, fixed point, RNG, memory pools
│   ├── platform/               SDL3: window, input, timing
│   ├── assets/                 loaders for all original formats (standalone library "libanno")
│   ├── sim/                    deterministic simulation: world, islands, buildings, economy,
│   │                           population, trade, figures, ships, AI, military, scenarios
│   ├── render/                 isometric sprite renderer
│   ├── audio/                  sound, music, speech, videos
│   ├── ui/                     GAD-based interface
│   ├── net/                    lockstep multiplayer
│   ├── script/                 modding (later)
│   └── app/                    main, game states, menus
├── tests/                      unit/, golden/, parity/
├── third_party/
└── .github/workflows/
```

Original data is **not** copied into the repository; the engine gets the path to the installation
(here `~/Games/Heroic/Anno 1602`) via configuration.

## 6. Milestones

| # | Milestone | Result |
|---|---|---|
| M0 | Setup | Repo, toolchain, rea+Ghidra feasibility test, CI skeleton |
| M1 | Assets | All formats documented, `tools/` + `libanno`, asset viewer |
| M2 | Map renderer | Load a scenario/savegame and display it statically in isometric view (3 zoom levels) |
| M3 | Core simulation | Building, production, population, storage, taxes – parity tests against the original |
| M4 | Playable endless game | Ships, trade, figures, UI, saving/loading (compatible with `.GAM`) |
| M5 | Complete game | Military, pirates, world generator stage 1, AI, events, scenarios/mission goals (already in the extensible model: factions and scaling with the number of players, ADR 0007) |
| M6 | Multiplayer | Lockstep, lobby with any number of players and flag choice, desync detection, reconnect |
| M7 | Improvements | Bug fixes, arbitrary resolutions, free zoom, hotkeys, UI scaling, editor |
| M8 | Content and modding | Content packs with manifest, content registry with identifiers, PNG/OGG import, Lua scripts, campaigns, custom island shapes (world generator stage 2, ADR 0007) |

**Status of the milestones (2026-10-18)**, details in [status.md](status.md): M0 ✅ · M1 🟡 (open:
parts of the island header and scenario chunks; fonts, layouts, sound and videos done) · M2 ✅ ·
M3 ✅ · M4 🟡 (simulation, ships, trade, savegames and sound done; foundations of the interface are in place, what is missing is drawing it and the windows) · M5 🟡
(military, pirates, world generator stage 1, AI and missions with victory and defeat done; open:
campaign progress, high score list, AI switches of the scenarios; scenarios have no events) · M6 🟡 (lockstep, desync detection with resync, reconnecting, lobby, LAN search, Open1602's own multiplayer menu, 4 to 12 players; open: banner choice) · M7 ⬜ (some improvements
already in place: mouse-wheel zoom, screenshot, fixed bugs) · M8 ⬜ (banners with tinted flags already in place).

## 7. Next steps

**Agreed order (2026-10-17):** first complete the game content, then multiplayer.
Every new system stays deterministic and command-based, so that multiplayer can later build on it
without rework.

1. **Ships** ✅: sailing with pathfinding at sea, loading and unloading, trade routes, free trader, shipyards.
2. **Military** ✅: diplomacy (peace and trade treaties, events, mood, tribute), soldiers
   and combat, shooting towers, naval combat with broadsides and sinking, building damage, training in the
   castle, embarking, conquest via a destroyed warehouse, curse of the natives
   ([military.md](re/military.md), [diplomacy.md](re/diplomacy.md)). Castles do not shoot in the original,
   and hits do not start fires. Open: windows for this in the interface (step 5).
3. **Pirates and natives** ✅: pirate ships by player level and difficulty, searching for loot,
   white flag and plundering into the nest, selling in the nest, mood and war every 5 s, buying peace and
   bribery; goodwill, consumption and warriors of the natives ([pirates.md](re/pirates.md)). The
   unit limit is a rule (`Rules::max_army`, default 250, original 100; L-008), as is the
   fleet limit (`Rules::max_ships_per_player`, default 100, original 33; L-001). Founding a nest and
   the computer players' diplomacy with the pirates came with the AI (step 5).
   **Shipping completed** (2026-10-17): speed, diagonals and turning checked against the video of the original,
   exploring islands with ore and gold symbols, damaged hulls, bow wave, white flag and
   sinking in the display, town tick at every full 10th second ([ships.md](re/ships.md)). Only
   windows of the interface (step 5) and the muzzle smoke remain open.
4. **World generator, stage 1** ✅: custom worlds for every map size (200–2000 fields per side) and
   number of players: number of islands and mix of sizes by area, starting areas with a home island and equally large
   neighbouring islands, climate by position with both climates in reach, iron on every home island, all
   plants and gold in reach, villages and volcanic islands by number of islands and players; larger worlds in
   their own chunk `O1602MAP` ([worldgen.md](re/worldgen.md#custom-worlds-world-generator-stage-1)). Open:
   pirate strength by number of players, start dialogue (step 5). Up to twelve players (2026-10-19).
5. **Computer opponents (AI)** ✅: task stack, levels and needs as in the original, building site search with
   road plan, maintenance, settling and colonies, supply routes and patrols, fleet, trade, recruiting,
   towers and castles, attacks with landings, island soldiers, diplomacy, pirate nest; deterministic on
   every machine, saved with the savegame ([ai.md](re/ai.md), S-095 to S-102). In test runs without an
   active human, the computer players reach merchants with 500–870 inhabitants after six hours. Open:
   AI switches of the scenarios and the ship missions from original savegames. Next: events
   (volcanic eruptions of the scenarios), build menu, start dialogue with all settings, the rest of the interface.
   **Done after the inventory (2026-10-18):** own save format `.o1602` without the limits
   of the original ([ADR 0009](architecture/0009-own-save-format.md)); more expensive water near enemy
   towers; town and ship windows as commands (tax rate, name, material, cannons, overboard,
   ship sale); trade settings by `Maxware`; `Grundflg` in pathfinding; missions of the
   computer ships from original savegames; reaction of the computer players to a founding. After that the
   **missions of the scenarios** ([missions.md](re/missions.md)): all 18 goals and both
   loss conditions, victory the same on every machine, defeat with no town and no ship, bankruptcy,
   computer players and pirates, triumphal arch for the culprit, trade statistics (S-105 to S-111).
   Scenarios have no events or scripts (confirmed).
6. **Multiplayer** including the endless game (`.szm`).
7. **World generator, stage 2** (deferred until everything else is done and running): custom island shapes,
   including ones larger than the templates, with coast, beach, mountain and river transitions.

Being able to load original formats and continue standard scenarios in the original is desirable, but not
a requirement: extended features of Open1602 do not have to be compatible with the original.

## 8. Decisions and progress

**Decisions (2026-10-16):** GPL-3.0-or-later · C++20 · reimplementation instead of decompiled code ·
project name *Open1602* (namespace `o1602`) · details in [docs/architecture](architecture/README.md).

| Step | Status |
|---|---|
| M0: Git repository, CMake presets, CI (Linux/Windows/macOS, ASan), ADRs | ✅ done |
| M0: rea + Ghidra feasibility test | ✅ passed (rea 6.5.0, Ghidra 12.1.2) – `re/notes/rea-smoke-test-result.md` |
| M1: chunk container, `.cod`, palette, BSH sprites | ✅ documented, implemented, tested against original data |
| M1: islands (`.SCP`) | 🟡 header (size, position, climate, deposits, switches) and field lists of both layers; open: attribute bits 26–31 and the rest of the header (carried along raw) |
| M1: `.cod` interpreter (grammar from the exe, 685 predefinitions, parser + tests) | ✅ `docs/formats/cod.md` |
| M1: building record (`haeuser.cod` → `g_haeuser`), island attributes, placement algorithm | ✅ `docs/re/haus-record.md` |
| M2: renderer foundation: sprite formula, painter's algorithm, software renderer, `render-island` | ✅ `docs/re/rendering.md` |
| M2: world map (500 × 350, open sea, animation clock), sprite atlas, camera, `render-world` | ✅ `docs/re/rendering.md` §5 |
| M2: SDL3 map viewer `open1602` (scrolling, zoom at the mouse, rotation, HiDPI) | ✅ `docs/setup-arch.md` |
| M2: savegames (`.GAM`) in the viewer, players (PLAYER4), player colours, ships with flags, ownership view | ✅ `docs/formats/gam.md` |
| M2: real savegame checked (Merlin01): two island layers, global chunks, names from `Game.dat`, flag choice confirmed | ✅ `docs/formats/gam.md` |
| M2: flags on warehouse, marketplace and tower in the owner's colour | ✅ `docs/re/rendering.md` §6 |
| M2: comparison with a screenshot of the original (Merlin01, SGFX): correlation 0.955 | ✅ `docs/re/rendering.md` §8 |
| Requirements for extensibility and modding (ADR 0007); engine model without a fixed number of players, banners instead of fixed colours, world size per map | ✅ `docs/architecture/0007-extensibility-and-modding.md` |
| Savegame analysis (Merlin01 played on): all chunks with write routine and runtime table; towns with names, warehouse stocks, production list, deer loaded; SHIP4 direction corrected | ✅ `docs/formats/gam.md` |
| M2: figure animation system (ANIM), animals with shadows, figure sheets per zoom level, figures in SGFX (can be switched off) | ✅ `docs/re/figures.md` |
| Player flags as a tinted standard flag (any colours, 12 presets, guide for modders) | ✅ `docs/modding/flags.md` |
| M2: production state from the savegame: building animations by state (mills, fill level, market stalls), smoke, status symbols; figures at reference point Y as in the original; comparison with the screenshot now 0.967 | ✅ `docs/re/production.md` |
| Check: soldiers in any player colours (template from the four versions, original images for the four original colours) | ✅ `docs/re/figures.md` |
| M2: soldiers from the savegame (`SOLDAT3`), in every banner colour (original images for the four original colours, otherwise a tinted template) | ✅ `docs/re/figures.md` |
| M3: simulation in fixed ticks (`sim::Simulation`), production cycle as in the original (raw material, rate, interval, storage, idling, statistics, resupply requests), viewer lets time run | ✅ `docs/re/production.md` |
| M3: logistics, stage 1: town storage (stock, reservation, storage space), pathfinding within the catchment area, carriers and market carts with movement and animation; bugs of the original S-022–S-025 fixed | ✅ `docs/re/logistics.md` |
| M3: logistics, stage 2: growth of fields (class clocks, `ROHWACHS2`), farmers, herds, fishing boats, deposits of the mines | ✅ `docs/re/growth.md`, `docs/re/logistics.md` |
| M3: building and demolishing as simulation commands: checks (terrain, rotation, territory, units, unlocking, fertility), costs in order of the request, registration in all lists, adapting roads to neighbours, territory of marketplace and warehouse, founding a town from a ship, demolition with territory check; build levels in the town tick; build tool in the viewer (B, X) | ✅ `docs/re/construction.md` |
| Game settings in `sim::Rules` (ADR 0008), first choice: refund on demolition (none, partial, full) | ✅ `docs/architecture/0008-game-settings.md` |
| M3: drought (roll by difficulty, withering, recovery, harvest, setting `Rules::droughts`) | ✅ |
| M3: fires, plague, robbers: town schedule, spreading, burning down, doctor and fire brigade, robbers with pursuit, display | ✅ `docs/re/disasters.md` |
| M3: volcanoes: crater, roll, eruption with 141 lava bombs, fire and destruction, display, setting `Rules::volcanoes`; S-042 fixed | ✅ `docs/re/volcanoes.md` |
| M4: writing savegames that the original loads: all chunks in the order of the original, records from the source file with Open1602's state, WIFF; written unchanged it is the original file byte for byte, played on it loads to the same state; F5 / `--save`, never into the installation | ✅ `docs/re/savegame-writer.md` |
| M4: own chunk `O1602` after `WIFF`: settings, randomness, clocks, all lists, fires, lava, figures, deviating fields; fingerprint of the original chunks; a saved game continues exactly as without interruption | ✅ `docs/formats/o1602-chunk.md` |
| M4: new endless game as in the original: world plan and island table (`SZENE`, `RANDTAB`), templates, villages, plants, ore, treasures, starting state after a new start; `open1602 --new-game D`; islands optionally from the whole seed; S-019 fixed (rule), S-067; check in the original is pending | ✅ `docs/re/worldgen.md`, `docs/re/newgame.md` |
| M3: consumption of the inhabitants from storage (town tick `0x0047F8A0`): demand from haeuser.cod, supply levels, satisfaction with tax curve, messages; inhabitants from `STADT4`, accounting from `MARKT2` | ✅ `docs/re/consumption.md` |
| M3: houses (`SIEDLER`), supply by marketplaces and public buildings, growth, rising and falling, ruins; taxes, operating costs, pay; random sources per subsystem, pirates | ✅ `docs/re/houses.md` |
| M3: deer and hunters: game spots, counting, return, grazing, hunter's call, shot, prey | ✅ `docs/re/deer.md` |
| M3: hunting checked: counting rule against saved game spots, a placed hunting lodge hunts with original data; Merlin02 → Merlin03 (with video) runs field by field as in the original (deer's target choice with the wave search, game spots, statistics of the lodge) | ✅ `docs/re/deer.md` |
| M2: life in the picture: walkers, whales, dolphins, krakens, jugglers, sound sources; display only, own randomness, no influence on the game (S-065) | ✅ `docs/re/flavor.md` |
| M2: decorative figures the same on all machines without network traffic (from seed, game time and world, not from camera and frame rate) | ✅ `docs/re/flavor.md` |
| M2: land figures with height (0.028 · Posoffs of the field they appear on) and field centre as in the original, sorted by Y within the row; no clipping by neighbouring fields; encounters without evasion as in the original | ✅ `docs/re/figures.md` |
| M2: all figures pixel-exact to the fields (clipping as in the original, one row offset of the field rows compensated), checked with a screenshot; „abgeschaltet“ ("switched off") symbol above shipyards | ✅ `docs/re/rendering.md` |
| M4: ships: sailing from field centre to field centre with turning on the spot, speed by damage and cargo, pathfinding at sea in a window with detours around other ships, arrival at the warehouse within radius 5, discovering islands; loading and unloading at own and foreign warehouses with prices and limits, trade with natives for affection; trade routes with four stops; free trader (arrival at the map edge, target choice by profit, buying and selling, ageing, tools out of nothing); shipyards with pack mule, building and repair; all saved (SHIP4, HANDLER, WERFT, KONTOR2, own chunk); sail command in the viewer; S-005, S-013, S-070–S-083 | ✅ `docs/re/ships.md` |
| M2: workers and animals hidden by fields and trees as in the original (woodcutter invisible while felling, mower knee-deep in the grain); ship checked with a screenshot | ✅ `docs/re/figures.md` |
| Bug catalogue of the GOG edition (initially 51, now 164 entries), ADR 0006, `sim::Rules`, CI check of the references | ✅ `docs/re/bugs.md` |
| M5: diplomacy and military (treaties, soldiers, combat, towers, castles, naval combat, conquest) | ✅ `docs/re/military.md`, `docs/re/diplomacy.md` |
| M5: pirates and natives, unit and fleet limits as rules | ✅ `docs/re/pirates.md` |
| M5: world generator stage 1 (custom worlds 200–2000 fields, `O1602MAP`) | ✅ `docs/re/worldgen.md` |
| M5: computer opponents (deterministic, saved, S-095 to S-102) | ✅ `docs/re/ai.md` |
| Inventory: what is done, open and unclear, in the original and in the extensions | ✅ `docs/status.md` |
| M5: missions of the scenarios, victory and defeat (S-105 to S-111) | ✅ `docs/re/missions.md` |
| M1: ZEI fonts, GAD | ✅ `docs/re/ui.md`, `docs/formats/gad.md` |
| M1: remaining scenario chunks | ⬜ |
| RE: first symbols (`WinMain`, `Game_Init`, `.cod` loader, `Text.cod` decryption) | ✅ `re/symbols/` |

## 9. Open points by dependency

The complete overview (done, open, needs clarification, extensions) is in
[status.md](status.md); what I need for it in terms of savegames, videos and observations is in
[requests.md](requests.md). Here only the points that are waiting for another system. When that system
arrives, they are ticked off.

**Saving**
- ~~More players, ships, soldiers and towers than the original can hold~~: own format `.o1602`
  ([o1602-save.md](formats/o1602-save.md)).
- List of savegames (section `META`) in the load menu of the interface.
- Original savegames take the values of the original for the options: fulfilled, because the default
  of the options is the original ([ADR 0008](architecture/0008-game-settings.md)); savegames of
  Open1602 bring their settings with them.

**Scenarios**
- ~~Mission goals, victory and defeat~~: done ([missions.md](re/missions.md)). The scenarios of the original
  have no events.
- Campaign progress (own file instead of `Game.dat`), high score list and points, end screen with message
  and video: with the interface.
- AI switch (player +0x70), scenario stance of diplomacy (`PLAYER4` +0x1C/+0x20).

**Interface (build menu, windows, messages)**
- Build menu of the original (GAD) with building names from Text.cod, unlocking shown as greyed-out
  buttons ([construction.md](re/construction.md)).
- Windows for warehouse, cargo holds, trade routes, shipyard, castle, diplomacy, units, statistics;
  health bars; formation choice.
- Warning when demolishing a marketplace that removes the basis for houses (S-035).
- Start dialogue for the endless game with all game settings ([ADR 0008](architecture/0008-game-settings.md)).
- Look at U-013 again (exploring at a shipyard and a pirate warehouse).
- Idea: full refund for a few seconds after building (protection against misclicks); for this
  every building needs its construction time.
- Idea: own behaviour of the road pathfinding alongside that of the original.

**Display**
- Muzzle and shot smoke, hits, wreck (SHIPSUNK); shipyard symbols „Material fehlt“ ("material missing") and „Brand“ ("fire");
  decorative figures only on discovered islands; brightness and colour-cast tables.

**Sound and videos**
- ~~Sounds, music, speech~~: done ([audio.md](re/audio.md)).
- ~~Smacker videos~~: done (2026-10-18, [videos.md](re/videos.md), [smk.md](formats/smk.md)); open only what depends on the
  main menu (intro button, credits) and video 2990 with the campaign progress.

**No dependency**
- ~~Extend the symbol table `re/symbols/`~~: done (532 functions).
- Done on 2026-10-18: trade limits when upgrading the warehouse, urgent goods (S-104), water near
  enemy towers (S-103), town and ship commands, missions from `SHIP4`, reaction of the AI to a
  founding, `Grundflg` in pathfinding.

**Multiplayer**
- How other machines take over a house upgrade (message 0x835, [houses.md](re/houses.md)).
- ~~Commands over the network, state hash, lobby and menus~~: done (2026-10-18, ADR 0011; fingerprint every 100 frames, Open1602's own menu, LAN search). Still open: banner choice in the lobby. Up to twelve players since 2026-10-19.

**Analysis only**
- Bits +0x0F/4 and /6 of production ([production.md](re/production.md)).
- Second list at island +0x30 ([growth.md](re/growth.md)).
- Further points in [status.md](status.md#13-needs-clarification-behaviour-of-the-original-not-confirmed).
