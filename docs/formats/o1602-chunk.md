# Der eigene Chunk von Open1602 (`O1602`)

Spielstände von Open1602 sind `.GAM`-Dateien des Originals ([savegame-writer.md](../re/savegame-writer.md))
mit einem zusätzlichen Chunk `O1602` hinter `WIFF`. Das Original kennt den Namen nicht und überspringt
ihn; speichert es den Stand neu, fällt er weg. Open1602 liest ihn nach den Chunks des Originals und
ersetzt damit, was er enthält.

In Open1602: `engine/sim/include/o1602/sim/savegame_extension.hpp`.

## Was er enthält und warum

Er ist **maßgeblich** für alles, was er enthält. Listen stehen vollständig und in ihrer Reihenfolge
darin, nicht nur das, was dem Original fehlt. So hängt das Weiterspielen nicht davon ab, wie das Original
Werte rundet, Bits zusammenfasst oder Listen ordnet:

- Spieleinstellungen (`sim::Rules`) der Partie;
- Spielzeit auf die Millisekunde, Uhren der Häuser und des Wachstums, Dürretermin und -saat, die
  Zufallsgeneratoren der Simulation, die nächste Nummer der Katastrophen;
- je Insel: Wildplätze mit genauer Zeit, Phase der Inselepochen, Dürre, Krater, Erzvorkommen,
  Fruchtbarkeit;
- Spieler, Städte (Lager aller Waren, Reservierungen, Zulieferungen, Versorgung mit 64-Bit-Zählern und
  Verlauf, Stolz, Ausbauwarteschlange, Ereignistermin), die Produktionsliste mit ihren freien Plätzen,
  Häuser, Wachstumseinträge, Werften und Burgen, Schiffe mit genauer Ladung, Soldaten mit genauer
  Position, Tiere;
- Brände und Seuchen (S-001), laufende Vulkanausbrüche und Lava im Flug (S-062);
- die Uhren der Simulation und alle Figuren: Träger, Karren, Bauern, Herden, Fischer, Hirsche, Jäger,
  Ärzte, Feuerwehr, Räuber, mit Weg, Ladung und Reservierungen, dazu die Statistik der Logistik.

**Felder** stehen nur dort darin, wo das Laden der Chunks des Originals etwas anderes ergäbe:
reifes Gras und Fischgründe außerhalb der Städte, die das Original nicht speichert. Dazu prüft der
Schreiber, was die geschriebenen `INSELHAUS`-Chunks beim Laden ergeben, und vergleicht beide Ebenen
jedes Felds.

Ergebnis (Datentest): Ein Spiel, gespeichert und geladen, läuft **genau so weiter, als wäre es nie
angehalten worden**. Auch mit Lava in der Luft, einem Brand und der Feuerwehr unterwegs ist es fünf
Minuten später Byte für Byte derselbe Spielstand wie ohne Unterbrechung.

## Aufbau

Alle Zahlen little-endian.

| Feld | Typ | Inhalt |
|---|---|---|
| Kennung | 8 Byte | `O1602SAV` |
| Version | u16 | 1 |
| Flags | u16 | 0 |
| Fingerabdruck | u64 | FNV-1a (64 Bit) über alle Bytes der Datei vor dem Chunk (also Chunks des Originals und `WIFF`) |
| Abschnitte | … | bis zum Ende des Chunks |

**Abschnitt:** u32 Kennung (vier Zeichen), u16 Version, u16 0, u32 Länge, Daten. Ein Leser
überspringt Abschnitte, die er nicht kennt, und Versionen, die er nicht lesen kann; dann gilt für diesen
Teil, was die Chunks des Originals sagen. Spätere Fassungen können so Teile hinzufügen, ohne ältere
Spielstände ungültig zu machen.

| Kennung | Inhalt |
|---|---|
| `RULE` | Einstellungen als Paare (u16 Schlüssel, u32 Wert); unbekannte Schlüssel werden übergangen |
| `WRLD` | Spielzeit, Stadtnamenzähler, Schwierigkeit und Einstellungswort, Dürretermin und -saat, Zufallsgeneratoren (Zustand, Inkrement), Uhren der Häuser und des Wachstums, nächste Katastrophennummer |
| `ISLE` | je Insel in der Reihenfolge der Welt, mit Inselnummer zur Prüfung |
| `TILE` | abweichende Felder: Inselnummer, x, y, Ebene, Gebäude-Id, Teilfeld, Drehung, Phase, Besitzer, Stadt, Zufallswert |
| `PLAY` `CITY` `PROD` `HOUS` `GROW` `COST` `SHIP` `SOLD` `ANIM` `DISA` `VOLC` | die Listen der Welt, vollständig |
| `SIMU` | Uhren der Simulation und Figuren (eigene Versionsnummer am Anfang) |

Gebäude stehen als **Id** (aus `haeuser.cod`) und Teilfeld, nicht als Bildnummer, damit ein Spielstand
auch mit geänderten Grafiken passt; eine unbekannte Id macht den Chunk ungültig.

## Sicherheit beim Laden

- **Fingerabdruck:** Passt er nicht (ein anderes Werkzeug hat die Chunks des Originals verändert), lädt
  Open1602 nur die Chunks des Originals und meldet es.
- **Prüfung vor dem Übernehmen:** Erst wird alles gelesen und gegen die Welt geprüft (Inseln, Felder
  innerhalb ihrer Insel, freie Plätze der Produktionsliste, Zahl der Zufallsgeneratoren); nur wenn alles
  stimmt, wird es übernommen. Ein beschädigter oder abgeschnittener Chunk ändert nichts.
- Jede Anzahl wird gegen die verbleibende Länge geprüft, bevor Speicher reserviert wird.

## Grenzen dieser Fassung

- Mehr Inseln, Städte, Spieler oder Schiffe, als das Original fasst, kann Open1602 noch nicht anlegen;
  der Schreiber lehnt solche Welten ab, bis der Chunk sie selbst tragen kann.
- Befehle, die zwischen zwei Takten noch nicht ausgeführt sind, werden nicht gespeichert (gespeichert
  wird zwischen den Takten).
