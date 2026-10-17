# Chunk-Container

Fast alle Binärdateien bestehen aus einer Folge von Chunks:

```c
struct Chunk {
    char     name[16];   // NUL-terminiert, Rest undefiniert (oft Speichermüll)
    uint32_t size;       // Länge der Nutzdaten
    uint8_t  data[size];
};
```

Es gibt keinen Dateikopf, keine Ausrichtung zwischen Chunks und keine Prüfsumme. Die Datei endet
exakt nach dem letzten Chunk (geprüft an allen untersuchten Dateien).

Wichtig: Die Bytes nach dem NUL im Namen sind nicht genullt (z. B. `42 53 48 00 34 a0 40 00 …`
bei `BSH`). Nur bis zum ersten NUL vergleichen.

## Bekannte Chunk-Namen

| Name | Vorkommen | Inhalt |
|---|---|---|
| `BSH` | `.BSH` | Sprite-Sammlung → [bsh.md](bsh.md) |
| `ZEI` | `.ZEI` | Schrift-Glyphen (gleiches Format wie BSH) |
| `COL` | `.COL` | Palette → [col.md](col.md) |
| `INSEL3` / `INSEL5` | `.SCP .SZS .SZM .GAM` | Insel-Kopf (40 bzw. 116 Byte) → [scp.md](scp.md) |
| `INSELHAUS` | dito | Felder/Gebäude einer Insel, 8 Byte je Eintrag |
| `HIRSCH2`, `PRODLIST2`, `WERFT`, `MILITAR`, `SIEDLER`, `ROHWACHS2`, `STADT4`, `KONTOR2`, `MARKT2` | Szenarien, Spielstände | Inselbezogene Spielzustände (je Insel ein Satz) |
| `SZENE_PLAYERMIN`, `SZENE_PLAYERMAX`, `SZENE_GAMEID`, `SZENE_MISSNR`, `SZENE_RANKING` | Szenarien | je 4 Byte |
| `RANDTAB` | Szenarien | 512 Zufallszahlen (u16), aus denen der Weltplan zieht → [worldgen.md](../re/worldgen.md) |
| `AUFTRAG4` | Szenarien | 2244 Byte – Missionsziele |
| `SZENE`, `PLAYER4`, `HANDLER`, `SHIP4`, `SOLDAT3`, `TURM`, `TIMERS` | Szenarien, Spielstände | globale Spielzustände |

Die angehängten Ziffern (`INSEL5`, `PLAYER4`, …) sind vermutlich Versionsnummern der Struktur.

## Werkzeuge

```sh
o1602-tool chunks "Szenes/Das Atoll.szs"
python3 tools/python/anno_tool.py chunks NORD/big.scp
```
