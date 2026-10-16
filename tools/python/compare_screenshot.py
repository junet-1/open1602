#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Align a screenshot of the original game with an Open1602 render and report the differences.

Usage:
    compare_screenshot.py SCREENSHOT RENDER [--game-view X,Y,W,H] [--out DIR]

SCREENSHOT is a capture of the original (any size; black side bars are cut off and the picture is
scaled back to 1024x768). RENDER is a larger crop of the same area from
`o1602-tool render-world ... --zoom N --tile X,Y --size WxH` at the zoom level the original showed
(the script tries all scales and reports the best one). The game view defaults to the map area of
the 1024x768 layout (without the money label and the side panel).

Writes aligned crops (orig.png, open1602.png), a difference mask (diff.png) and prints the
correlation and the largest differing regions. Needs numpy and opencv-python.
"""

import argparse
import os
import sys

try:
    import cv2
    import numpy as np
except ImportError:  # pragma: no cover
    sys.exit("compare_screenshot.py needs numpy and opencv-python")


def native_screenshot(path):
    img = cv2.imread(path)
    if img is None:
        sys.exit(f"cannot read {path}")
    mask = img.sum(axis=2) > 0
    cols = np.where(mask.any(axis=0))[0]
    rows = np.where(mask.any(axis=1))[0]
    img = img[rows[0] : rows[-1] + 1, cols[0] : cols[-1] + 1]
    return cv2.resize(img, (1024, 768), interpolation=cv2.INTER_AREA)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("screenshot")
    ap.add_argument("render")
    ap.add_argument("--game-view", default="0,40,766,680", help="x,y,w,h of the map area at 1024x768")
    ap.add_argument("--out", default=".")
    args = ap.parse_args()

    shot = native_screenshot(args.screenshot)
    render = cv2.imread(args.render)
    if render is None:
        sys.exit(f"cannot read {args.render}")
    x, y, w, h = (int(v) for v in args.game_view.split(","))
    view = shot[y : y + h, x : x + w]

    best = None
    for scale in (0.5, 1.0, 2.0):
        v = cv2.resize(view, None, fx=scale, fy=scale, interpolation=cv2.INTER_AREA)
        if v.shape[0] > render.shape[0] or v.shape[1] > render.shape[1]:
            continue
        result = cv2.matchTemplate(render, v, cv2.TM_CCOEFF_NORMED)
        _, score, _, loc = cv2.minMaxLoc(result)
        if best is None or score > best[0]:
            best = (score, scale, loc, v)
    if best is None:
        sys.exit("render is smaller than the screenshot view")
    score, scale, (lx, ly), v = best
    print(f"best match: scale {scale}, correlation {score:.3f}, at {lx},{ly} in the render")

    mine = render[ly : ly + v.shape[0], lx : lx + v.shape[1]]
    diff = np.abs(mine.astype(int) - v.astype(int)).sum(axis=2)
    print(f"mean colour distance {diff.mean():.1f}")
    mask = (diff > 120).astype(np.uint8) * 255
    os.makedirs(args.out, exist_ok=True)
    cv2.imwrite(os.path.join(args.out, "orig.png"), v)
    cv2.imwrite(os.path.join(args.out, "open1602.png"), mine)
    cv2.imwrite(os.path.join(args.out, "diff.png"), mask)
    n, _, stats, _ = cv2.connectedComponentsWithStats(mask)
    regions = sorted((s for s in stats[1:n] if s[4] > 15), key=lambda s: -s[4])
    for s in regions[:20]:
        print(f"  differs: x {s[0]}, y {s[1]}, {s[2]}x{s[3]} ({s[4]} px)")


if __name__ == "__main__":
    main()
