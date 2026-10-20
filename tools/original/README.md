# Driving the original game

`original_wine.py` runs the original *Anno 1602* under Wine or Proton in a private, headless X server
(Xvfb) and drives it from the command line. It is meant for comparing the original with Open1602:
take screenshots of the same screen in both, or replay the same clicks and watch what happens.

The game never appears on the desktop, so this also works on Wayland, where `xdotool` cannot reach
native windows. Clicks and keys go through the X server's XTEST extension and screenshots are read
from the X server directly.

## Requirements

- Linux with `Xvfb` (Arch: `xorg-server-xvfb`, Debian/Ubuntu: `xvfb`, Fedora: `xorg-x11-server-Xvfb`)
- Python with `python-xlib` and `Pillow`
- An installed original that already runs under Wine or Proton (for example through Heroic), with a
  **1024x768 virtual desktop** set in `winecfg` (Graphics tab). Screen detection and menu
  coordinates assume the game fills a 1024x768 display.

## Setup

The script reads the game folder from `ANNO1602_DIR` (or `--game-dir`). The Wine binary and prefix
come from Heroic's config for the GOG version (`~/.config/heroic/GamesConfig/1438168222.json`), or
from `--wine` and `--prefix`. `--wine` takes a plain `wine` binary or a Proton `proton` script; for
Proton, `--prefix` is the compat data folder that contains `pfx/`.

The script does not change the prefix or the game files, but the game itself writes its usual
files (settings, savegames, high scores) as when it is started through Heroic. Use a copy of the
prefix and game folder for runs that must not touch the real installation.

## Usage

```bash
tools/original/original_wine.py start                 # Xvfb + game, skips the intro, waits for the main menu
tools/original/original_wine.py shot menu.png
tools/original/original_wine.py new-endless           # main menu -> running endless game (average difficulty)
tools/original/original_wine.py click 820 310         # left click at (x, y); --button 3 for a right click
tools/original/original_wine.py key F2 space          # X keysym names
tools/original/original_wine.py type "Merlin"
tools/original/original_wine.py screen                # main_menu, single_player_menu, game_setup, name_entry, in_game, black, unknown
tools/original/original_wine.py wait in_game --timeout 60
tools/original/original_wine.py stop                  # ends Wine for this prefix and the X server
```

Several instances can run side by side on different displays (`--display :92`), as long as each
uses its own prefix; a second start in the same prefix is refused, because it would share the first
instance's Wine desktop. `start` also refuses to run while `1602.exe` is already running (for
example started from Heroic). To watch a run, point a VNC server at the display
(`x11vnc -display :91`) or take screenshots.

Logs and state live in `$XDG_RUNTIME_DIR/original_wine_91/` (Wine log) and
`$XDG_RUNTIME_DIR/original_wine_91.json` (process ids).

## Screen detection

`screen` compares the average colour of a few regions with recorded values. The signatures come
from the German GOG "Königsedition" and avoid text, so other languages should match too; when a
screen reads `unknown`, take a screenshot and add or adjust its regions in `SIGNATURES`.
