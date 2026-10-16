# engine/assets

Loader für die Originalformate (Spezifikation in [docs/formats](../../docs/formats/README.md)):

| Header | Format |
|---|---|
| `chunk_file.hpp` | Chunk-Container (Basis fast aller Binärdateien) |
| `cod.hpp` | `.cod`-Skripte entschlüsseln |
| `palette.hpp` | `.COL`-Palette |
| `sprite_sheet.hpp` | `.BSH`/`.ZEI`-Sprites (RLE) |
| `island.hpp` | Inseln aus `.SCP`/`.SZS`/`.SZM`/`.GAM` |
| `game_directory.hpp` | Zugriff auf eine Originalinstallation |

Keine Abhängigkeit von Plattform oder Grafik; jede Funktion liefert `core::Result` und stürzt bei
fehlerhaften Daten nicht ab.
