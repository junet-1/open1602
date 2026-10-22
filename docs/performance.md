# Performance and system requirements

What a computer needs to run Open1602, and how to measure it. The numbers below come from one
measurement on 2026-10-22 and are an estimate, not a guarantee.

## Estimated minimum: 8 players in a late game, also in multiplayer

| | Minimum | Why |
|---|---|---|
| CPU | 64-bit, 2 cores; single-core speed about an Intel Core 4th generation (Haswell) or AMD Ryzen 1000 at 3 GHz | The simulation runs on one core (ADR 0004: deterministic, one thread). A late game is estimated at about 1 ms per tick on the measuring machine, a quarter of a core at speed 8 |
| Memory | 4 GB | The game holds about 1.4 GB, mostly the sprite atlases of the three zoom levels; the simulation itself 15 to 25 MB |
| Graphics | Any GPU with OpenGL, Direct3D 11 or Metal, textures of 2048 px, 1 GB video memory (integrated graphics such as Intel HD 4600 or AMD Vega are enough) | 40 atlas pages of 2048 x 2048 RGBA, about 650 MB |
| Disk | About 1 GB | The original game (650 MB, 450 MB of it videos); Open1602 a few MB; a savegame 0.2 to 0.6 MB |
| Network | Any broadband connection; latency matters more than bandwidth | Lockstep: only commands travel, and a savegame when a computer has to get the game anew |

In a multiplayer game every computer runs the whole world, so the slowest computer sets the pace.
A multiplayer game of more than four players makes its world larger (707 x 495 for eight, about
37 islands), between the two world sizes measured below.

## Measurements

Measuring machine: a cloud VM with an Intel Xeon at 2.1 GHz and 4 cores, no GPU, Linux, the
`release` preset (GCC 13). A tick is 50 ms of game time: 20 ticks per second at speed 1, 160 at
speed 8.

### Simulation (`o1602-tool bench`)

New own worlds (level 1, seed 7), every player handed to the computer:

| World | Players | Game hours | Mean tick | p99 | Max | Houses | Inhabitants | Peak memory |
|---|---|---|---|---|---|---|---|---|
| 500 x 350 | 4 | 10 | 0.24-0.54 ms | 3.5-7.1 ms | 20.7 ms | 122 | 1 162 | 14 MB |
| 500 x 350 | 8 | 10 | 0.22-0.42 ms | 2.7-4.6 ms | 38 ms (once) | 290 | 2 809 | 15 MB |
| 800 x 600 | 8 | 10 | 0.48-0.65 ms | 4.3-6.7 ms | 35 ms (once) | 166 | 1 379 | 24 MB |

The computer players do not build a real late game: after ten hours the eight of them have about
290 houses together. To stand in for one, their cities were filled densely by a throwaway program
(a grid of roads, houses, markets, workshops and public buildings, the goods topped up every game
minute): about 1 150 houses, 510 production buildings and 5 200 to 6 600 inhabitants. Over an
hour its ticks took 0.30 to 0.39 ms on average, 2.5 to 3.3 ms at p99, mostly 6 to 9 ms at most and
60 ms once. Shares of the tick:

| Step | Share | Mean per tick | Max |
|---|---|---|---|
| computers | 63 % | 243 us | 3.3 ms |
| ships | 23 % | 87 us | 8.6 ms |
| combat | 5 % | 20 us | 0.8 ms |
| production | 5 % | 18 us | 2.3 ms |
| logistics | 2 % | 8 us | 0.1 ms |
| houses | 0.5 % | 2 us | 0.2 ms |

Houses, production and logistics together cost about 30 us per tick for 1 000 houses. Eight human
players with 500 to 1 000 houses each would add 0.15 to 0.25 ms, which puts a late game at about
0.6 to 1 ms per tick on this machine, with peaks around 10 ms from sea routes and the computers'
planning.

The multiplayer fingerprint (`Simulation::state_hash()`, every `SessionConfig::hash_interval`
frames) writes a savegame in memory: 11 to 13 ms in a 500 x 350 world, 28 ms in an 800 x 600 world,
on the main thread.

### Drawing

The game with the dense world, SDL's software renderer (no GPU), vsync off, speed 1:

| Zoom | 1920 x 1080 | 2560 x 1440 |
|---|---|---|
| 0 (near) | 110 fps, 2 700 sprites | 74 fps, 4 200 sprites |
| 1 | 93 fps, 8 200 sprites | 57 fps, 12 800 sprites |
| 2 (far) | 60 fps, 23 600 sprites | 46 fps, 34 900 sprites |

The process held 1.35 GB; with the software renderer the textures are in main memory. Preparing
the sprites took 2.2 s (GFX), 0.6 s (MGFX) and 0.16 s (SGFX).

### Limits of these numbers

- No real late game with eight players existed; the dense world is a quarter to an eighth of one,
  the rest is extrapolated linearly.
- No GPU was measured; the graphics requirement follows from the size of the atlases.
- Only Linux was measured, and no multiplayer game over the internet.

## Measuring on your computer

```sh
o1602-tool bench                                    # new own world, 8 players (707 x 495), 10 game hours
o1602-tool bench --players 8 --world 500x350 --minutes 120 --period 10
o1602-tool bench SAVEGAME/game00.gam --minutes 60   # a savegame of the original or of Open1602
o1602-tool bench --minutes 600 --out late.o1602     # keep the end state, e.g. for the game's --load
```

Without `--world` the world is as large as a multiplayer game of that many players makes it
(`sim::settings_for_players`: the original's 500 x 350 for four, both sides growing with the square
root of the players, so 707 x 495 for eight); the islands follow the area. `bench` hands every
player to the computer, runs the game as fast as it goes and prints per period
(`--period`, game minutes) the mean, median, 99th percentile and maximum tick in microseconds with
the size of the world. At the end it shows each step of a tick (`Simulation::tick_timed()`), the
time of the multiplayer fingerprint and of a savegame, and the peak memory of the process. The
simulation does not draw, so the game itself needs more: run it with `--load late.o1602 --frames 600`
to see its frame rate.
