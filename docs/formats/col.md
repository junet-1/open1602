# COL – Palette

`TOOLGFX/STADTFLD.COL` (1044 Byte) = ein Chunk `COL` mit 1024 Byte Nutzdaten:

```c
struct ColEntry { uint8_t r, g, b, unused; };   // unused ist immer 0
ColEntry palette[256];
```

Eintrag 0 ist Schwarz. Dieselbe Palette gilt für alle Sprites aus `GFX/`, `MGFX/`, `SGFX/`
und `TOOLGFX/` (visuell geprüft: Gelände, Gebäude, Schiffe, Träger, Hauptmenü).
Das Spiel lädt sie in `Game_Init` (`0x00494870`, String `stadtfld.col` @ `0x0049C4B0`).

Transparenz wird nicht über einen Paletteneintrag, sondern über die RLE-Kodierung der Sprites
abgebildet (siehe [bsh.md](bsh.md)).

**Offen:** Palettenanimation (Wasser, Feuer?) – prüfen, ob das Spiel zur Laufzeit Einträge rotiert
(`IDirectDrawPalette::SetEntries`).
