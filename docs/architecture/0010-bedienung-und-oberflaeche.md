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
| F + Klick | Angriffsbewegung (greift unterwegs an; „Feuer“) |
| H | Stopp („Halt“) |
| G | Stellung halten (verfolgt nicht) |
| P + Klick | Patrouille |
| C | Formation wechseln (Linie, Reihe, Block) |
| Shift+C | verteilen |
| Y | weiße Flagge hissen / streichen |
| X | Insel erkunden |
| T | Handel / Umladen am Kontor |
| R | Handelsroute bearbeiten (Schiff ausgewählt) |
| Entf | Einheit auflösen / Schiff versenken (mit Rückfrage) |

Angriffsbewegung, Stellung halten und das Springen zu untätigen Einheiten gibt es im Original nicht.

Keine Taste der Einheiten ist zugleich eine allgemeine Taste (18.10.2026: Angriff von A auf F, Stopp von
S auf H, dafür Stellung halten auf G, Formation auf C, verteilen auf Shift+C), W A S D verschieben also
immer. Doppelt belegt sind nur Tasten in Zusammenhängen, die nie zugleich gelten: R und Entf (Bauen:
drehen, abreißen; Einheiten: Route, auflösen), C (Bauen: Pipette; Einheiten: Formation) und Tab.

Umgesetzt (18.10.2026): Tabelle `ui::KeyBindings` mit dieser Vorgabe und der Einstellungsdatei
`keys.cfg` im Datenordner des Benutzers (bis zum Einstellungsmenü von Hand zu ändern; F1 zeigt die
Belegung), Auswahl mit Rahmen, bildpunktgenau wie das Original, Gruppen mit der Ziffer des Originals,
Formationen, Filtern und Tab (Arten der Auswahl nacheinander; `ui::UnitSelection`), Rechtsklick- und
Minikartenbefehle, Befehlsketten in der Simulation, die Rückfragen, die Fenster des Kampfmodus, die
Meldungsvideos im Minikartenfenster, Angriffsbewegung (Schiffe und Soldaten greifen Feinde in
Reichweite an, verfolgen sie und fahren danach weiter; im Spielstand `STNC`), Stellung halten (sie
schießen nur auf Feinde in Reichweite, verfolgen nicht), Schnellspeichern und -laden (F5, F9) und der
Erkunden-Knopf neben Reparieren und Piraten im Schiffsfenster (U-013), Schiffs- und Städteliste (L,
Shift+L), Diplomatiefenster mit Tribut (V; Rückfrage vor dem Bruch eines Friedensvertrags), die
Einstellungen der Seitenleiste und das Hauptmenü für Einzelspieler (Szenarien, Endlosspiel, Spielstände).
Noch nicht: Endbildschirm mit Bewertung und ein eigenes Spielmenü (Escape) mit Speichern, Laden, Einstellungen und Beenden. Das Original
hat kein Spielmenü: Speichern, Laden und Beenden liegen in den Einstellungen der Seitenleiste (umgesetzt
mit Rückfrage vor „Ende“).

## Folgen
- Die Eingabe läuft über eine Tabelle Aktion → Taste (Einstellungsdatei), nicht über feste Tasten im
  Code. Die heutigen festen Tasten des Betrachters werden darauf umgestellt.
- Die Simulation braucht eine Befehlswarteschlange je Schiff und Einheit sowie Gruppenbefehle mit
  Versatz je Einheit.
- Abweichungen vom Original in der Bedienung sind keine Fehler im Sinne von ADR 0006, sondern
  Einstellungen der Oberfläche; die Simulation bleibt davon unberührt.
