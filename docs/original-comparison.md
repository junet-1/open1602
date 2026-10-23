# Comparison with the running original

Observations of the original *Anno 1602* (German GOG "Königsedition", `1602.exe`) running under
Proton, compared with Open1602 on the same savegames. Date: 2026-10-20. Open1602 was built from
`main` at `b3df3f2` (#55); the simulation results were identical to `2aaf423`.

This document lists the differences that were found, what the original does where the notes still
had open questions, and what Open1602 should change. The items are ordered by priority. Each one
names its evidence; addresses are virtual addresses in `1602.exe`.

## Summary

| Section | Priority | Topic | Result |
|---|---|---|---|
| 1 | High | Pirates and computer ships | **Difference.** Open1602's pirates stay at war with the computers and sink all their ships; the original makes peace with everyone after a surrender. Cause found in `Fleet::plunder`. |
| 2 | High | Tutorial (`TIMERS` +0xA5) | **Difference.** The flag is the tutorial mode; in the original the computer opponent of the tutorial does nothing. Open1602 ignores the flag and the computer builds. |
| 3 | Medium | Speed and strength of the computers | **Mostly the same in the first two hours**; afterwards Open1602's computers keep growing where the original's stall, and they found their second town earlier on "hard". The original's computers build warships after about 100 minutes, Open1602's did not in the same world. |
| 7 | Medium | Disasters in large towns | **Difference.** In the original, fires in a computer's large town burned their full time, again and again; in Open1602 every fire and plague ended within three epochs. |
| 4 | Medium | Rendering in rotated views | Rotation order matches; Open1602 darkens areas the original does not (for the thread "Darstellung, Ton und Videos"). |
| 5 | Low | Drought | Same: strength 0x40, duration halved for a computer island, recovery 64 → 32 → 16 → 0 per 30 s. |
| 6 | Low | Volcano | Same: an eruption runs about 28 s, the next roll comes about 26 minutes later. |
| 8 | Low | Shipyard orders, units in training | Same as Open1602. The shipyard window prevents two of the three fleet counter errors of S-094; units in training cannot be sent out. |
| 9 | – | Open analysis items | Message 0x48, production bits, `Destroyflg`, city store fields: answered, several notes are out of date. |

## Follow-up

Status 2026-10-22. What is still open is also listed in [status.md](status.md), section 1.4.

| Section | Status |
|---|---|
| 1 Pirates | fixed in #58: the surrender events go out loot or not |
| 2 Tutorial | fixed in #58: the computers of the tutorial stay idle (`TIMERS` +0xA5, savegame section `TUTR`); the tutorial windows, bubbles and speech followed ([tutorial.md](re/tutorial.md)) |
| 3 Computers | warships: #58 plans a shipyard whenever a computer wants ships, not yet checked again in a long run; the original's computer players are a setting (`Rules::classic_ai`), Open1602's stay the default |
| 4 Rendering | not a difference: Open1602 was in build mode in that screenshot, which dims everything outside the own territory |
| 7 Disasters | not a bug: the long fires of the original come from S-050 (a burning house catches fire again) and S-053 (the fire brigade also goes out for the plague), which Open1602 fixes on purpose |
| 8, 9 | the notes are updated (#56, #12 and #13 of `open1602-re`) |
| Other observations | the tool prints names as UTF-8 (#56); the driver `tools/original/original_wine.py` is not merged yet |

## How the original was observed

- The original ran headless in Xvfb through `tools/original/original_wine.py` (branch
  `claude/original-wine-harness`), started from a small host process that sets itself as child
  subreaper. That keeps it the ancestor of `1602.exe`, so with Yama `ptrace_scope` 1 it may read
  and write `/proc/<pid>/mem`. Windows addresses are Linux addresses under Wine, so the game's
  globals could be read live (game time `0x005B6040`, players `0x005B7680` + 0x280·p, cities
  `0x005DBAE0` + 0x258·i, ships `0x004CF358` + 0x218·i, islands `0x005E6B20` + 0xB00·i,
  disasters `0x005A5100`).
- One instance ran in Merlin's installation (the 3-hour endless game). Three more ran in copies of
  the prefix and the game folder, so that loading Merlin's savegames and poking timers changed
  nothing in the installation. The savegames, `Game.dat` and the registry files were backed up
  first and restored afterwards.
- The original runs at game speed 1 in real time. Under Wine it draws a frame every 1–3 ms
  (`0x005B315C`), so its computers get their full 50-ms step; S-095 (computer speed tied to the
  frame rate) does not slow them on a current machine.
- Open1602 was run headless from the same savegames with a small driver that loads a `.gam`,
  advances `sim::Simulation` second by second with `Rules::original()` and writes `.gam` snapshots
  and the simulation's events. It is not part of the repository.
- Timers were poked to trigger events in the copies: the next drought (`0x005DBAA8`) and the next
  eruption (`0x005DBAA0`), the same fields as `TIMERS` +0xB4 and +0xCC.

Savegames, logs and screenshots are in the evidence archive that came with this report
(`original-comparison-evidence.zip`); the screenshots of the original are not in the repository.

## 1. Pirates sink the computers' ships in Open1602 (high)

**Observed.** Merlin's savegame "Open1602Test" (`game03.gam`, 48 min, difficulty 3) was played on
for an hour in both:

| Time | Original: pirates' peace with players 0–3 | Original: computer ships | Open1602: peace | Open1602: computer ships |
|---|---|---|---|---|
| 48 min (load) | 0 0 3 0 (war after loading, both) | 4 | 0 0 3 0 | 4 |
| 50.6 min | **3 3 3 3** | 4 | 0 0 0 0 | 4, damaged |
| 64 min | 3 3 3 3 | 7 | – | – |
| 78 min | – | – | 0 0 0 0 | 6 |
| 98 min | – | – | 0 0 0 0 | 1 |
| 108 min | – | – | 0 0 0 0 | **0** |

In the original the pirates make peace with all four players at once, two minutes after loading,
including the human who has no ships, and the computers keep their ships (and build three more).
Merlin's own later savegames show the same: the computers stayed at peace with the pirates
(`game04.gam` at 108 min, `lastgame.gam` at 258 min). In Open1602 the event log shows the cycle
for every computer ship: hit by a pirate, white flag, plundered with nothing taken, peace, war
again within five seconds, hit again; after 11 to 33 rounds the ship sinks.

**Cause.** The original's caller of the plunder routine (`0x00452BD2`) posts the events 9 (to all
players) and 6 (to the victim's owner) whenever the routine returns non-zero (`0x00452BE0`,
`0x00452BEF`). `0x00455840` returns non-zero whenever the pirates have a nest and the ship exists,
whether or not there was loot; it returns 0 only at `0x00455A17`. Afterwards peace is set with the
victim's owner in any case (`0x00452C42`). Open1602's `Fleet::plunder`
(`engine/sim/src/pirates.cpp`, the block after the cargo loop) adds `AnySurrender` and
`Surrendered` only when it found loot (`if (n != 0)`). Without them the pirates' mood stays at or
below 0, and `pirates_reconsider` declares war again at its next check.

**Recommendation.** Add the two events whenever the pirates have a base, like the original, and
keep the route only when there is loot. A test: a computer ship without valuable cargo surrenders,
and the pirates are at peace with every player afterwards and stay so for the event's lifetime.

## 2. Tutorial: the computer opponent must stay idle (high)

**Observed.** `TIMERS` +0xA5 (`0x005B62FC`) is 1 in exactly the five tutorial scenarios
`Einführungsspiel0–4.szs` and 0 in every other scenario and savegame of the installation. It is
written only by the new game (0, `0x0040E08F`) and by the loader (`0x00484EA4`); nothing in the
game sets it. It is read in about 25 places, among them:

- the computers' step (`0x00429AB6`, `0x00429AFA`: no root task, so no city development;
  `0x00429B3F`), the 6-s timer for recruiting and buying peace from the pirates (`0x0042B67A`), and
  two further computer routines that return at once (`0x00422150`, `0x004253F0`);
- a timed event of the game (`0x00489A3D`), the check after loading that marks players without
  ships and cities as absent (`0x0048C236`), and several windows of the tutorial and buttons.

In "Diplomatie & Handel" (`Einführungsspiel2.szs`, it has a computer opponent) the original's
computer spent nothing: its money rose from 19 652 to 20 236 in 5 minutes by taxes alone. Open1602
started building at once: 16 754 gold after 5 minutes, 9 777 after 30, 13 more production
buildings.

**Recommendation.** Read the flag into the world (it is already kept as a raw value for the
export) and, while it is set, skip the computers' root task, recruiting, pirate peace and the
timed pirate event, as the original does. The rest of the tutorial (its windows, the speech) is a
separate open item.

## 3. Speed and strength of the computers (medium)

Three comparisons, each from the same savegame in both games, inhabitants per computer (pioneers
and settlers unless stated):

**A new endless game, average** (started for this test in Merlin's installation, saved 1.4
minutes after the start; Open1602 ran from that savegame):

| Game time | Original: computer 1 / 2 / 3 | Open1602 | Remarks |
|---|---|---|---|
| 15 min | 26 / 24 / 20 | 24 / 29 / 32 | both bought peace from the pirates |
| 30 min | 46 / 40 / 36 | 44 / 37 / 74 | |
| 60 min | 124 / 116 / 113 | 135 / 99 / 144 | both at war with the pirates again |
| 90 min | 162 / 140 / 158 | 170 / 136 / 174 | second towns in both |
| 120 min | 218 / 165 / 221 | 288 / 188 / 265 | original: two warships (KRIEG1) |
| 135 min | 290 / 217 / 284 | 326 / 230 / 299 | |
| 180 min | 335 / 336 / 328 | 344 / 316 / 308 | original: 2 warships, 1 more trade ship; Open1602: none |

**Merlin03, difficulty 3** (`game03.gam` from 48 minutes on, the human idle):

| Game time | Original | Open1602 |
|---|---|---|
| 110 min | 348 / 336 / 324, 4 towns each | 348 / 336 / 328, 4 towns each |
| 170 min | 348 / 336 / 324 (unchanged since 100 min) | 532 (292 citizens) / 336 / 328 |
| 230 min | – | 758 (350 merchants) / 531 / 328 |

**Endlos_Laufend, difficulty 1** (`game10.gam`, 239 minutes): in the original the computers built
nothing in 57 minutes (143 houses before and after, money 24 402 → 31 546 and 25 040 → 29 020);
Merlin's own continuation (`lastgame.gam`, 258 minutes) shows the same standstill. Open1602
founded a second town for computers 1 and 3 within 20 minutes and built 60 houses, four fire
stations and five doctors in the hour. Computers 1 and 3 have one town of about 140 settlers there,
which is the state S-097 describes (no island found for the first colony, the town waits on stage
0x15 for ever).

**A new endless game, hard** (in a copy, saved 1 minute after the start):

| Game time | Original | Open1602 |
|---|---|---|
| 60 min | 132 / 140 / 76, one town each | 132 / 131 / 118, one town each |
| 120 min | 144 / 214 / 144; only computer 2 has a second town | 243 / 255 / 277, two towns each |

On "hard" the original's computers 1 and 3 sat on 144 settlers in one town from about 90 minutes
on; Open1602's founded their second towns between 60 and 90 minutes.

**Reading.** In the first two hours the pace matches closely, and at three hours the new average
game ends at the same plateau of about 330 settlers per computer in both. Open1602's computers are
a little ahead in between (up to 30 % more inhabitants at 120 minutes). In Merlin's two
savegames and on "hard" the original's computers stall, while Open1602's go on, mostly because of
the deliberate fixes S-097 (settling) and S-098/S-099 (counting and site search); in the new
average game Open1602's computers also grow again after four hours (654 / 316 / 332 at 300 minutes,
the original was not run that long). That makes Open1602's
computers clearly stronger in long games. Two more differences:

