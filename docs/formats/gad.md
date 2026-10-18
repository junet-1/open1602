# GAD – UI layouts

`GADDATA/*.GAD` (52 files, 45 of which the game loads) and `GADDATA/TOOLS.INC`. **Plain text** in the language
of the `.cod` files ([cod.md](cod.md)): symbols, `@` for relative values, `ObjFill`, `Nummer`, `[n]` for one
value of a symbol, `Include: "file"`. Unlike `haeuser.cod` they are **not encrypted**. Loader
`Gad_LoadFile 0x00405480`. What the game does with the layouts is described in [ui.md](../re/ui.md).

Open1602: `assets::parse_gad`, `assets::load_gad` (`engine/assets/src/gad.cpp`), at runtime `ui::Panel`.

## Structure

```
  Include: "TOOLS.INC"          ; constants: blocks, colours, image numbers (GFXTAFEL = 0, …)
  Objekt: GADGET
    @Nummer:  0                 ; defaults for all gadgets
    Id:       0
    Blocknr:  BLK_TOOL
    Kind:     GAD_UNUSED
    ObjFill:  0,MAXGADGET
    IDHAUPT = 30001
    @Nummer:  +1                ; each further gadget
    Id:       IDHAUPT+0
    Kind:     GAD_GFX
    Gfxnr:    GFXPFEIL
    Pressoff: 2
    Pos:      23, 199
    Size:     54, 71
    Posoffs:  32-Pos[0], 213-Pos[1]
  EndObj
```

- Predefined: `GADGET = 1`, `MAXGADGET = 300` and the kinds `GAD_UNUSED` … `GAD_TEXTFL` (0–10).
- `Include` reads the file at this point into the same symbol table (path relative to the layout).
- The number of gadgets is the **highest `Nummer` + 1**; the 300 default records of `ObjFill` do not count.
- **Value pairs keep their second value:** the loader copies the value slots of the symbol, not what the line
  wrote. `Color: 5` takes over the second value of the last `Color:` line, `@Pos: +30` changes only x.
- Unknown keys are skipped (`Moveselflg` in `CHAT.GAD`); unknown symbols are 0 (in
  `FARBWAHL`, `HOST`, `MISSION` `ZEINR` is missing, in `MILITAR`, `STADT` `IDNIX`; D-013).

## Keys (record of 0x4C bytes)

| Key | Record | Meaning |
|---|---|---|
| `Id` | +0x00 | identifier the code uses (`IDHAUPT+n`; `IDNIX` = decoration) |
| `Kind` | +0x04 | kind (below) |
| `Basenr` | +0x08 | gadget of the group or background |
| `Color` | +0x10, +0x14 | text colour 0–9 (or palette index for FILL/BALKEN) for state 0 / 1 |
| `Reiheflg` | +0x18 bit 0 | member of a group (same `Basenr`), exactly one selected |
| `Flipflg` | bit 2 | toggle: a click inverts the state |
| `Noselflg` | bit 3 | not clickable |
| `Selmoveflg` | bit 5 | pressing or dragging over it selects it (colour choice) |
| `Menuflg` | bit 6 | the code already reacts to the press |
| `Baseflg` / `Baseselflg` | bit 7 / +0x19 bit 0 | draw the gadget `Basenr` first / only in state ≠ 0 |
| `Clipflg` | +0x19 bit 1 | no reader |
| `Nodrawflg` | bit 2 | not drawn, **but clickable** |
| `Rasterflg` | bit 3 | disabled: checkerboard over it |
| `Alphaflg` | bit 4 | semi-transparent (checkerboard) |
| `Sliderflg`, `Slidhorflg`, `Slidverflg` | bits 5–7 | slider (horizontal, vertical) |
| `Slidoffs`, `Slidsize` | +0x20, +0x24 | origin and travel of the slider (0 = `Size`) |
| `Posoffs` | +0x28 | drawing offset (text anchor; for TEXTFL y = line height) |
| `Pressoff` | +0x2C | image step for state 1 |
| `Pos`, `Size` | +0x30, +0x38 | click area (not the image size) |
| `Blocknr` | +0x40 | image block (0 TOOLS, 1 START, 2 SYMBOL, 3 BAUHAUS, 4 BAUSHIP) **or font** for text |
| `Gfxnr` | +0x44 | image in the block (constants from `TOOLS.INC`) |

The text of a gadget is never in the layout; the code sets it.

## Kinds

| Kind | No. | Drawing |
|---|---|---|
| GAD_UNUSED | 0 | nothing, not clickable |
| GAD_NIX | 1 | nothing: invisible area (world map, parts of buttons) |
| GAD_GFX | 2 | image `Gfxnr + Pressoff · state` |
| GAD_GFXRAST | 3 | outline of the image as a checkerboard in `Color` |
| GAD_GFXFILL | 4 | outline in `Color` |
| GAD_TEXTL / TEXTZ / TEXTR | 5 / 6 / 7 | text left / centred / right at the anchor, y = bottom edge of the line |
| GAD_FILL | 8 | rectangle `Pos`/`Size` in palette colour `Color` |
| GAD_BALKEN | 9 | bar that follows a slider (horizontal) or grows upwards (vertical) |
| GAD_TEXTFL | 10 | wrapped text: width `Size.x`, line height `Posoffs.y`, `Size.y / Posoffs.y` lines |

## Coordinates

Always units of a **1024 × 768** screen, relative to the area of the layout (side panel,
status bar, tutor window, whole screen). Pixel = `trunc((v + 0.5) · width / 1024)`. The layouts
apply to all three resolutions (640, 800, 1024); the matching images are prescaled (`TOOLS6`, `TOOLS8`, …,
same numbers).
