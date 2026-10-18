# Spielstand von Open1602 (`.o1602`)

Open1602 speichert Spiele in einem eigenen Format ([ADR 0009](../architecture/0009-eigenes-speicherformat.md)).
Es enthält den vollständigen Zustand eines Spiels, hängt von keiner anderen Datei ab und kennt keine
Grenzen des Originals (Spieler, Schiffe, Soldaten, Türme, Kartengröße). Ein gespeichertes Spiel läuft
nach dem Laden **genau so weiter, als wäre es nie angehalten worden**; das prüfen Datentests mit einem
echten Spielstand (Lava in der Luft, ein Brand mit Feuerwehr) und mit einem Endlosspiel voller
Computergegner.

In Open1602: `engine/sim/include/o1602/sim/save_game.hpp` (`write_game`, `read_game`, `save_info`).
Die Abschnitte teilt sich das Format mit dem Zusatz-Chunk exportierter `.GAM`-Dateien
([o1602-chunk.md](o1602-chunk.md)), `engine/sim/src/savegame_extension.cpp`.

Die Dateien des Originals (`.SZS`, `.SZM`, `.GAM`) liest Open1602 weiter. Ein Spiel lässt sich als
`.GAM` exportieren, solange es in die Grenzen des Originals passt ([savegame-writer.md](../re/savegame-writer.md)).

## Aufbau

Alle Zahlen little-endian.

| Versatz | Typ | Inhalt |
|---|---|---|
| 0 | 8 Byte | Kennung `OPEN1602` |
| 8 | u16 | Formatversion, derzeit 1 |
| 10 | u16 | Schalter: Bit 0 = die Abschnitte sind ein zlib-Strom |
| 12 | u32 | 0 |
| 16 | u64 | Größe der Abschnitte (ungepackt) |
| 24 | u32 | CRC-32 (IEEE, wie zlib und PNG) der ungepackten Abschnitte |
| 28 | u32 | Größe der Daten hinter dem Kopf (gepackt oder nicht) |
| 32 | … | die Abschnitte, mit zlib gepackt (Stufe 6), wenn Bit 0 gesetzt ist |

Ohne zlib im Build schreibt Open1602 die Abschnitte ungepackt; eine gepackte Datei lädt ein solcher
Build mit einer klaren Meldung nicht.

**Abschnitt:** u32 Kennung (vier Zeichen), u16 Version, u16 0, u32 Länge, Daten. Ein Leser überspringt
Abschnitte, die er nicht kennt, und Versionen, die er nicht lesen kann. Spätere Fassungen können so
Teile hinzufügen, ohne ältere Spielstände ungültig zu machen.

| Kennung | Inhalt |
|---|---|
| `META` | Breite, Höhe, Spielzeit, Fingerabdruck der Gebäudedaten (FNV-1a über Id, Bild und Größe jedes Gebäudes aus `haeuser.cod`), Name des ersten menschlichen Spielers: für eine spätere Liste der Spielstände |
| `MAPW` | die ganze Karte: Weltgröße, je Insel der Kopf (Kennung, Nummer, Größe, Lage, Vorlage, Klima), die Lage auf der Weltkarte und **beide Ebenen aller Felder**. Die Werte stehen nach Art geordnet (erst die Gebäude-Ids aller Felder zeilenweise, dann alle Teilfelder, Drehung und Phase, Besitzer, Stadt, Zufallswert); das packt zlib gut |
| `RULE` | Einstellungen (`sim::Rules`) als Paare (u16 Schlüssel, u32 Wert); unbekannte Schlüssel werden übergangen |
| `WRLD` | Spielzeit, Stadtnamenzähler, Schwierigkeit und Einstellungswort, Dürretermin und -saat, Zufallsgeneratoren, Uhren der Häuser und des Wachstums, nächste Katastrophennummer |
| `WRL2` | weitere Einstellungen der Welt: Abstand zwischen zwei Piratenschiffen |
| `ISLE` | je Insel: Wildplätze, Dürre, Krater, Lagerstätten, Fruchtbarkeit |
| `PLAY` `CITY` `PROD` `HOUS` `GROW` `COST` `ANIM` `DISA` `VOLC` | Spieler, Städte, Produktionsliste mit freien Plätzen, Häuser, Wachstum, Werften und Burgen, Tiere, Brände und Seuchen, Vulkane mit Lava im Flug |
| `PSTA` | je Spieler (Reihenfolge wie `PLAY`): Auftrag, Stand (spielt, besiegt, abwesend), gewonnen, besiegte Gegner, die beiden Zähler der Niederlageregeln, Handelsstatistik (Käufe, Verkäufe) |
| `CSTA` | je Stadt (Insel, Nummer): Handelsstatistik des Kontors (Käufe, Verkäufe, wie Stadt + 0x1F0/+0x1F4); fehlt der Abschnitt, gelten die Werte aus `STADT4` |
| `MISN` | die Aufträge (Ziele, Verlustbedingungen, Werte, Text, die beim Start festgehaltenen Inseln der Inselziele) und das Spielende (Zeit, Sieger) ([missions.md](../re/missions.md)) |
| `FLEE` `ARMY` `DIPL` `EPOC` `XPLR` | Schiffe mit Kurs, Route und Ladung, Werftaufträge, Handelseinstellungen; Soldaten, Türme, Gebäudeschaden, Schüsse im Flug, Burgplätze; Diplomatie; Inselepochen; Erkunden |
| `QUEU` `STNC` | Befehlsketten der Schiffe und Einheiten (Shift+Rechtsklick, je bis 16 Ziele); Angriffsbewegung und „Stellung halten“ (ADR 0010, gibt es im Original nicht) |
| `SIMU` | Uhren der Simulation, alle laufenden Figuren und der Zustand der Computergegner |
| `ORIG` | die Datei des Originals, aus der das Spiel stammt (Spielstand, Szenario oder die Vorlage eines neuen Spiels), als Chunks ohne die Inselfelder (`INSELHAUS`), etwa 12 KB gepackt. Nur für den Export als `.GAM`: Er nimmt daraus die Bytes, die Open1602 nicht führt, genau wie bei einem Spiel, das direkt aus der Datei geladen wurde. Fehlt der Abschnitt, schreibt der Export Vorgaben wie ein neues Spiel |

