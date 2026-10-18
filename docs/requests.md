# What Open1602 needs from you

Status: 2026-10-18 (ships, military, pirates, custom worlds and the computer opponents are in place; next come events and the interface). What still lacks an observation in the original is collected in [status.md](status.md#13-needs-clarification-behaviour-of-the-original-not-confirmed). This list collects everything that I (Claude) need from you, or have already received, in terms of savegames, images,
videos and observations in the original. It is updated
as soon as something is added or done.

## How to give me something

- **Savegames:** Please save in **new slots** (from „Leer03“ ("Empty03") on), never over Merlin01, Merlin02
  or Merlin03: the tests compare exactly these savegames with Open1602. Give the savegame a
  descriptive name (e.g. „VorVulkan“ ("BeforeVolcano")) and write me a short note on what you did before.
- **Handover:** Nothing to copy. The installation folder under Heroic is shared with me;
  I read `SAVEGAME/` and `Game.dat` there directly (read only, the installation stays unchanged).
  Just tell me the name of the savegame.
- **Videos and screenshots:** into the folder `eingaben/` in your project folder "Projekt Anno".
  I read the folder directly; Git does not include it (original data never goes into the repository).
  Screenshots preferably at the lowest graphics level (SGFX) and 1024 × 768, like the first one.
- **Game time:** The original counts game time, not real time. At normal speed they are
  the same; at a faster speed it passes correspondingly faster.

## Already delivered

| What | What for | Status |
|---|---|---|
| Access to the GOG installation under Heroic (graphics, `.cod` files, scenarios, savegames, music `MUSIC8`, sounds `SAMPLES`, speech `Speech8`, videos `VideoSmk`) | Basis for everything | ✅ fully readable |
| Merlin01 (`game00.gam`) and `lastgame.gam`, also in the first version that was later overwritten | Savegame format, towns, storage, production, houses, consumption, money | ✅ both versions are tested |
| Screenshot of the original (Merlin01, SGFX, 1024 × 768) and one with shut-down production buildings | Comparison of graphics, flags, status symbols | ✅ correlation 0.967 |
| Confirmations in the running game: warehouse stocks of Lemonhall, antelopes and deer, flag choice | Units of the stock amounts, animals | ✅ |
| Merlin02 and Merlin03 with a hunting lodge, plus the video `Anno1602_Jagd.mp4` | Hunting and deer; Merlin02 → Merlin03 runs as a field-by-field test | ✅ |
| Feedback on building in the viewer (invisible preview) | Build tool | ✅ fixed; your confirmation is pending (below) |
| Test in the original: `Open1602Test.gam` and `Open1602Test2.gam` (with extra section) loaded and played on, without a crash; then `Merlin04_Laufend` (67:20, with a spice plantation) | Writing savegames, own chunk | ✅ (gold after loading 3219 instead of 3206: probably the first town tick) |
| Test in the original: `Open1602Endlos.gam` (an endless game created by Open1602) loaded and played | New game in the original format | ✅ |
| Endlos1 and Endlos2 (`game06.gam`, `game07.gam`): two endless games of the original directly after the start („Durchschnitt“ ("Average"), Merlin, yellow) | Byte-for-byte comparison of the new start | ✅ islands, players, towns, warehouses and marketplaces match; the original re-rolls the island table at every start |
| Video `Schiffe_Fahren.mp4` (92 s, 4K) | Speed and turning of the ships | ✅ evaluated: speed of all ship types within ±1 % of the calculation, diagonals √2 longer, turning as assumed; in addition the town tick at every full 10th second and an extra one on loading ([ships.md](re/ships.md#comparison-with-the-original)) |
| Routenschiff_Vor and Routenschiff_Nach (`game08.gam`, `game09.gam`): a free trader shortly before and directly after docking at your own warehouse | Trade of the free trader | ✅ the trade matches exactly (7 t of tools at 105 each to Lemonhall); the computer players keep the same money in the same tick; small deviations only in the course of individual ships |
| Endlos_Laufend (`game10.gam`): two islands, first merchants, training in the castle, one cannoneer ready | Military in the savegame | ✅ loads and is written back byte for byte, including the units in the castle |

## How you get something from me

Files that I create for you (for example savegames to try out in the original) I put into
the folder `ausgaben/` in your project folder. Git does not include it.

## Useful now

Ordered by benefit. None of it is urgent; everything carries on without it too.

1. **Custom world in the original** (checks the world generator, stage 1). In `ausgaben/` there is
   `Open1602EigeneWelt.gam`: a world planned by Open1602 in original size (18 islands, four players with
   separate starting areas). Please copy it like the earlier test files as a free savegame into `SAVEGAME/`,
   load it in the original and play for a few minutes: Does it load without a crash, are the four
   ships at different places, and do the islands look normal? In the viewer the same works with
   `open1602 --new-game 1 --world 500x350` (or larger, e.g. `--world 900x600 --players 2`).

2. **Your own trade route** (optional; video and trader savegames are already here). Two savegames with
   your own trade route: one directly before arriving at a warehouse („VorKontor“ ("BeforeWarehouse")), one shortly after
   („NachKontor“ ("AfterWarehouse")). With these I check loading and unloading by route.

3. **Feedback on the viewer.** Does the camera start above Lemonhall, and do you see the preview when
   building (red area where nothing is possible)? If you see no houses in your own town, send me
   the lines `renderer: …` and `GFX: …` from the terminal. New: left-click selects a ship (name,
   speed and cargo are shown in the title bar), right-click sends it off (onto the sea, to a warehouse,
   to another ship); dragging with the right button moves the map as before.

4. **Volcanic eruption** (checks the built-in volcanoes). In Merlin04 two volcanoes have already
   erupted (islands 5 and 6); that confirms the sequence. What is missing is a savegame directly before and after.
   The next eruption in Merlin04 is scheduled for **130:32 game time**; which of the three
   islands with a volcano it hits is random, none of them is yours.
   - one savegame shortly before (around minute 129), „VorVulkan“ ("BeforeVolcano");
   - one shortly after the message „Ein Vulkan ist ausgebrochen!“ ("A volcano has erupted!"), „NachVulkan“ ("AfterVolcano") (the savegame shows the
     next date and which crater it was);
   - one about a minute after that, „NachVulkan2“ ("AfterVolcano2") (destroyed trees and buildings);
   - if possible a video of the eruption: how long it lasts, how far the bombs fly.

5. **Drought** (checks the built-in drought). In Merlin04 the drought roll already hit once between minute 88 and 95
   (on which island, the savegame no longer shows); the next one is possible from
   **122:00 game time**. So that there is something to wither, Lemonhall needs fields or
   plantations; your spice plantation is a start, more help.
   - one savegame during the drought (after „Die Trockenheit vernichtet Eure Ernte!“ ("The drought is destroying your harvest!")), „Duerre“ ("Drought");
   - one about two minutes after it ends (there is no message for that; the withered fields
     then grow again), „NachDuerre“ ("AfterDrought").

6. **Two savegames in quick succession** (for the test of the town tick with money and consumption). Of the
   new savegames, no two are less than 10 seconds of game time apart. Load a
   savegame, save immediately as „TaktA“ ("TickA"), let the game run for about 5 seconds and save as
   „TaktB“ ("TickB"). The larger the town, the more this checks.

7. **A larger town.** For fires, plague, robbers and the higher inhabitant classes:
   - Fire: from about 80 pioneers or 250 pioneers and settlers;
   - Plague: from 200 settlers and higher; citizens, merchants, aristocrats in half-full houses;
   - Robbers: from 300 citizens and higher.

   Savegames with citizens and merchants check consumption, satisfaction and house upgrades of the
   higher classes. Fires and epidemics themselves are not stored in the savegame of the original; only
   a **video** helps here: a fire with the fire brigade arriving, a plague with the doctor, very rarely a robber
   (he robs carriers and market carts on their way home). How long a fire lasts and where it
   jumps to can be seen well on it.

8. **Palace or cathedral**, if you get that far: one savegame immediately before building it and one
   about a minute after. The difference in satisfaction checks the "pride" of the town.

9. **Military** (checks training, combat and conquest, which I am building in right now). Build a castle,
   train two swordsmen and two musketeers (swords and muskets must be in the warehouse)
   and attack a native village (attack cursor on a hut), if you get that far. In addition:
   - one savegame with the fully trained units still in the castle, „Burg“ ("Castle");
   - one shortly after the attack order, when the warriors of the village come out, „Angriff“ ("Attack");
   - one after the fall of the village (message „Die Eingeborenen verfluchen Euch!“ ("The natives curse you!"), if it comes),
     „DorfZerstoert“ ("VillageDestroyed");
   - a video of the fight: how fast the units strike and shoot, how long a hut
     holds.

## Later, when the systems arrive

| When | What | What for |
|---|---|---|
| Military (beyond point 9) | a savegame with a destroyed warehouse and a watchtower next to it (S-018), a video of a naval battle with warships | Loss of territory, broadside and speed of naval combat |
| Computer opponents (useful now) | savegames of an endless game after about one and after three hours, so that I can see how far the computer players get in the original; plus a short note on how much you built yourself (the computer players get subsidies when they are behind you); a short video of computer ships sailing is welcome too | Compare speed and strength of the AI |
| Multiplayer | only once Open1602 can do networking | – |

## Only when convenient

Points that were inferred from the code or never seen in the game. An observation in the original
would confirm or refute them; none is urgent (Open1602 avoids the bugs among them
anyway).

- **U-007 (old path when dragging):** On an island with two of your own towns: drag a road in town A,
  then start dragging in town B and move the mouse to town A. Does the
  displayed path start at the old starting point in A?
- **Rotated view:** a screenshot of the original in one of the three rotated views (same
  spot as the first screenshot), to check the rotation in the viewer.
- **Units in training:** Does the castle window of the original let you send out a unit before its
  training is finished? Open1602 does not.
- **Fire at a shipyard:** Does a burning shipyard keep building its ship?
- **S-018 (watchtower holds territory):** clarified from the code and kept: a town can only be
  conquered once the warehouse, marketplaces and towers have fallen. A savegame with a destroyed warehouse and a
  standing tower would confirm it in the game.
