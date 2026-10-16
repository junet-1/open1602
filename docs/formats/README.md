# Dateiformate von Anno 1602

Status-Legende: ✅ verifiziert (Code + Test gegen Originaldaten) · 🟡 teilweise · ⬜ offen

| Format | Dateien | Status | Doku | Implementierung |
|---|---|---|---|---|
| Chunk-Container | `.BSH .ZEI .COL .SCP .SZS .SZM .GAM` | ✅ | [chunks.md](chunks.md) | `engine/assets/chunk_file.*` |
| COD-Skripte | `haeuser.cod figuren.cod` | ✅ Kodierung, Grammatik, Objekte | [cod.md](cod.md) | `engine/assets/cod.*`, `cod_parser.*` |
| Texte | `Text.cod Editor.cod` | 🟡 Kodierung, Abschnitte | [cod.md](cod.md#7-textcod--editorcod) | – |
| Palette | `TOOLGFX/STADTFLD.COL` | ✅ | [col.md](col.md) | `engine/assets/palette.*` |
| Sprites | `GFX/ MGFX/ SGFX/ TOOLGFX/ *.BSH` | ✅ | [bsh.md](bsh.md) | `engine/assets/sprite_sheet.*` |
| Schriften | `TOOLGFX/*.ZEI` | 🟡 | [bsh.md](bsh.md#zei-schriften) | `engine/assets/sprite_sheet.*` |
| Inseln | `NORD/ SUED/ NORDNAT/ SUEDNAT/ NOKLIMA/ *.SCP` | ✅ Felder, Attribute · 🟡 Inselkopf | [scp.md](scp.md) | `engine/assets/island.*`, `engine/sim/island_map.*` |
| Szenarien / Spielstände | `Szenes/*.SZS *.SZM`, `SAVEGAME/*.GAM` | 🟡 Inseln, Spieler, Schiffe (Grunddaten) | [gam.md](gam.md) | `engine/assets/scenario.*`, `engine/sim/world.*` |
| Figuren | `figuren.cod` (FIGUR) | 🟡 Zeichendaten | [gam.md](gam.md#darstellung-der-schiffe) | `engine/assets/figure_table.*` |
| UI-Gadgets | `GADDATA/*.GAD`, `Gadedit/*.GAD`, `*.INC` | ⬜ (Klartext) | – | – |
| Videos | `VideoSmk/*.SMK`, `TOOLGFX/6s.smk` | ⬜ (Smacker, extern dokumentiert) | – | – |
| Audio | `SAMPLES/ Speech8/ MUSIC8/ *.wav` | ✅ (Standard-WAV) | – | – |

Alle Binärformate sind Little-Endian. Texte sind Windows-1252.
