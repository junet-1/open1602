#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the original Anno 1602 under Wine/Proton in a private headless X server and drive it.

The game runs in its own Xvfb display, so it never touches the desktop (this also works on
Wayland, where tools like xdotool cannot reach native windows). Input is sent with the XTEST
extension and screenshots are read straight from the X server, so no xdotool or ImageMagick is
needed. Intended for comparing the original with Open1602 (screenshots, behaviour).

Usage:
    original_wine.py start [--no-wait]        start Xvfb and the game, wait for the main menu
    original_wine.py screen                   print the detected screen (main_menu, in_game, ...)
    original_wine.py wait SCREEN [--timeout S]
    original_wine.py click X Y [--button N]   left click (N=3: right click) in 1024x768 coordinates
    original_wine.py key KEY [KEY ...]        X keysym names, e.g. Escape Return F2 space
    original_wine.py type TEXT
    original_wine.py shot FILE.png
    original_wine.py new-endless [--difficulty 0-3]   main menu -> running endless game
    original_wine.py stop

Common options (before the command): --display :91, --game-dir DIR, --prefix DIR, --wine BIN.
Defaults come from ANNO1602_DIR and Heroic's game config (Wine/Proton binary and prefix), see
tools/original/README.md. Needs Xvfb, python-xlib and Pillow.

