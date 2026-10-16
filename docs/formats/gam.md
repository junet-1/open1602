# GAM / SZS / SZM – Spielstände und Szenarien

Spielstände (`.GAM`, im Original unter `SAVEGAME\`), Einzelspieler-Szenarien (`.SZS`) und
Mehrspieler-Karten (`.SZM`) haben **dasselbe Format**. Es ist ein Chunk-Container
(siehe [chunks.md](chunks.md)). `1602.exe` kennt genau eine Tabelle der Chunk-Namen
(`0x00498244`), die Lade- und Speicherroutinen beider Dateiarten verwenden:

```
CUSTOM  NAME  RANDTAB  SZENE  AUFTRAG4 (AUFTRAG2, AUFTRAG)  PLAYER4 (PLAYER3, PLAYER2)  TIMERS  TURM
SOLDATINSEL  SOLDAT3 (SOLDAT2)  HANDLER  SHIP4  HIRSCH2  STADT4 (STADT3)  MARKT2  KONTOR2  MILITAR
WERFT  SIEDLER  ROHWACHS2  PRODLIST2  INSEL5 (INSEL4, INSEL3)  INSELHAUS
```

Namen in Klammern sind ältere Versionen, die nur noch geladen werden. Die Inseln sind in
[scp.md](scp.md) beschrieben. Open1602 lädt Spielstände und Szenarien deshalb mit demselben Code
(`sim::load_world`). Der Betrachter öffnet Spielstände mit `open1602 --savegame NAME`.

> In der GOG-Installation des Autors gibt es noch keine Spielstände. Geprüft ist das Format bisher
> an allen Szenarien. Ein Test mit einer echten `.GAM`-Datei steht noch aus.

## PLAYER4 – Spieler (7 × 0x430 Byte)

Sieben Plätze: vier Spieler (Mensch oder Computer), der freie Händler, die Piraten und die
Eingeborenen. Die Nummer des Platzes ist zugleich die **Besitzernummer** in den Inselfeldern
(Attributbits 22–25) und bei Schiffen. 7 bedeutet „niemand“.

| Offset | Typ | Inhalt |
|---|---|---|
| 0x000 | int32 | Geld |
| 0x004 | u8 | Steuerung: 0 Mensch, 0x0B Eingeborene, 0x0C Computer, 0x0D freier Händler, 0x0E Piraten |
| 0x005 | u8 | Platznummer (0–6) |
| 0x006 | u8 | immer 1 (Bedeutung offen) |
| 0x007 | u8 | **Spielerfarbe** (siehe unten) |
| 0x009 | u8 | im Netzwerkspiel ausgewertet (`0x00473C50`) |
| 0x3BC | char[64] | Name (in Szenarien leer) |
| 0x3FC | char[64] | zweiter Name (Netzwerk) |

Belegt durch die Mehrspieler-Listenfunktion `0x00473C50` (Datensatzgröße 0x430) und alle Szenarien.
Beispiel „Das Atoll“: Farben 0, 1, 2, 3, 6, 4, 5 für die Plätze 0–6. Im „Endlosspiel00“ hat der Mensch
die blaue und der erste Computergegner die rote Flagge: Die Farbe gehört also nicht fest zu einem Platz.

## Spielerfarben

Die Farbe ist ein Index 0–6. Sichtbar wird sie im Original nur durch **eigene Sprite-Varianten**,
nicht durch eine Umfärbung der Palette. Die Flagge eines Schiffs wählt das Spiel über die Tabelle
`0x00498F2C` (Farbe → Figurnummer in `figuren.cod`):

| Farbe | Bedeutung | Flaggenfigur | Sprites in `SHIP.BSH` |
|---|---|---|---|
| 0 | rot | FAHNE1 (106) | 192–199 |
| 1 | blau | FAHNE2 (107) | 200–207 |
| 2 | gelb | FAHNE3 (108) | 208–215 |
| 3 | weiß | FAHNE4 (109) | 216–223 |
| 4 | Piraten | FAHNEPIRAT (110) | 224–231 (Totenkopf) |
| 5 | Eingeborene | – (keine Flagge) | |
| 6 | freier Händler | – (keine Flagge) | |

`FAHNEWEISS` (111, Sprites 232–239) ist die weiße Fahne und keine Spielerfarbe. Die acht Sprites
je Farbe sind die Animationsphasen des Wehens. Ebenso gibt es eigene Varianten für Marktflaggen
(`FAHNEMARKT`, `ANIM` 0–5 je 8 Bilder) und vermutlich für die Soldaten (`SOLDAT.BSH`, noch nicht untersucht).

> Korrektur: Eine frühere Fassung von [rendering.md](../re/rendering.md) vermutete hinter den
> 256-Byte-Tabellen ab `0x005B6360` Spielerfarben. Es sind **Helligkeits- und Farbstich-Tabellen**,
> die beim Start aus der Palette berechnet werden (`0x00494870`: Faktoren 1,5 / 0,62 / 0,62 / 0,9 /
> 0,55 und eine Tabelle mit Blaustich 0 / 0,4 / 1,7). Index 6 heißt „unverändert“.

## SHIP4 – Schiffe (je 0x1B4 Byte)

Geschrieben von `0x004600C0`, das auch `HANDLER`, `SOLDAT3` und `TURM` speichert. Bisher genutzt:

| Offset | Typ | Inhalt |
|---|---|---|
| 0x000 | char[28] | Name |
| 0x01C | u16 | x (Weltfeld) |
| 0x01E | u16 | y (Weltfeld) |
| 0x046 | u16 | laufende Nummer |
| 0x048 | u16 | Figurnummer (HANDEL1 = 21, HANDEL2 = 23, KRIEG1 = 25, KRIEG2 = 27, HANDLER = 29, PIRAT = 31) |
| 0x04A | u8 | Figurart aus der Figurdefinition (1 Handelsschiff, 3 Pirat, …) |
| 0x04B | u8 | Besitzer (Platz in PLAYER4, 7 = niemand) |
| 0x04D | u8 | Richtung 0–7 (45°-Schritte; 0xFF = nicht gesetzt) |
| 0x052… | | Ladung, Kanonen, Handelsroute (noch nicht entschlüsselt) |

## Darstellung der Schiffe

Nach der Figurenzeichenroutine `0x0044FA90` (Fall 1–3 = Schiffe):

- **Sprite** = `Gfx + ((Richtung + 2 · Ansichtsrotation) & 7) · Rotate` (`0x0044FA60`; Schiffe haben `Rotate: 1`).
- **Position:** Figurpunkt → Bildschirm wie bei den Feldern (`0x0044D940`:
  x = (fx − fy) · Breite/2, y = (fx + fy) · (Höhe+1)/2). Das Sprite wird waagrecht zentriert, seine
  Unterkante liegt bei y + `Posoffs[0]` · Breite/64 (Feld +0x0C der Figurdefinition).
- **Flagge:** Figur aus der Farbtabelle, Bild `Gfx + Animationsphase (0–7)`. Ihr Abstand vom
  Figurpunkt ist `Fahnoffs[1]` entlang der Schiffsachse, gedreht um (Richtung + 2) · 45°
  (`0x00443180`), projiziert wie oben. Gehoben wird sie um `Fahnoffs[2]` · Breite/2 (`0x00451220`).
  Die Flagge hängt von diesem Punkt nach unten.
- **Reihenfolge:** Figuren werden zwischen den beiden Durchgängen einer Bildschirmzeile gezeichnet
  (`0x00489A60`), sortiert nach Bildschirm-y. Open1602 ordnet ein Schiff der letzten Feldzeile zu,
  deren Raute oberhalb der Rumpfunterkante beginnt. So überdecken flache Meeresfelder den Rumpf nie.

Offene Punkte: genaue Bezugshöhe beim Einsortieren, Animationsphase der Flagge (Byte +0x0B der
Laufzeitfigur; Open1602 nimmt 90 ms je Bild wie `FAHNEMARKT`), Bugwellen (`BUGH`), Schiffe in Fahrt
(Gleitkomma-Position).
