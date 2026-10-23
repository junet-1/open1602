# Application icon

A compass rose in a sandstone medallion, drawn for Open1602. It borrows the look of the original
game's menus (sandstone, brass, painted parchment) but none of its pictures or logos.

| File | Use |
|---|---|
| `open1602.svg` | Source for 48 px and up |
| `open1602-small.svg` | Source for 16 to 32 px, with fewer details so it stays legible |
| `png/open1602-<N>.png` | 16 to 512 px; installed into the hicolor icon theme on Linux |
| `open1602.ico` | Windows; linked into `open1602.exe` (`engine/app/open1602.rc.in`) |
| `open1602.icns` | macOS, 16 to 1024 px; for an app bundle |
| `window/open1602-<N>.bmp` | 64 and 128 px; compiled into the game, which sets them as the window icon (title bar, task bar, macOS dock) |

The desktop entry for Linux is `data/linux/org.open1602.Open1602.desktop`; its name matches the
application id the game gives SDL.

Everything except the two SVGs is generated. After changing an SVG, run `tools/make_icons.py`
(needs `rsvg-convert`, `resvg` or Inkscape) and commit the results.

## License

The icon files in this folder are dedicated to the public domain under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/), so distributions and packagers can
use them without conditions. The rest of the project stays under GPL-3.0-or-later.
