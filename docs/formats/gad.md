# GAD – Layouts der Oberfläche

`GADDATA/*.GAD` (52 Dateien, 45 davon lädt das Spiel) und `GADDATA/TOOLS.INC`. **Klartext** in der Sprache
der `.cod`-Dateien ([cod.md](cod.md)): Symbole, `@` für relative Werte, `ObjFill`, `Nummer`, `[n]` für einen
Wert eines Symbols, `Include: "Datei"`. Anders als `haeuser.cod` sind sie **nicht verschlüsselt**. Lader
`Gad_LoadFile 0x00405480`. Was das Spiel mit den Layouts macht, steht in [ui.md](../re/ui.md).

Open1602: `assets::parse_gad`, `assets::load_gad` (`engine/assets/src/gad.cpp`), im Betrieb `ui::Panel`.

## Aufbau

```
  Include: "TOOLS.INC"          ; Konstanten: Blöcke, Farben, Bildnummern (GFXTAFEL = 0, …)
  Objekt: GADGET
    @Nummer:  0                 ; Vorgaben für alle Gadgets
    Id:       0
    Blocknr:  BLK_TOOL
    Kind:     GAD_UNUSED
    ObjFill:  0,MAXGADGET
    IDHAUPT = 30001
    @Nummer:  +1                ; jedes weitere Gadget
    Id:       IDHAUPT+0
    Kind:     GAD_GFX
    Gfxnr:    GFXPFEIL
    Pressoff: 2
    Pos:      23, 199
    Size:     54, 71
    Posoffs:  32-Pos[0], 213-Pos[1]
  EndObj
```

- Vordefiniert: `GADGET = 1`, `MAXGADGET = 300` und die Arten `GAD_UNUSED` … `GAD_TEXTFL` (0–10).
- `Include` liest die Datei an dieser Stelle in dieselbe Symboltabelle (Pfad relativ zum Layout).
- Die Zahl der Gadgets ist die **höchste `Nummer` + 1**; die 300 Vorgabesätze von `ObjFill` zählen nicht.
- **Wertpaare behalten ihren zweiten Wert:** Der Lader kopiert die Wertplätze des Symbols, nicht was die Zeile
  schrieb. `Color: 5` übernimmt den zweiten Wert der letzten `Color:`-Zeile, `@Pos: +30` ändert nur x.
- Unbekannte Schlüssel werden übergangen (`Moveselflg` in `CHAT.GAD`); unbekannte Symbole sind 0 (in
  `FARBWAHL`, `HOST`, `MISSION` fehlt `ZEINR`, in `MILITAR`, `STADT` `IDNIX`; D-013).

## Schlüssel (Satz von 0x4C Byte)

| Schlüssel | Satz | Bedeutung |
|---|---|---|
| `Id` | +0x00 | Kennung, die der Code benutzt (`IDHAUPT+n`; `IDNIX` = Zierde) |
| `Kind` | +0x04 | Art (unten) |
| `Basenr` | +0x08 | Gadget der Gruppe bzw. Hintergrund |
| `Color` | +0x10, +0x14 | Textfarbe 0–9 (oder Palettenindex bei FILL/BALKEN) für Zustand 0 / 1 |
| `Reiheflg` | +0x18 Bit 0 | Glied einer Gruppe (gleiche `Basenr`), genau eins gewählt |
| `Flipflg` | Bit 2 | Umschalter: ein Klick kehrt den Zustand um |
| `Noselflg` | Bit 3 | nicht anklickbar |
| `Selmoveflg` | Bit 5 | Drücken oder Hinüberziehen wählt es (Farbwahl) |
| `Menuflg` | Bit 6 | der Code hört schon das Drücken |
| `Baseflg` / `Baseselflg` | Bit 7 / +0x19 Bit 0 | erst das Gadget `Basenr` zeichnen / nur im Zustand ≠ 0 |
| `Clipflg` | +0x19 Bit 1 | ohne Leser |
| `Nodrawflg` | Bit 2 | nicht gezeichnet, **aber anklickbar** |
| `Rasterflg` | Bit 3 | gesperrt: Schachbrett darüber |
| `Alphaflg` | Bit 4 | halb durchsichtig (Schachbrett) |
| `Sliderflg`, `Slidhorflg`, `Slidverflg` | Bit 5–7 | Schieber (waagrecht, senkrecht) |
| `Slidoffs`, `Slidsize` | +0x20, +0x24 | Ursprung und Weg des Schiebers (0 = `Size`) |
| `Posoffs` | +0x28 | Zeichenversatz (Textanker; bei TEXTFL y = Zeilenhöhe) |
| `Pressoff` | +0x2C | Bildschritt für Zustand 1 |
| `Pos`, `Size` | +0x30, +0x38 | Klickfläche (nicht die Bildgröße) |
| `Blocknr` | +0x40 | Bildblock (0 TOOLS, 1 START, 2 SYMBOL, 3 BAUHAUS, 4 BAUSHIP) **oder Schrift** bei Text |
| `Gfxnr` | +0x44 | Bild im Block (Konstanten aus `TOOLS.INC`) |

Der Text eines Gadgets steht nie im Layout; der Code setzt ihn.

## Arten

| Art | Nr. | Zeichnen |
|---|---|---|
| GAD_UNUSED | 0 | nichts, nicht anklickbar |
| GAD_NIX | 1 | nichts: unsichtbare Fläche (Weltkarte, Teile von Knöpfen) |
| GAD_GFX | 2 | Bild `Gfxnr + Pressoff · Zustand` |
| GAD_GFXRAST | 3 | Umriss des Bildes als Schachbrett in `Color` |
| GAD_GFXFILL | 4 | Umriss in `Color` |
| GAD_TEXTL / TEXTZ / TEXTR | 5 / 6 / 7 | Text links / mittig / rechts am Anker, y = Unterkante der Zeile |
| GAD_FILL | 8 | Rechteck `Pos`/`Size` in Palettenfarbe `Color` |
| GAD_BALKEN | 9 | Balken, der einem Schieber folgt (waagrecht) oder nach oben wächst (senkrecht) |
| GAD_TEXTFL | 10 | umbrochener Text: Breite `Size.x`, Zeilenhöhe `Posoffs.y`, `Size.y / Posoffs.y` Zeilen |

## Koordinaten

Immer Einheiten eines **1024 × 768**-Bildschirms, relativ zum Bereich des Layouts (Seitenleiste,
Statusleiste, Tutorfenster, ganzer Bildschirm). Bildpunkt = `trunc((v + 0,5) · Breite / 1024)`. Die Layouts
gelten für alle drei Auflösungen (640, 800, 1024); die Bilder dazu sind vorskaliert (`TOOLS6`, `TOOLS8`, …,
gleiche Nummern).
