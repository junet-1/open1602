# BSH – Sprite-Sammlungen

Ein Chunk `BSH` (bzw. `ZEI` für Schriften). Nutzdaten:

```c
uint32_t offsets[n];      // Offset jedes Sprites relativ zum Beginn der Nutzdaten
                          // n ist nicht gespeichert: Die Tabelle endet dort, wo der
                          // erste (niedrigste) Sprite-Datensatz beginnt -> n = min(offsets)/4
// danach die Datensätze:
struct SpriteRecord {
    uint32_t width;
    uint32_t height;
    uint32_t type;        // in allen untersuchten Dateien 1; Bedeutung unbekannt
    uint32_t length;      // Größe des Datensatzes inkl. dieser 16 Byte, auf 4 Byte aufgerundet
    uint8_t  rle[];       // siehe unten
};
```

Mehrere Tabelleneinträge können auf denselben Datensatz zeigen (z. B. die ersten vier Einträge von
`GFX/STADTFLD.BSH`), und die Offsets sind nicht monoton steigend.

## RLE-Kodierung

```
loop:
    b = next byte
    0xFF        -> Bildende
    0xFE        -> neue Zeile (x = 0, y += 1)
    sonst       -> b transparente Pixel überspringen,
                   n = next byte, dann n Palettenindizes (deckende Pixel)
```

Eine Zeile kann mehrere `[skip][n][pixel…]`-Folgen enthalten. Pixel außerhalb von
`width × height` kommen bei BSH nie vor; bei einigen ZEI-Glyphen schon (siehe unten).

## Zoomstufen und Inhalte

| Ordner | Zoom | Bodenkachel |
|---|---|---|
| `GFX/` | groß | 64 × 31 |
| `MGFX/` | mittel | (halbiert) |
| `SGFX/` | klein | (geviertelt) |

`STADTFLD.BSH` enthält in allen drei Stufen 5964 Sprites (Gelände, Küsten, Flüsse, Gebäude in allen
Ausbaustufen/Rotationen). Die Indizes entsprechen den `Gfx`-Werten aus `haeuser.cod`
(`GFXBODEN = 0`, `GFXHANG = GFXBODEN+432`, …). Weitere Dateien: `SHIP` (368), `TRAEGER` (1784),
`SOLDAT`, `TIERE`, `FISCHE`, `EFFEKTE`, `GAUKLER`, `MAEHER`, `SCHATTEN`, `NUMBERS`;
in `TOOLGFX/`: `START` (Hauptmenü, 90), `TOOLS` (Interface), `BAUHAUS`, `BAUSHIP`, `EDITOR`, `SYMBOL`
mit Varianten `*6`/`*8` (vermutlich für 640×480 bzw. 800×600).

Verifiziert durch: C++-Dekoder dekodiert alle Sprites von `GFX/MGFX/SGFX STADTFLD`, `SHIP`, `TRAEGER`,
`START` ohne Fehler (`tests/data/test_original_data.cpp`); Sichtprüfung der exportierten PNGs.

## ZEI (Schriften)

Gleiches Format, Chunk-Name `ZEI`. **Zeichen = Code − 0x20** (Windows-1252, 224 Zeichen; die großen Schriften
ZEI28V/ZEI24V nur 33: Codes 32–64). Alle Zeichen einer Schrift sind gleich hoch (die Zeilenhöhe); der
Vorschub ist die Breite, ohne Abstand. Einzelne Zeichen (z. B. Index 14 in `ZEI20V.ZEI`) setzen ein, zwei
Bildpunkte jenseits von `width`: Sie überlappen das nächste Zeichen (das Original schneidet nichts ab;
`SpriteSheet::decode(i, extra_width)` behält sie). Die Bildpunkte haben nur die Palettenindizes 1 (Körper)
und 7 (Schatten), die beim Zeichnen umgefärbt werden. Schriften je Auflösung, Farben und Umbruch:
[ui.md](../re/ui.md#schriften). Open1602: `ui::Font`.

## Werkzeuge

```sh
o1602-tool bsh-info GFX/SHIP.BSH
o1602-tool bsh-export GFX/TRAEGER.BSH out/ --sheet --count 96 --palette TOOLGFX/STADTFLD.COL
```