The prefix should have a 1024x768 virtual desktop (winecfg > Graphics); the screen detection and
the menu coordinates assume the game fills a 1024x768 display.
"""

import argparse
import json
import os
import signal
import subprocess
import sys
import time

try:
    from PIL import Image
    from Xlib import X, XK, display as xdisplay
    from Xlib.ext import xtest
except ImportError:  # pragma: no cover
    sys.exit("original_wine.py needs python-xlib and Pillow")

GOG_ID = "1438168222"
WIDTH, HEIGHT = 1024, 768

# Screen signatures: average RGB of a few regions (x0, y0, x1, y1) on a 1024x768 capture.
# Taken from the German GOG "Königsedition"; the regions avoid text so other languages match too.
SIGNATURES = {
    "main_menu": {
        (140, 390, 410, 425): (94, 49, 11),  # "single player" plank
        (140, 610, 410, 645): (75, 39, 9),  # "quit" plank
        (380, 90, 700, 200): (180, 130, 49),  # logo
        (500, 358, 950, 368): (63, 49, 29),  # picture frame
        (520, 380, 930, 700): (94, 64, 40),  # harbour picture
    },
    "single_player_menu": {
        (380, 90, 700, 200): (180, 130, 49),
        (140, 610, 410, 645): (56, 47, 30),
        (500, 358, 950, 368): (40, 35, 21),
        (520, 380, 930, 700): (90, 81, 55),  # scenario list
    },
    "game_setup": {  # difficulty, mission text and high scores
        (380, 90, 700, 200): (180, 130, 49),
        (140, 610, 410, 645): (86, 46, 11),
        (500, 358, 950, 368): (97, 83, 47),
        (450, 600, 900, 740): (80, 70, 43),
    },
    "name_entry": {
        (780, 190, 1015, 360): (102, 90, 66),
        (790, 20, 1000, 170): (113, 102, 74),
        (0, 740, 760, 765): (91, 72, 44),
    },
    "in_game": {
        (780, 190, 1015, 360): (151, 132, 97),  # side panel
        (790, 20, 1000, 170): (192, 162, 101),  # minimap parchment
        (0, 740, 760, 765): (177, 152, 114),  # status bar
    },
}
TOLERANCE = 10  # max channel difference of a region mean

# Click targets (1024x768).
MAIN_SINGLE_PLAYER = (280, 407)
SP_NEW_GAME = (280, 391)
SP_ENDLESS_ENTRY = (570, 429)
DIFFICULTY = [(175, 425), (175, 457), (175, 489), (175, 521)]
START_GAME = (268, 626)
FLAG_RED = (215, 420)


def run_dir(disp):
    base = os.environ.get("XDG_RUNTIME_DIR") or "/tmp"
    return os.path.join(base, f"original_wine{disp.replace(':', '_')}")


def state_path(disp):
    return run_dir(disp) + ".json"


def heroic_config():
    path = os.path.expanduser(f"~/.config/heroic/GamesConfig/{GOG_ID}.json")
    try:
        with open(path) as f:
            return json.load(f).get(GOG_ID, {})
    except (OSError, ValueError):
        return {}


def resolve_setup(args):
    cfg = heroic_config()
    game_dir = args.game_dir or os.environ.get("ANNO1602_DIR")
    if not game_dir:
        sys.exit("set ANNO1602_DIR or pass --game-dir")
    wine = args.wine or cfg.get("wineVersion", {}).get("bin") or "wine"
    prefix = args.prefix or cfg.get("winePrefix")
    if not prefix:
        sys.exit("no Wine prefix: pass --prefix (Heroic config not found)")
    exe = os.path.join(game_dir, "1602.exe")
    if not os.path.isfile(exe):
        sys.exit(f"{exe} not found")
    return game_dir, wine, prefix, exe


def is_proton(wine):
    return os.path.basename(wine) == "proton"


def wineserver_for(wine):
    if is_proton(wine):
        return os.path.join(os.path.dirname(wine), "files", "bin", "wineserver")
    candidate = os.path.join(os.path.dirname(wine), "wineserver")
    return candidate if os.path.isfile(candidate) else "wineserver"


def wine_env(wine, prefix, disp, scratch):
    env = dict(os.environ)
    env.pop("WAYLAND_DISPLAY", None)  # force winex11 into our Xvfb
    env["DISPLAY"] = disp
    if is_proton(wine):
        steam_dir = os.path.join(scratch, "steam")
        os.makedirs(steam_dir, exist_ok=True)
        env.update(
            STEAM_COMPAT_DATA_PATH=prefix,
            STEAM_COMPAT_CLIENT_INSTALL_PATH=steam_dir,
            SteamAppId="0",
            SteamGameId="0",
        )
        env["WINEPREFIX"] = os.path.join(prefix, "pfx")
    else:
        env["WINEPREFIX"] = prefix
    return env


def open_display(disp):
    try:
        return xdisplay.Display(disp)
    except Exception as e:  # Xlib raises several types here
        sys.exit(f"cannot open display {disp}: {e} (is the game started?)")


# ---------------------------------------------------------------- input and capture


def capture(d):
    root = d.screen().root
    geo = root.get_geometry()
    raw = root.get_image(0, 0, geo.width, geo.height, X.ZPixmap, 0xFFFFFFFF)
    return Image.frombytes("RGB", (geo.width, geo.height), raw.data, "raw", "BGRX")


def move(d, x, y):
    xtest.fake_input(d, X.MotionNotify, x=x, y=y)
    d.sync()


def click(d, x, y, button=1):
    move(d, x, y)
    time.sleep(0.05)
    xtest.fake_input(d, X.ButtonPress, button)
    d.sync()
    time.sleep(0.08)
    xtest.fake_input(d, X.ButtonRelease, button)
    d.sync()
    time.sleep(0.05)


def keycode(d, name):
    sym = XK.string_to_keysym(name)
    if not sym and len(name) == 1:
        sym = ord(name)
    code = d.keysym_to_keycode(sym)
    if not code:
        sys.exit(f"unknown key {name!r}")
    return code


def press(d, name, shift=False):
    shift_code = keycode(d, "Shift_L")
    if shift:
        xtest.fake_input(d, X.KeyPress, shift_code)
    code = keycode(d, name)
    xtest.fake_input(d, X.KeyPress, code)
    d.sync()
    time.sleep(0.06)
    xtest.fake_input(d, X.KeyRelease, code)
    if shift:
        xtest.fake_input(d, X.KeyRelease, shift_code)
    d.sync()
    time.sleep(0.04)


def type_text(d, text):
    for ch in text:
        name = {" ": "space", "\n": "Return"}.get(ch, ch)
        press(d, name.lower() if len(name) == 1 else name, shift=ch.isupper())


# ---------------------------------------------------------------- screen detection


def region_mean(img, box):
    return img.crop(box).resize((1, 1), Image.BOX).getpixel((0, 0))


def detect(img):
    """Return the best-matching screen name, 'black' or 'unknown'."""
    if img.size != (WIDTH, HEIGHT):
        return "unknown"
    best, best_err = "unknown", TOLERANCE + 1
    for name, regions in SIGNATURES.items():
        err = max(
            max(abs(a - b) for a, b in zip(region_mean(img, box), rgb)) for box, rgb in regions.items()
        )
        if err < best_err:
            best, best_err = name, err
    if best == "unknown" and img.convert("L").getextrema()[1] < 8:
        return "black"
    return best


def wait_for(d, screen, timeout, poke=None):
    """Poll until SCREEN shows; call POKE() between polls (e.g. to skip videos)."""
    end = time.time() + timeout
    while time.time() < end:
        if detect(capture(d)) == screen:
            return True
        if poke:
            poke()
        time.sleep(1.0)
    return False


# ---------------------------------------------------------------- commands


def cmd_start(args):
    disp = args.display
    state = state_path(disp)
    if os.path.exists(state):
        sys.exit(f"already running on {disp} (see {state}); run 'stop' first")
    if os.path.exists(f"/tmp/.X11-unix/X{disp.lstrip(':')}"):
        sys.exit(f"display {disp} is already in use; pick another with --display")
    game_dir, wine, prefix, exe = resolve_setup(args)
    if subprocess.run(["pgrep", "-x", "1602.exe"], capture_output=True).returncode == 0:
        sys.exit("1602.exe is already running (Heroic?); a second instance would share its Wine desktop")

    scratch = run_dir(disp)
    os.makedirs(scratch, exist_ok=True)
    xvfb = subprocess.Popen(
        ["Xvfb", disp, "-screen", "0", f"{WIDTH}x{HEIGHT}x24", "-nolisten", "tcp"],
        stdout=subprocess.DEVNULL,
        stderr=open(os.path.join(scratch, "xvfb.log"), "w"),
        start_new_session=True,
    )
    for _ in range(50):
        if os.path.exists(f"/tmp/.X11-unix/X{disp.lstrip(':')}"):
            break
        time.sleep(0.1)
    else:
        xvfb.kill()
        sys.exit("Xvfb did not start")

    env = wine_env(wine, prefix, disp, scratch)
    cmd = [wine, "run", exe] if is_proton(wine) else [wine, exe]
    log_path = os.path.join(scratch, "wine.log")
    game = subprocess.Popen(
        cmd, cwd=game_dir, env=env, stdout=open(log_path, "w"), stderr=subprocess.STDOUT, start_new_session=True
    )
    with open(state, "w") as f:
        json.dump(
            {"xvfb": xvfb.pid, "game": game.pid, "wine": wine, "prefix": prefix, "log": log_path}, f
        )
    print(f"started on {disp} (log: {log_path})")
    if args.no_wait:
        return

    d = open_display(disp)
    # The intro videos skip with Escape; keep poking until the main menu shows.
    if not wait_for(d, "main_menu", args.timeout, poke=lambda: press(d, "Escape")):
        sys.exit(f"main menu did not show within {args.timeout}s (screen: {detect(capture(d))})")
    print("main menu")


def cmd_stop(args):
    state = state_path(args.display)
    try:
        with open(state) as f:
            st = json.load(f)
    except OSError:
        sys.exit(f"nothing running on {args.display}")
    env = wine_env(st["wine"], st["prefix"], args.display, run_dir(args.display))
    subprocess.run([wineserver_for(st["wine"]), "-k"], env=env, capture_output=True)
    for key in ("game", "xvfb"):
        try:
            os.killpg(st[key], signal.SIGTERM)
        except ProcessLookupError:
            pass
    time.sleep(1.0)
    for key in ("game", "xvfb"):
        try:
            os.killpg(st[key], signal.SIGKILL)
        except ProcessLookupError:
            pass
    os.remove(state)
    for _ in range(100):  # wait until Wine has really gone, so a new start does not race it
        if subprocess.run(["pgrep", "-x", "1602.exe"], capture_output=True).returncode != 0:
            break
        time.sleep(0.1)
    print("stopped")


def cmd_new_endless(args):
    d = open_display(args.display)
    steps = [
        (MAIN_SINGLE_PLAYER, "single_player_menu"),
        (SP_ENDLESS_ENTRY, None),
        (SP_NEW_GAME, "game_setup"),
    ]
    if detect(capture(d)) != "main_menu":
        sys.exit("not in the main menu")
    for (x, y), expect in steps:
        click(d, x, y)
        time.sleep(1.5)
        if expect and not wait_for(d, expect, 10):
            sys.exit(f"expected {expect}, got {detect(capture(d))}")
    click(d, *DIFFICULTY[args.difficulty])
    time.sleep(0.5)
    click(d, *START_GAME)
    if not wait_for(d, "name_entry", 30):
        sys.exit(f"name entry did not show (screen: {detect(capture(d))})")
    press(d, "Return")  # keep the suggested name
    time.sleep(1.5)
    click(d, *FLAG_RED)
    if not wait_for(d, "in_game", 60):
        sys.exit(f"game did not start (screen: {detect(capture(d))})")
    print("in game")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--display", default=":91")
    p.add_argument("--game-dir")
    p.add_argument("--prefix", help="Wine prefix (for Proton: the compat data dir that holds pfx/)")
    p.add_argument("--wine", help="wine binary or Proton 'proton' script")
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("start")
    s.add_argument("--no-wait", action="store_true")
    s.add_argument("--timeout", type=float, default=120)
    sub.add_parser("stop")
    sub.add_parser("screen")
    s = sub.add_parser("wait")
    s.add_argument("screen", choices=sorted(SIGNATURES))
    s.add_argument("--timeout", type=float, default=60)
    s = sub.add_parser("click")
    s.add_argument("x", type=int)
    s.add_argument("y", type=int)
    s.add_argument("--button", type=int, default=1)
    s = sub.add_parser("key")
    s.add_argument("keys", nargs="+")
    s = sub.add_parser("type")
    s.add_argument("text")
    s = sub.add_parser("shot")
    s.add_argument("file")
    s = sub.add_parser("new-endless")
    s.add_argument("--difficulty", type=int, choices=range(4), default=1)
    args = p.parse_args()

    if args.cmd == "start":
        return cmd_start(args)
    if args.cmd == "stop":
        return cmd_stop(args)
    if args.cmd == "new-endless":
        return cmd_new_endless(args)

    d = open_display(args.display)
    if args.cmd == "screen":
        print(detect(capture(d)))
    elif args.cmd == "wait":
        if not wait_for(d, args.screen, args.timeout):
            sys.exit(f"timeout (screen: {detect(capture(d))})")
    elif args.cmd == "click":
        click(d, args.x, args.y, args.button)
    elif args.cmd == "key":
        for k in args.keys:
            press(d, k)
    elif args.cmd == "type":
        type_text(d, args.text)
    elif args.cmd == "shot":
        capture(d).save(args.file)
        print(args.file)


if __name__ == "__main__":
    main()
