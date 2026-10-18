# Player flags and colours

All player flags are **one** flag that the engine tints in the player's colour. A new
colour therefore needs no graphics: in the flag menu you choose one of the twelve preset colours
(`render::BannerTable::kDefaultColors`) or any RGB colour. This way the number of possible
players grows without new images ([ADR 0007](../architecture/0007-extensibility-and-modding.md), section 4).

**Status:** Done are the tinting (`render/flag_tint.hpp`), the twelve presets and the template from
the white original flag (`render::BannerTable`), custom images per banner (`FlagStyle::Sprites`, used for
the skull) and units in player colour (`render/unit_tint.hpp`). Still open: the flag menu and
the free RGB colour (interface; until then `--colour 0–3` for a new game in the viewer) as well as custom flags as PNG
(content packages, milestone M8).

## How tinting works

Every pixel of the flag template with **R = G = B** (pure grey) belongs to the cloth and takes on the
player colour. Its grey value is the shading:

| Grey value | Result |
|---|---|
| 128 | exactly the player colour |
| 0 … 127 | darker, linearly down to black: `colour · grey / 128` |
| 129 … 255 | lighter, linearly up to white: `colour + (255 − colour) · (grey − 128) / 127` |

All other pixels – coloured or transparent – stay unchanged. This lets you draw the mast, border,
coat of arms or lettering in fixed colours.

## Drawing your own flag

- **Images:** 8 phases of waving, one set per zoom level (full size like `gfx/ship.bsh` 216–223,
  89 × 30 pixels; half and quarter size for the other levels). The phases run at 90 ms per frame.
- **Anchor point:** The engine centres the image horizontally above the mast. On buildings the
  bottom edge of the image lies at the height `Fahnoffs[2]` of the flag figure; on ships the top edge hangs
  down from the masthead ([figures.md](../re/figures.md), [rendering.md](../re/rendering.md) §6).
  In the original images the cloth starts just right of centre (column 46 of 89, row 6); a
  custom flag should have the same attachment point, otherwise it floats next to the mast.
- **Cloth in greys:** base tone 128, shadows below, highlights above. Draw with a
  grey gradient instead of a fixed colour, and the flag will look right in every player colour.
- **Fixed parts in colour:** A coat of arms that should not be tinted must not contain pure grey.
  For grey fixed parts (e.g. a silver border) a minimal deviation is enough, such as
  (120, 120, 121).
- **Format:** PNG with an alpha channel; pixels with alpha 0 are transparent. Semi-transparency is not
  supported, so that the flag matches the pixel style of the game.

A mod can also supply its own finished, pre-coloured images for individual banners (`FlagStyle::Sprites`)
if a motif should not be tintable – this is how the pirates keep their skull.

The same rules are meant to apply to custom units in player colour (soldiers, cavalry, cannoneers): cloth
in pure grey, everything else deviating by at least one value. For the original units
Open1602 already generates the template itself ([figures.md](../re/figures.md#soldiers-and-player-colours)).

## The original flags

The original has separate, hand-dithered images for red, blue, yellow and white (`ship.bsh`
192–223), plus the pirates' skull (224–231). Open1602 makes the template from the white flag:
every pixel gets the grey value 0.48 · brightness + 5. The factors and the base colours of the four
banners (red 216/51/8, blue 0/0/241, yellow 255/219/57, white 255/255/255) are fitted to the original
images; the mean deviation is about 15 of 255 per colour channel, because the originals are dithered with
256 colours. Anyone who wants the original images switches them on with `--original-flags`
(`render::BannerOptions::original_player_flags`).

## Choosing colours

The twelve presets are easy to tell apart on land, on water and on the minimap: red, blue, yellow,
white (as in the original), green, orange, purple, turquoise, pink, light blue, brown, light green. Black is missing
because the pirates fly black and very dark flags are hard to see on the roofs. Later, the
menu is meant to warn about free RGB colours that are too similar or too dark.
