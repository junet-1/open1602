#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render the application icon in every size and format the platforms need.

Usage:
    make_icons.py [--renderer rsvg-convert|resvg|inkscape]

Sources are data/icons/open1602.svg (48 px and up) and data/icons/open1602-small.svg (16 to
32 px, fewer details). The script writes, next to them:

    png/open1602-<N>.png    16 to 512 px, installed into the hicolor theme on Linux
    open1602.ico            Windows: 16, 24, 32, 48, 64 and 256 px, linked into open1602.exe
    open1602.icns           macOS: 16 to 1024 px (@1x and @2x)
    window/open1602-<N>.bmp 64 and 128 px, compiled into the game for SDL_SetWindowIcon

The SVGs use filters (lighting, noise), so the renderer must support them: rsvg-convert
(librsvg), resvg and Inkscape do. Only the Python standard library is needed besides.
Run it after changing an SVG and commit the results.
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ICON_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "icons")
SMALL_MAX = 32  # sizes up to this use the simplified SVG
PNG_SIZES = [16, 24, 32, 48, 64, 128, 256, 512]
ICO_SIZES = [16, 24, 32, 48, 64, 256]
# ICNS element types with PNG data: (type, pixel size)
ICNS_TYPES = [
    (b"icp4", 16), (b"icp5", 32), (b"icp6", 64), (b"ic07", 128), (b"ic08", 256), (b"ic09", 512),
    (b"ic11", 32), (b"ic12", 64), (b"ic13", 256), (b"ic14", 512), (b"ic10", 1024),
]
WINDOW_SIZES = [64, 128]


def renderer_command(renderer, svg, size, out):
    if renderer == "rsvg-convert":
        return [renderer, "-w", str(size), "-h", str(size), svg, "-o", out]
    if renderer == "resvg":
        return [renderer, "-w", str(size), "-h", str(size), svg, out]
    if renderer == "inkscape":
        return [renderer, "-w", str(size), "-h", str(size), svg, "-o", out]
    sys.exit(f"unknown renderer {renderer}")


def find_renderer(wanted):
    for name in [wanted] if wanted else ["rsvg-convert", "resvg", "inkscape"]:
        if shutil.which(name):
            return name
    sys.exit("no SVG renderer found: install librsvg (rsvg-convert), resvg or Inkscape")


def render(renderer, size, out):
    svg = os.path.join(ICON_DIR, "open1602-small.svg" if size <= SMALL_MAX else "open1602.svg")
    subprocess.run(renderer_command(renderer, svg, size, out), check=True)
    with open(out, "rb") as f:
        return f.read()


def decode_png_rgba(data):
    """Pixels of an 8-bit RGB or RGBA, non-interlaced PNG (what the renderers write) as RGBA rows."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit("renderer did not write a PNG")
    pos, idat, header = 8, b"", None
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos : pos + 8])
        body = data[pos + 8 : pos + 8 + length]
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    width, height, depth, color, _, _, interlace = header
    if depth != 8 or color not in (2, 6) or interlace:
        sys.exit("unsupported PNG layout from the renderer")
    channels = 4 if color == 6 else 3
    raw = zlib.decompress(idat)
    stride = width * channels
    rows, previous, pos = [], bytearray(stride), 0
    for _ in range(height):
        kind, line = raw[pos], bytearray(raw[pos + 1 : pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = previous[i]
            c = previous[i - channels] if i >= channels else 0
            if kind == 1:
                line[i] = (line[i] + a) & 0xFF
            elif kind == 2:
                line[i] = (line[i] + b) & 0xFF
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        previous = line
        if channels == 3:
            line = bytearray(b"".join(bytes(line[i : i + 3]) + b"\xff" for i in range(0, stride, 3)))
        rows.append(bytes(line))
    return width, height, rows


def bmp_rgba(width, height, rows):
    """A 32-bit BMP with alpha (BITMAPV5HEADER, BI_BITFIELDS), as SDL_LoadBMP reads it."""
    pixels = b"".join(
        b"".join(bytes((row[i + 2], row[i + 1], row[i], row[i + 3])) for i in range(0, len(row), 4))
        for row in reversed(rows)  # BMP rows go bottom-up
    )
    header = struct.pack(
        "<IiiHHIIiiIIIIII", 124, width, height, 1, 32, 3, len(pixels), 2835, 2835, 0, 0,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    header += b"BGRs" + bytes(36) + struct.pack("<7I", 0, 0, 0, 4, 0, 0, 0)  # sRGB, intent: images
    offset = 14 + len(header)
    return struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset) + header + pixels


def ico(pngs):
    """A Windows .ico with PNG-compressed images (Windows Vista and later)."""
    out = struct.pack("<HHH", 0, 1, len(pngs))
    offset = 6 + 16 * len(pngs)
    for size, data in pngs:
        dim = 0 if size >= 256 else size  # 0 means 256
        out += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    return out + b"".join(data for _, data in pngs)


def icns(elements):
    body = b"".join(kind + struct.pack(">I", 8 + len(data)) + data for kind, data in elements)
    return b"icns" + struct.pack(">I", 8 + len(body)) + body


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    print(os.path.relpath(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--renderer", help="rsvg-convert, resvg or inkscape (default: the first found)")
    args = parser.parse_args()
    renderer = find_renderer(args.renderer)

    sizes = sorted(set(PNG_SIZES + ICO_SIZES + [s for _, s in ICNS_TYPES] + WINDOW_SIZES))
    with tempfile.TemporaryDirectory() as tmp:
        png = {s: render(renderer, s, os.path.join(tmp, f"{s}.png")) for s in sizes}

    for s in PNG_SIZES:
        write(os.path.join(ICON_DIR, "png", f"open1602-{s}.png"), png[s])
    write(os.path.join(ICON_DIR, "open1602.ico"), ico([(s, png[s]) for s in ICO_SIZES]))
    write(os.path.join(ICON_DIR, "open1602.icns"), icns([(k, png[s]) for k, s in ICNS_TYPES]))
    for s in WINDOW_SIZES:
        write(os.path.join(ICON_DIR, "window", f"open1602-{s}.bmp"), bmp_rgba(*decode_png_rgba(png[s])))


if __name__ == "__main__":
    main()