- **Warships.** The original's computers had warships after 105–120 minutes in the new game; in
  Open1602's run of the same world none built one in six hours (in the Merlin03 run computer 3
  built one). Worth a look in `ai_military.cpp`.
- **The hourly rebuild** of the computers' state (`0x0040F580`) resets the stage of every town in
  Open1602 and replays it at once (`O1602_AI_LOG=1` shows `stage 0x0 -> 0x14 -> 0x15` for every
  town at 3 682 s, 7 284 s, 10 882 s and 14 483 s). Towns that were on 0x1E/0x1F or 0x16/0x17 come
  back to them, but the 50-minute fallback of stage 0x15 starts again each time. Whether the
  original does the same could not be read from memory in this session.

**Recommendation.** Keep the fixes, but offer the original's behaviour as a setting next to the
other `Rules` (a "classic computer" switch), because a player who wants the original's difficulty
gets a noticeably stronger opponent after two hours. Look at the missing warships.

## 4. Rendering in rotated views (for the thread "Darstellung, Ton und Videos")

The same savegame (`game03.gam`, human city Lemonhall) was shown in the four views in both. The
compass of the original and `E` in Open1602 turn the view the same way, and the island looks the
same in each step. Differences seen:

- Open1602 darkens parts of the map with sharp diagonal edges (meadow north of the town, the sea
  above the island) where the original shows uniform colours. The whole picture is also somewhat
  darker. If this is the new shading (#55) for areas outside the view or not yet discovered, it
  should not apply here: the island is discovered and in view.
- Open1602 shows the goods of the town in the top bar while the camera is over the island; the
  original showed only the gold in the same position (it shows the goods when the town is
  selected).

Screenshots: `shots/cmp_rot1.png` (left the original, right Open1602), `b2_rot_grid.png` and
`n1_rot_grid.png` (all four views).

## 5. Drought

Poking the next drought time in Merlin's savegame "Merlin04_Laufend" (difficulty 3) started a
drought on island 3 (a computer's town) at once: strength 0x40, end after 4 500 tenths of a second
(15 minutes halved for a computer town). After the end the strength went 64 → 32 → 16 → 0 in three
island epochs of 30 s. The next drought was set 30 minutes ahead. This matches
[growth.md](re/growth.md) and Open1602; Open1602's run of the same savegame had its drought on
another island because the random numbers differ. Withered fields could not be counted from memory
in this session; the screenshots show the fields during the drought.

## 6. Volcano

Poking the next eruption time started an eruption on island index 7 (crater 10, 9): it ran from
73 224 to 73 509 (28.5 s), and the next one was set 26 minutes later. No message was shown (the
island is undiscovered, so this is expected, but it was not checked further). Open1602's run of
the same savegame had an eruption of 30 s on another volcanic island. No difference found.

## 7. Fires, plague and robbers in large towns

Merlin's savegame "Endlos_Laufend" (`game10.gam`, 239 min, difficulty 1; the human's Worrach with
405 citizens and 100 merchants, the computer's Zwackau with 525 citizens) was played on for 50
minutes in the original and 60 minutes in Open1602. The original's disaster list (`0x005A5100`, 8
bytes: island, x, y, flags, –, epoch counter, duration 0x14 plague / 0x19 fire) was read every 10 s.

| | Original (50 min) | Open1602 (60 min) |
|---|---|---|
| Zwackau (computer, island 13) | 4 fires, each burning to epoch 24 of 25; 8 plague outbreaks or spreads, mostly 1–3 epochs, one 14 | 2 plagues, ended after 1–2 epochs; no fire |
| Worrach (human, island 14) | 2 plagues, 1 and 3 epochs | 2 plagues, ended after 1–2 epochs |
| Other towns | – | 2 fires (island 17), 2 plagues, all ended within 3 epochs |

In the original the same two houses of Zwackau caught fire again after burning down. In Open1602
nothing burned for long anywhere. The frequency is similar for the human's town; the difference is
in the computer's town and in how long fires last.

**Recommendation.** Check whether Open1602's fire brigade reaches and ends fires it cannot reach in
the original (range of the fire station, a town without one), and whether its computers build fire
stations and doctors earlier or in other places than the original's computers. A longer run of the
same savegame in both, with the positions of fire stations, would settle it.

## 8. Shipyard orders and units in training

- **Fleet counter (S-094).** The counter is player +0x9A. Ordering a ship at the small shipyard of
  Worrach raised the human's counter from 2 to 3 at once; cancelling lowered it to 2. While an
  order runs, the window shows no build button, and the cancel button only exists while an order
  runs, so "cancel without an order" and "a second order over a running one" cannot happen through
  the window. The third case (a captured shipyard with an order) remains as described. Cancelling
  keeps the material already in the yard and drops the progress, as in Open1602.
- **Units in training.** The castle window offers the send-out arrow only for a trained unit; for a
  unit in training it shows "Einheitenausbildung zu 15% abgeschlossen" and no arrow. Open1602 also
  keeps units in training inside, so nothing changes; [military.md](re/military.md) can drop "im
  Original offen".

## 9. Open analysis items

| Item | Result | Notes to update |
|---|---|---|
| Message 0x48 ("protestiert") | Sent by the city tick (`0x0047FD49`) to the local player while the city's flag `+0x257` bit 0 is set, 18 minutes after it was switched on and then every 18 minutes. The flag is the button "Baustop für Siedler verhängen" (STADT.GAD, id 43025, handler `0x0043DEC3`, city command 0x4C at `0x0048073A`). Open1602 already implements it (`CityEvent::MaterialLocked`). | `docs/status.md` 1.1 still lists it as open; [diplomacy.md](re/diplomacy.md) "Offen" too. |
| `TIMERS` +0xA5 | The tutorial flag, see 2. | `docs/status.md`, [ai.md](re/ai.md), [gam.md](formats/gam.md) |
| Production bit +0x0F/4 | "No pickup by the market carts" (`0x0047CD60`, `0x0047CD90`); Open1602 implements it (`Production::kNoPickupFlag`). | [production.md](re/production.md) "Offen" |
| Production bit +0x0F/6 | Only the loader (`0x0048381D`) and the writer touch it; no code reads or sets it in play. It can stay a raw bit for the export. | [production.md](re/production.md) |
| HAUS +0x6A bit 7 | `Destroyflg`: the `haeuser.cod` loader sets bit 7 of +0x6A for that keyword (`0x004614B7`–`0x0046151F`). | [logistics.md](re/logistics.md) "Offen" |
| City store +2/+4/+8 | The lower/upper trade limits and the prices/switches, as [gam.md](formats/gam.md) (`KONTOR2` +0x08 and +0x00) already states. | [logistics.md](re/logistics.md) "Offen" |
| Fleet counter after loading | `0x0048C242`: after loading (not in the tutorial) every human or computer player with no cities (+0x86 = 0) and no ships (+0x9A = 0) is marked absent (kind 10). | for [missions.md](re/missions.md) |

Not looked at in this session: the second list at island +0x30, the random class of loaded fields,
the loader switches `0x0060912C`/`0x005E6AA8`, the special figures of branches 9/10/11/13, the
island header rest, `SZENE` chunks and U-007 (needs two own towns on one island).

## Other observations

- The original saves `SAVEGAME/lastgame.gam` by itself during play (in one copy every 10 minutes
  of game time after loading), and `Game.dat` keeps an absolute path to it (`Lastfile`). "Spiel fortsetzen" loads that
  path, so a copied installation still continues the game of the original folder.
- `o1602-tool state` prints player names in Windows-1252 unchanged ("Wei�er Spieler").
- One copy of the original ended by itself after 70 minutes of "Open1602Test" (the process
  crashed while exiting, SIGSEGV in a Wine thread). Loading its autosave from 68 minutes and playing
  on to 170 minutes did not repeat it, so the cause is unknown; it may be Wine's.