Die Einzelheiten der Abschnitte ab `RULE` stehen in [o1602-chunk.md](o1602-chunk.md).

**Gebäude** stehen als Id aus `haeuser.cod` mit Teilfeld, nie als Bildnummer. Passt der Fingerabdruck
der Gebäudedaten nicht, lädt Open1602 trotzdem und meldet es; eine Id, die die Daten nicht kennen,
macht den Spielstand unlesbar (statt falscher Gebäude).

## Eigenschaften

- **Kanonisch:** Derselbe Zustand ergibt dieselben Bytes, gleich woher das Spiel kam. Speichern,
  Laden und wieder Speichern liefert die Datei unverändert; zwei Spiele lassen sich Byte für Byte
  vergleichen (die Tests tun das).
- **Robust:** Erst prüft der Lader Kopf, Größe und CRC-32, dann liest er alle Abschnitte und prüft sie
  gegen die Welt (Inseln, Felder innerhalb ihrer Insel, Schiffe in der Welt, Einheiten auf bekannten
  Inseln, Zufallsgeneratoren). Nur wenn alles stimmt, entsteht die Welt; ein beschädigter, abgeschnittener
  oder neuerer Spielstand lädt nichts. Jede Anzahl wird gegen die verbleibende Länge geprüft, bevor
  Speicher reserviert wird. Geschrieben wird erst in eine Nebendatei, die dann umbenannt wird.
- **Klein:** Merlin04 (130 Minuten, zwei Städte) braucht 248 KB statt 885 KB als `.GAM` mit Zusatz-Chunk.

## Im Betrachter

- **F5** speichert nach `quicksave.o1602` im Benutzerordner von Open1602 (Linux:
  `~/.local/share/Open1602/saves/`, Windows: `%APPDATA%\Open1602\saves\`), nie in die Installation.
- `--save DATEI` speichert beim Beenden: im eigenen Format, mit der Endung `.gam` als Spielstand des
  Originals (wenn das Spiel hineinpasst).
- `--load DATEI` lädt einen Spielstand von Open1602 (oder eine Datei des Originals über ihren Pfad).

## Grenzen dieser Fassung

- Inseln werden in einigen Abschnitten über ihre Nummer (8 Bit) angesprochen; eine Welt hat höchstens
  255 Inseln, eine Insel höchstens 255 × 255 Felder (so weit reichen auch Weltengenerator und Vorlagen).
- Befehle, die zwischen zwei Takten noch nicht ausgeführt sind, werden nicht gespeichert (gespeichert
  wird zwischen den Takten).
- Der Export als `.GAM` eines aus einem `.o1602` geladenen Spiels gleicht dem eines direkt geladenen
  Byte für Byte (Test über game00 und game10), bis auf die sechs Bits unbekannter Bedeutung je Inselfeld
  (`INSELHAUS` +4 Bits 26–31), die `ORIG` nicht mitnimmt. Ein Spielstand ohne `ORIG` (ältere
  Fassung) exportiert Spieler und Inselvorlagen richtig, den Rest mit Vorgaben; der erste Export
  ohne beides stürzte im Original nach zehn Sekunden ab (alle Spieler rot, Inselvorlagen 0).
