# 0009 – Eigenes Speicherformat

Status: angenommen · 18.10.2026

## Kontext
Bisher waren Spielstände von Open1602 `.GAM`-Dateien des Originals mit einem Zusatz-Chunk `O1602`. Der
Schreiber baute die Chunks des Originals aus den Datensätzen der Quelldatei nach, der Zusatz-Chunk trug
den vollständigen Zustand ein zweites Mal. Das hatte drei Nachteile:

- **Grenzen:** Die Chunks des Originals fassen 7 Spieler, 150 Schiffe, 400 Soldaten und 350 Türme. Mit
  den Standardregeln von Open1602 (100 Schiffe, 250 Einheiten je Spieler) ist das erreichbar; dann schlug
  das Speichern fehl.
- **Aufwand und Fehler:** Derselbe Zustand stand zweimal in der Datei, und manche Bytes hingen davon ab,
  woraus das Spiel ursprünglich geladen wurde (zwei solche Fehler allein am 17.10.2026).
- **Größe:** Feste Tabellen des Originals, ungepackt (Merlin04: 885 KB).

Der Projektinhaber legt Wert auf ein effizientes, einfaches und robustes Format; dass das Original
Spielstände von Open1602 laden kann, ist ihm nicht wichtig (18.10.2026).

## Entscheidung
1. Open1602 speichert in einem **eigenen Format** (`.o1602`, [o1602-save.md](../formats/o1602-save.md)):
   kurzer Kopf mit Version, Größe und CRC-32, dahinter versionierte Abschnitte mit dem ganzen Zustand,
   mit zlib gepackt.
2. Die Abschnitte sind die des bisherigen Zusatz-Chunks, ergänzt um die ganze Karte (`MAPW`) und was
   sonst nur in den Chunks des Originals stand. Es gibt **einen** Weg zu schreiben und **einen** zu lesen.
3. Das Format ist **kanonisch**: gleicher Zustand, gleiche Bytes. Gebäude stehen als Id aus
   `haeuser.cod`, nicht als Bildnummer (ADR 0007: Kennungen statt Laufzeitnummern).
4. Die Dateien des Originals werden weiter **gelesen**. Ein Spiel lässt sich als `.GAM` **exportieren**,
   solange es hineinpasst; dafür bleibt der bisherige Schreiber mit dem Zusatz-Chunk.

## Folgen
- Keine Grenzen außer dem Speicher; neue Teile kommen als neue Abschnitte dazu, ältere Leser
  überspringen sie.
- Spielstände sind etwa ein Viertel so groß.
- Bestehende `.GAM`-Stände von Open1602 laden weiter (mit ihrem Zusatz-Chunk genau wie bisher).
- Ein aus einem `.o1602` geladenes Spiel hat keine Quelldatei mehr; ein Export füllt die Felder, die
  Open1602 nicht führt, mit 0 (im Original ungeprüft).
- Später kommt in den Kopf oder einen Abschnitt die Liste der Inhaltspakete mit Versionen (ADR 0007).
