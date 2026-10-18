# 0010 – Bedienung und Oberfläche

Status: angenommen · 18.10.2026 (Tastenbelegung vom Projektinhaber als Standard bestätigt)

## Kontext
Die Oberfläche des Originals ist nachgebaut: Seitenleiste, Baumenü, die Fenster der Auswahl. Ihre
Bedienung ist von 1998: Befehle gibt die linke Taste, die rechte bricht ab
(Bericht zur Auswahl, wird nach [ui.md](../re/ui.md) übernommen); viele Funktionen haben keine Taste, manche Tasten keine
Funktion; Schiffe versenken und Krieg erklären geschieht ohne Rückfrage; manches bietet die
Oberfläche nicht an, was die Simulation könnte (Erkunden an Werft und Piratenkontor, U-013).

Der Projektinhaber möchte die Oberfläche so überarbeiten, dass neue Menüs, Einstellungen und
Informationen Platz finden und die Bedienung heutigen Echtzeitstrategiespielen folgt (18.10.2026).

## Entscheidung
1. **Die Optik bleibt, die Bedienung wird modern.** Grafiken und Layouts des Originals sind die
   Grundlage; was das Original verbirgt oder nicht anbietet, darf die Oberfläche von Open1602 anbieten
   (Beispiel: Erkunden am Piratenkontor und an der Werft als Knopf, U-013). Neue Fenster und Knöpfe
   kommen als Daten dazu (ADR 0007).
2. **Rechtsklick gibt Befehle** (Standard): Bewegen, Angreifen, Anlegen, Handeln je nach Ziel. Die
   Linksklick-Steuerung des Originals ist eine Einstellung. Die linke Taste wählt aus (Klick, Rahmen,
   Shift fügt hinzu); Escape bricht ab und geht eine Seite zurück. Ziehen mit der rechten oder
   mittleren Taste verschiebt die Karte weiter.
3. **Befehle über die Minikarte:** ein Rechtsklick auf die Minikarte schickt die Auswahl dorthin.
4. **Befehlsketten:** Shift + Rechtsklick hängt einen Befehl an die Warteschlange der Einheit an
   (Wegpunkte). Die Warteschlange gehört zur Simulation (Befehl mit „anhängen“, deterministisch,
   gespeichert); eine Handelsroute bleibt davon getrennt.
5. **Tasten frei belegbar** im Einstellungsmenü. Die Vorgabe folgt heutigen Strategiespielen
   (unten); Tasten können in verschiedenen Zusammenhängen (Bauen, Einheiten) doppelt belegt sein, wenn
   diese nie gleichzeitig gelten. Die Belegung des Originals gibt es als zweite Vorlage.
6. **Rückfragen** vor Folgen, die sich nicht zurücknehmen lassen: Schiff versenken, Einheit auflösen
   und Krieg erklären durch einen Angriff. Die Rückfrage lässt sich in den Einstellungen abschalten.
7. **Videos der Meldungen laufen im Fenster der Minikarte**, das Spiel läuft weiter; ein Klick auf das
   Video beendet es. Intro, Abspann, Sieg und Niederlage bleiben bildschirmfüllend.

## Tastenbelegung (Standard)

**Kamera und Spiel**

| Taste | Funktion |
|---|---|
| W A S D, Pfeiltasten | Karte verschieben (Shift: schneller) |
| Q / E | Ansicht drehen |
| Mausrad, Bild↑ / Bild↓ | Zoom |
| Leertaste | zur letzten Meldung springen |
| Pos1 | Ansicht auf die Auswahl |
| F3, Pause | Pause |
| Num+ / Num− | Spielgeschwindigkeit |
| F5 / F9 | Schnellspeichern / Schnellladen |
| F10, Escape (wenn nichts abzubrechen ist) | Spielmenü |
| F12, Druck | Bildschirmfoto |
| F1 | Hilfe zum Fenster unter der Maus |

**Fenster**

| Taste | Funktion |
|---|---|
| B | Baumenü |
| K | Kampfmodus |
| I | Status/Info |
| O | Optionen |
| L / Shift+L | Schiffsliste / Städteliste |
| V | Diplomatie (Verträge) |

**Bauen**

| Taste | Funktion |
|---|---|
| R | Gebäude drehen |
| Shift halten | weiter bauen nach dem Setzen |
| C | Pipette: das Gebäude unter der Maus als Bauauftrag |
| Entf | Abreißwerkzeug |
| Escape, Rechtsklick | Werkzeug weglegen |

**Auswahl**

| Taste | Funktion |
|---|---|
| Linksklick / Ziehen | auswählen / Rahmen |
| Shift + Linksklick | zur Auswahl hinzu oder heraus |
| Doppelklick, Strg + Linksklick | alle sichtbaren Einheiten dieser Art |
| Strg+1…0 | Gruppe festlegen |
| Shift+1…0 | zur Gruppe hinzufügen |
| 1…0 | Gruppe wählen (zweimal: hinspringen) |
| . (Punkt) | nächstes untätiges Handelsschiff |
| , (Komma) | nächste untätige Kampfeinheit |
| Tab | durch die Arten in der Auswahl (Schiffe, Infanterie, Kavallerie …) |

**Befehle an Einheiten**

| Taste | Funktion |
|---|---|
| Rechtsklick (auch auf der Minikarte) | Befehl je nach Ziel |
| Shift + Rechtsklick | Befehl anhängen |
| A + Klick | Angriffsbewegung (greift unterwegs an) |
| S | Stopp |
| H | Stellung halten (verfolgt nicht) |
| P + Klick | Patrouille |
| F | Formation wechseln (Linie, Reihe, Block) |
| G | verteilen |
| Y | weiße Flagge hissen / streichen |
| X | Insel erkunden |
| T | Handel / Umladen am Kontor |
| R | Handelsroute bearbeiten (Schiff ausgewählt) |
| Entf | Einheit auflösen / Schiff versenken (mit Rückfrage) |

Angriffsbewegung, Stellung halten und das Springen zu untätigen Einheiten gibt es im Original nicht;
sie kommen mit dem Kampfmodus dazu.

## Folgen
- Die Eingabe läuft über eine Tabelle Aktion → Taste (Einstellungsdatei), nicht über feste Tasten im
  Code. Die heutigen festen Tasten des Betrachters werden darauf umgestellt.
- Die Simulation braucht eine Befehlswarteschlange je Schiff und Einheit sowie Gruppenbefehle mit
  Versatz je Einheit.
- Abweichungen vom Original in der Bedienung sind keine Fehler im Sinne von ADR 0006, sondern
  Einstellungen der Oberfläche; die Simulation bleibt davon unberührt.
