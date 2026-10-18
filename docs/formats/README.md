# Dateiformate von Anno 1602

Status-Legende: ✅ verifiziert (Code + Test gegen Originaldaten) · 🟡 teilweise · ⬜ offen

| Format | Dateien | Status | Doku | Implementierung |
|---|---|---|---|---|
| Chunk-Container | `.BSH .ZEI .COL .SCP .SZS .SZM .GAM` | ✅ | [chunks.md](chunks.md) | `engine/assets/chunk_file.*` |
| COD-Skripte | `haeuser.cod figuren.cod` | ✅ Kodierung, Grammatik, Objekte | [cod.md](cod.md) | `engine/assets/cod.*`, `cod_parser.*` |
| Texte | `Text.cod Editor.cod` | 🟡 Kodierung, Abschnitte; Zuordnung der Zeilen nur, wo gebraucht (Stadtnamen, Spielernamen) | [cod.md](cod.md#7-textcod--editorcod) | `engine/assets/text_table.*` |
| Palette | `TOOLGFX/STADTFLD.COL` | ✅ | [col.md](col.md) | `engine/assets/palette.*` |
| Sprites | `GFX/ MGFX/ SGFX/ TOOLGFX/ *.BSH` | ✅ | [bsh.md](bsh.md) | `engine/assets/sprite_sheet.*` |
| Schriften | `TOOLGFX/*.ZEI` | ✅ Zeichen, Maße, Farben, Umbruch | [bsh.md](bsh.md#zei-schriften), [ui.md](../re/ui.md#schriften) | `engine/assets/sprite_sheet.*`, `engine/ui/font.*` |
| Inseln | `NORD/ SUED/ NORDNAT/ SUEDNAT/ NOKLIMA/ *.SCP` | ✅ Felder, Attribute · 🟡 Inselkopf | [scp.md](scp.md) | `engine/assets/island.*`, `engine/sim/island_map.*` |
| Szenarien / Spielstände | `Szenes/*.SZS *.SZM`, `SAVEGAME/*.GAM` | 🟡 alle Chunks des Spielstands gelesen und geschrieben (Byte für Byte zurück); offen: Szenario-Haltung in `PLAYER4`, KI-Auftragsfelder in `SHIP4`, Teile von `TIMERS`, die `SZENE`-Chunks der Szenarien (gelesen: Inselliste, `RANDTAB`, Missionsnummer) | [gam.md](gam.md), [savegame-writer.md](../re/savegame-writer.md) | `engine/assets/scenario.*`, `savegame.*`, `engine/sim/world.*`, `savegame_writer.*` |
| Spielstand von Open1602 | `*.o1602` | ✅ (Open1602, eigenes Format) | [o1602-save.md](o1602-save.md) | `engine/sim/save_game.*` |
| Eigener Chunk | `O1602`, `O1602MAP` in exportierten `.GAM` | ✅ (Open1602) | [o1602-chunk.md](o1602-chunk.md) | `engine/sim/savegame_extension.*` |
| Figuren | `figuren.cod` (FIGUR) | 🟡 Zeichen-, Lauf- und Kampfdaten, `FORMATION` (die Oberfläche wählt noch keine) | [gam.md](gam.md#darstellung-der-schiffe), [figures.md](../re/figures.md) | `engine/assets/figure_table.*` |
| UI-Gadgets | `GADDATA/*.GAD`, `*.INC` | ✅ alle 45 Layouts des Spiels (`Gadedit/` des Editors nicht) | [gad.md](gad.md) | `engine/assets/gad.*`, `engine/ui/panel.*` |
| Videos | `VideoSmk/*.SMK`, `TOOLGFX/6s.smk` | ✅ Smacker 2 (Bild, Palette, alle Tonarten; `SMK4` nur nach Beschreibung) | [smk.md](smk.md) | `engine/video/smacker.*` |
| Audio | `SAMPLES/ Speech8/ MUSIC8/ *.wav` | ✅ PCM und die eigene 4-Bit-Musik (IMA-Abart) | [wav.md](wav.md) | `engine/audio/wav.*` |

Alle Binärformate sind Little-Endian. Texte sind Windows-1252.
