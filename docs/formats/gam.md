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
(`sim::load_world`). Der Betrachter öffnet Spielstände mit `open1602 --savegame NAME` (oder `--scenario DATEI`) und
schreibt sie mit F5 oder `--save DATEI` ([savegame-writer.md](../re/savegame-writer.md)).

## Unterschiede eines Spielstands zum Szenario

Geprüft an einem Spielstand, der direkt nach dem Start von „Das Atoll“ gespeichert wurde
(`SAVEGAME/game00.gam`, 153 Chunks statt 387):

- **Zwei `INSELHAUS` je Insel.** Die erste enthält die vollständige Grundinsel (die Einträge der
  Vorlage, mit bereinigten Attributen: Inselnummer gesetzt, Besitzer 7, Zufallsbits gelöscht), die
  zweite die Änderungen – bei diesem Spielstand genau die Einträge des Szenarios. Vorlagennummer
  und Flag `0x65` = 0 bleiben wie im Szenario; das Original lädt die Vorlage also auch hier, die erste
  Ebene überdeckt sie vollständig. Die Darstellung ist pixelgleich mit dem Szenario.
- **Inselbezogene Chunks nur einmal:** `PRODLIST2`, `WERFT`, `MILITAR`, `SIEDLER`, `ROHWACHS2`,
  `STADT4`, `KONTOR2`, `MARKT2` stehen einmal für alle Inseln (im Szenario je Insel).
- **`NAME`**: Name des Szenarios („Das Atoll“). **`WIFF`** (4 Byte): Summe aller 32-Bit-Wörter der Daten davor, vom Original nie gelesen ([savegame-writer.md](../re/savegame-writer.md)).
- Kein `SZENE`, `RANDTAB` und keine `SZENE_*`-Chunks.

## Namen der Spielstände (Game.dat)

Die Datei heißt immer `SAVEGAME/gameNN.gam` (NN = Platz 00–…). Den Namen, den man beim Speichern
eingibt, schreibt das Spiel in die Textdatei `Game.dat` im Spielordner:

```
  Objekt:   SPIELNAME
    Name:      0, "Merlin01", 1
    Name:      1, "Leer01", 1
```

`Game.dat` enthält außerdem Einstellungen (Musik, Lautstärke, Videos) und den zuletzt gespeicherten
Pfad (`Lastfile`, `lastgame.gam`). Das Spiel schreibt also in seinen Installationsordner (auch
`Szenes/*.hss`, vermutlich Bestenlisten) – siehe Fehler T-005. Open1602 liest `Game.dat` nur, um
Spielstände über ihren Namen zu finden (`open1602 --savegame Merlin01`, `o1602-tool savegames`).

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
| 0x00D | u8 | im Netzwerkspiel ausgewertet (`0x00473C50`) |
| 0x00E | u8 | Schlösser gebaut (Laufzeit +0x87, höchstens eines) |
| 0x00F | u8 | Kathedralen gebaut (+0x88) |
| 0x010, 0x012 | u16, u16 | Triumphbögen: Zuteilungen, gebaut (+0x8E, +0x90) |
| 0x034 | u32 | **erreichte Baustufen**: Bit n − 1 = INFRA-Stufe n (Laufzeit +0x6C, [construction.md](../re/construction.md#freischaltung)) |
| 0x008 | u8 | **Schuldiger**: wer zuletzt ein Kontor oder einen Markt dieses Spielers zerstört hat (Laufzeit +0x81, 7 = niemand; Fluch der Eingeborenen) |
| 0x038, 0x03A | u16, u16 | Denkmäler: Zuteilungen, gebaut (+0x8A, +0x8C) |
| 0x040 + 2b | u16 × 7 | Tribut von Spieler b erhalten, skaliert, verblasst (+0xB0) ([diplomacy.md](../re/diplomacy.md)) |
| 0x060 + 2b | u16 × 7 | Handel mit b, verblasst (+0xC0) |
| 0x080 + 4b | u32 × 7 | Schaden von b erlitten, 1/32 LP, verblasst (+0xD0) |
| 0x0C0 + 8b | u8, u32 | **Frieden** gegenüber b: 0 Krieg, 1 angeboten, 2 erhalten, 3 Vertrag; dahinter die Zeit der letzten Kriegserklärung (1/10 s, nur der eigene Eintrag) (+0xF0, +0xF4) |
| 0x140 + 8b | u8 | **Handelsvertrag** gegenüber b, gleiche Werte (+0x130) |
| 0x1C0 + 8i | 7 × {u8 Art, u8 Spieler, u16, u32 Zeit} | die ersten 7 der 32 Ereignisse, an die sich der Spieler erinnert (+0x170, S-085) |
| 0x3C0 | char[64] | Name des Spielers (in Szenarien leer, im Spielstand der Name des Menschen) |
| 0x400 | char[64] | zweiter Name (Netzwerk) |

Belegt durch die Mehrspieler-Listenfunktion `0x00473C50` (Datensatzgröße 0x430; sie liest ab
Datensatz + 4) und alle Szenarien. Beispiel „Das Atoll“: Farben 0, 1, 2, 3, 6, 4, 5 für die Plätze 0–6.

**Bestätigt am Spielstand:** Wählt der Spieler zu Beginn die gelbe Flagge, steht im Spielstand bei
Platz 0 Farbe 2, und die Computergegner rücken nach (Platz 1 rot, Platz 2 blau, Platz 3 weiß). Die
Farbe gehört also nicht fest zu einem Platz; die Besitzernummern auf Inseln und Schiffen bleiben die
Plätze.

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

**In Open1602** werden Platz, Steuerung und Farbe beim Laden übersetzt (`sim::load_world`): Plätze
0–6 werden zu `sim::PlayerId`, Besitzer 7 zu `sim::kNoPlayer`, die Steuerung zu `sim::PlayerRole`
und die Farbe zum gleichnamigen Banner des Basisspiels (`render::BannerTable::original`). Spielerzahl
und Bannerliste sind im Engine-Modell offen ([ADR 0007](../architecture/0007-erweiterbarkeit-und-modding.md)).

`FAHNEWEISS` (111, Sprites 232–239) ist die weiße Fahne und keine Spielerfarbe. Die acht Sprites
je Farbe sind die Animationsphasen des Wehens. Ebenso gibt es eigene Varianten für die Gebäudeflaggen
(`FAHNEMARKT`, `ANIM` 0–5 je 8 Bilder, dieselben Sprites; siehe [rendering.md](../re/rendering.md#6-gebäudeflaggen-anzeigefiguren)) und vermutlich für die Soldaten (`SOLDAT.BSH`, noch nicht untersucht).

> Korrektur: Eine frühere Fassung von [rendering.md](../re/rendering.md) vermutete hinter den
> 256-Byte-Tabellen ab `0x005B6360` Spielerfarben. Es sind **Helligkeits- und Farbstich-Tabellen**,
> die beim Start aus der Palette berechnet werden (`0x00494870`: Faktoren 1,5 / 0,62 / 0,62 / 0,9 /
> 0,55 und eine Tabelle mit Blaustich 0 / 0,4 / 1,7). Index 6 heißt „unverändert“.

## SHIP4 – Schiffe (je 0x1B4 Byte)

Geschrieben von `0x004600C0` aus der Schiffstabelle `0x004CF358` (150 × 0x218 Byte). Dieselbe
Routine speichert `HANDLER`, `SOLDAT3` und `TURM`. Jedes Schiff besitzt eine **Anzeigefigur** (Zeiger
bei Schiff + 4), die Position, Richtung und Figurnummer trägt.

| Offset | Typ | Inhalt |
|---|---|---|
| 0x000 | char[28] | Name |
| 0x01C | u16 | x (Weltfeld): Gleitkomma-Position der Anzeigefigur (+0x28), abgeschnitten |
| 0x01E | u16 | y (Weltfeld), ebenso (+0x2C) |
| 0x024 | u16 | Schiff + 0x1AE (offen) |
| 0x02C | u32 | **befohlenes Ziel** (Verweis, [ships.md](../re/ships.md#daten)) |
| 0x030 | u32 | **aktuelles Ziel** (Verweis) |
| 0x034 | u32 | **letztes Ziel** (Verweis, z. B. das Kontor, das der Händler zuletzt besucht hat) |
| 0x03C | u16 | **Lebenspunkte** in 1/32 |
| 0x03E | u16 | Schiff + 0x1AC (offen) |
| 0x040 | u8 | Zähler, bei jedem Stationswechsel um 1 vermindert |
| 0x041 | u8 | Planungsradius (0x50) |
| 0x042 | u8 | **Kanonen** an Bord |
| 0x043 | u8 | Schalter: Bit 1 Handelsroute an, Bit 3 Einbahnroute, Bit 2 zum Verkauf, Bit 4 in Reparatur, Bit 0 Merker der Oberfläche |
| 0x044 | u16 | Verkaufspreis in 1/128 des Preises (0 = 100 %) |
| 0x046 | u16 | laufende Nummer (Platz in der Schiffstabelle) |
| 0x048 | u16 | Figurnummer (HANDEL1 = 21, HANDEL2 = 23, KRIEG1 = 25, KRIEG2 = 27, HANDLER = 29, PIRAT = 31) |
| 0x04A | u8 | Figurart: 1 Schiff, 2 freier Händler, 3 Pirat, 5 Händlerkarren an Land (benutzt die Schiffstabelle) |
| 0x04B | u8 | Besitzer (Platz in PLAYER4, 7 = niemand) |
| 0x04C | u8 | Datensatz des freien Händlers in `HANDLER` |
| 0x04D | u8 | **Insel an der Schiffsposition** (`0x0046A570`, z. B. am Kontor), 0xFF = offenes Meer |
| 0x04F | u8 | **aktuelle Station** der Handelsroute |
| 0x050 | u8 | **Richtung 0–7** (Byte 3 der Anzeigefigur, 45°-Schritte) |
| 0x052 + 0x24·s | | **Handelsroute**, 4 Stationen: u32 Verweis, dann 3 × (u16 Ware als Id des Erzeugers − 20000, u16 Menge in 1/32 t, u8 Bit 0 abliefern), Rest frei |
| 0x174 | 8 × 8 Byte | **Ladung**: u16 Ware als Id des Erzeugers − 20000 (Soldaten: Ware + 9876), u16 Bits 0–13 Menge in 1/32 t, u32 Bits 0–9 **Durchschnittspreis** je Tonne; leerer Platz = Ware 0 |

`HANDLER` (0x25C Byte je Händlerschiff): Byte 0 belegt, Bytes 1–2 Platz des Schiffs, Byte 3 Nummer des
Datensatzes; ab +4 50 Einträge zu 12 Byte (u32 Bits 0–9 Durchschnittspreis, Bits 10–14 Alter in
Besuchen; u32 Menge in 1/32 t; u16 Ware als Id des Erzeugers − 20000), belegt sind die ersten 25 in der
Reihenfolge der Waren.

> Korrektur: Eine frühere Fassung hielt Byte 0x04D für die Richtung. Es ist die Insel, an der das
> Schiff liegt. Die Richtung steht in 0x050. Aufgefallen ist das am weitergespielten Spielstand, in dem
> fast alle Schiffe auf See 0xFF hatten.

## Darstellung der Schiffe

Nach der Figurenzeichenroutine `0x0044FA90` (Fall 1–3 = Schiffe):

- **Sprite** = `Gfx + ((Richtung + 2 · Ansichtsrotation) & 7) · Rotate` (`0x0044FA60`; Schiffe haben `Rotate: 1`).
- **Position:** Figurpunkt → Bildschirm wie bei den Feldern (`0x0044D940`:
  x = (fx − fy) · Breite/2, y = (fx + fy) · (Höhe+1)/2). Wie jede Figur liegt das Schiff bei
  Y = y + `Posoffs[1]` · Breite/64 (`0x00451CC5`, [figures.md](../re/figures.md#zeichnen)). Das Sprite
  wird waagrecht zentriert, seine Unterkante liegt bei Y + `Posoffs[0]` · Breite/64 (Feld +0x0C der
  Figurdefinition); bei Handelsschiffen (−19, 34) also 15 Pixel unter dem Figurpunkt.
- **Flagge:** Figur aus der Farbtabelle, Bild `Gfx + Animationsphase (0–7)`. Ihr Abstand vom
  Figurpunkt ist `Fahnoffs[1]` entlang der Schiffsachse, gedreht um (Richtung + 2) · 45°
  (`0x00443180`), projiziert wie oben, ausgehend von Y. Gehoben wird sie um `Fahnoffs[2]` · Breite/2 (`0x00451220`).
  Die Flagge hängt von diesem Punkt nach unten.
- **Reihenfolge:** Figuren werden zwischen den beiden Durchgängen einer Bildschirmzeile gezeichnet
  (`0x00489A60`), sortiert nach Y. Weil die Rumpfunterkante darüber liegt, überdecken flache
  Meeresfelder den Rumpf nie.

Offene Punkte: Animationsphase der Flagge (Byte +0x0B der
Laufzeitfigur; Open1602 nimmt 90 ms je Bild wie `FAHNEMARKT`), Bugwellen (`BUGH`). Der Spielstand
speichert nur ganze Felder; die Nachkommastellen fahrender Schiffe gehen beim Speichern verloren.

## Spielzustand im Spielstand

Alle Chunks eines Spielstands und ihre Speicherroutinen. Die Laufzeittabellen haben feste Größen;
gespeichert werden nur belegte Einträge (erstes Byte ≠ 0xFF bzw. Besitzer ≠ 7). Die Zahl in Klammern
ist die Anzahl in Merlin01 nach einigen Minuten Spielzeit.

| Chunk | Datensatz | Schreiber | Laufzeittabelle | Inhalt |
|---|---|---|---|---|
| `INSEL5`, `INSELHAUS` | [scp.md](scp.md) | `0x00468740` | Inseln `0x005E6B20` (je 0x2C00) | Inselkopf, zwei Feldebenen |
| `HIRSCH2` | 8 | `0x00468740` | Insel + 0x6C, 8 je Insel | Hirsche: Inselnummer, x, y, –, u32 Zeitgeber in ms (153) |
| `PLAYER4` | 0x430 | `0x00478363` | – | Spieler (oben) |
| `SHIP4` | 0x1B4 | `0x004600C0` | `0x004CF358`, 150 × 0x218 | Schiffe (oben, 14) |
| `HANDLER` | 0x25C | `0x004600C0` | `0x004A0DC8`, 30 × 0xD0 | Waren des freien Händlers je Schiff: Preis, Alter, Menge (oben, 7) |
| `SOLDAT3` | 0x44 | `0x004600C0` | `0x0051C678`, 400 × 0x1B0 | Soldaten: Position in halben Feldern, Lebenspunkte, Figur, Platz, Heimat und Ziel, Besitzer, Animation, Planungsradius, Richtung, Streife ([figures.md](../re/figures.md#soldaten-im-spielstand), 9) |
| `TURM` | 0x24 | `0x004600C0` | `0x004EDE60`, 350 × 0x118 | Kanonen der Wachtürme: +0 Weltfeld x/y, +4 Lebenspunkte (nie verringert), +6 Figur (38/40), +8 Platz, +0x0A Verweis auf das Turmgebäude (0x33), +0x17 Insel, +0x18 Besitzer, +0x1B Richtung (Merlin01: 61 Piratentürme, Besitzer 5) |
| `STADT4` | 0xA8 | `0x00485020` | Städte `0x005DBAE0`, 75 × 0x258 | Städte (unten, 20) |
| `KONTOR2` | 0x3EC | `0x00485020` | dieselbe Städtetabelle | Warenlager einer Stadt (unten, 20) |
| `MARKT2` | 0x104 | `0x00485020` | dieselbe Städtetabelle | Verbrauch einer Stadt: Insel, Stadt; 11 Einträge à 16 Byte: Bedarf A, Versorgung B (u32, 1/8192 t), Ware als Id − 20000, Versorgungsgrad C ([consumption.md](../re/consumption.md)) (20) |
| `PRODLIST2` | 0x18 | `0x00485530` | `0x0054A3B8`, 4144 × 0x14 | Produktionsbetriebe und Kontore am Ursprungsfeld: Zustand, Lager, Fortschritt (unten, 126) |
| `WERFT` | 0x18 | `0x00485530` | `0x005A6C18`, 160 × 0x12 | Werften: Insel, x, y, Bit 27 eingeschaltet; +4 Sekunden bis zum Taktende, +6 Auftrag (Schiffsart 0–3, 0xFF keiner), +8 Baupunkte, +0x0A Holz, +0x0C Schiff in Reparatur (0xFFFF keins) ([ships.md](../re/ships.md#werft)) (1) |
| `MILITAR` | 0x70 | `0x00485530` | `0x0055EB80`, 288 × 0x30 | Burgen und Kriegerhütten (Lader `0x00483C60`): Insel, x, y, Epoche (+3 Bits 0–2), +4 Sekunden bis zum nächsten Ausbildungsschritt, +8/+0x0A/+0x0C Waffen im Lager (Schwerter, Musketen, Kanonen, 1/32 t), ab +0x10 neun Plätze zu 8 Byte: Figur (Bits 0–11), in Ausbildung (Bit 12), bewaffnet (Bit 13); Lebenspunkte (Bits 0–11 des zweiten Worts), Fortschritt 0–320 (Bits 16–31) (3: die Hütten der Eingeborenen mit je acht fertigen Speerträgern) |
| `SIEDLER` | 0x10 | `0x00485530` | `0x005A77E8`, 4160 × 10 | Wohnhäuser: Insel, x, y, Epochen, Klasse, Marktentfernung, Einwohner · 64, Zeitgruppe, Markt- und Dienstbits ([houses.md](../re/houses.md)) (132) |
| `ROHWACHS2` | 8 | `0x00485C20` | `0x00562DC8`, 32860 × 8 | Wachstum von Feldern, Bäumen, Plantagen, Fischgründen: Insel, x, y, Klasse, gesehener Zählerstand, Stufe, 2 Byte 0 ([growth.md](../re/growth.md)) |
| `TIMERS` | 0x298 | `0x00485D10` | verstreute Zähler (`0x005A77E0`, `0x005DBAB0`, …) | globale Zähler und Zeitgeber; +0x00: Phase des Stadttakts (10 s), +0x01: Phase des Inseltakts (30 s), +0x02–0x04: Sekundenphasen (Werft, Militär, Produktion), +0x84/+0x88/+0x8C/+0x90/+0x94: deren Zeitspeicher in ms; +0x64: 32 Zähler der Wachstumsklassen (0–7), +0x218: 32 Klassenuhren in ms, +0x44: 16 Epochen der Hauszeitgruppen, +0x198: 16 Hauszeitgeber in ms, +0xA0: Spielzeit in 1/10 s, +0xA6: Schwierigkeit 0–3 (`0x005B6304`, Dürretabelle), +0xA8: Spieleinstellungen (`0x005B706C`; Bit 2 keine Dürren, Bit 8 keine Vulkane), +0xB0: nächster Stadtname aus `[STAEDTE]` (`0x005B3168`), +0xB4: frühester nächster Dürrewurf in 1/10 s (neues Spiel 54000; eine gespeicherte 0 wird 36000), +0xC4: laufender Vulkanausbruch, +0xC8: letzter (Verweise `{0x33, Insel, x, y}`), +0xCC: frühester nächster Vulkanausbruch in 1/10 s, +0xD0: Krater der Welt, höchstens 20 ([volcanoes.md](../re/volcanoes.md)) |
| `AUFTRAG4` | – | `0x00478558` | – | Aufträge/Ziele des Szenarios (2244 Byte, noch nicht untersucht) |
| `WIFF` | 4 | `0x00485EE0` | u32 `0x005B3160` | Prüfsumme: Summe der 32-Bit-Wörter aller Daten davor (`0x00490730`), wird nie gelesen |

Alle Koordinaten in `STADT4` … `ROHWACHS2` sind Inselfelder; die Insel steht als Inselnummer im ersten
Byte. Die Bitfelder der Datensätze sind in den Schreibroutinen genau beschrieben, ihre Bedeutung ist
erst teilweise bekannt. Open1602 liest die bekannten Felder (`assets::load_game_state`) und behält den
Rest als Rohwerte, damit beim späteren Speichern nichts verloren geht.

### STADT4 – Städte (0xA8 Byte)

Eine Stadt ist die Siedlung eines Spielers auf einer Insel. Eingeborenendörfer und Piratennester sind
ebenfalls Städte.

| Offset | Typ | Inhalt |
|---|---|---|
| 0x00 | u8 | Inselnummer |
| 0x01 | u8 | Stadtnummer auf der Insel (= Attributbits 14–16 der Inselfelder) |
| 0x02 | u8 | Besitzer |
| 0x04 | Bit 0 | Stadt + 0x257 Bit 0 |
| 0x05 | u8 | Stadtrang (Stadt + 0x253) |
| 0x08, 0x0C | u32 | Stadt + 0x1F0, + 0x1F4: geglättete Geldstatistik |
| 0x10 | u32 | Stadt + 0x1FE: Zahl der laufenden Brände und Seuchen, sperrt das Wachstum; der Lader überliest es (S-001, [disasters.md](../re/disasters.md)) |
| 0x18 | 7 × u32 | Stadt + 0x206 … (Eingeborene: 49 je Wert, Spieler: 0) |
| 0x58 | u32 | Stadt + 0x1D4 |
| 0x5C | 5 × u32 | **Einwohner** je Klasse (Stadt + 0x220) |
| 0x78 | 5 × u8 | **Zufriedenheit** je Klasse, 128 = zufrieden (Stadt + 0x248) |
| 0x7F | 5 × u8 | **Steuersatz** je Klasse, 128 = normal (Stadt + 0x24D) |
| 0x86 | u8 | **Versorgungsgrad Nahrung** (Stadt + 0x255) ([consumption.md](../re/consumption.md)) |
| 0x87 | char[] | **Name** der Stadt (z. B. „Lemonhall“, „Nova Semlja“) |

### KONTOR2 – Warenlager einer Stadt (0x3EC Byte)

| Offset | Typ | Inhalt |
|---|---|---|
| 0x00 | u32 | Bits 0–7 Inselnummer, 8–15 **x**, 16–23 **y** des Kontors, 24–27 Stadtnummer |
| 0x2C | 23 × 20 Byte | Waren 2–24 (Eisenerz … Ziegel) in der Reihenfolge der Warennummern |

Ein Warenplatz (20 Byte):

| Offset | Inhalt |
|---|---|
| +0x00 | u32: Bits 0–9 **Verkaufspreis** je Tonne, 10–19 **Kaufpreis**, Bit 20 verkauft, Bit 21 kauft ([ships.md](../re/ships.md#handel-am-kontor)) |
| +0x04 | u16 für Träger reserviert (Stadt + 0x24 + 12 · i; der Lader überliest es) |
| +0x08 | u16 **Untergrenze** beim Verkauf, u16 **Obergrenze** beim Kauf, in 1/32 t (Stadt + 0x26/0x28) |
| +0x0C | u16 **Bestand in 1/32 t**: Lemonhall 817 Nahrung (25,5 t), 400 Stoffe (12,5 t), 32 Werkzeug (1 t), 800 Ziegel (25 t), Holz 0 – im laufenden Spiel bestätigt (25 t, 12 t, 1 t, 25 t, kein Holz; das Kontor zeigt ganze Tonnen) |
| +0x10 | u16 Ware als Id des ersten Erzeugers − 20000 (z. B. 513 → 20513 Ziegelei); daran erkennt das Spiel die Ware wieder |

Belegt ist die Position: Bei allen Siedlerstädten liegt an (x, y) ein Gebäude mit
`HAUS_PRODTYP Kind` = `KONTOR`, und dessen Feld gehört zur Stadt (Datentest). Die Einheit der
Bestände hat der Blick ins Kontor im laufenden Spiel bestätigt. Open1602 rechnet intern ebenfalls
in 1/32 t (`assets::kStockUnitsPerTon`), also ganzzahlig und deterministisch.

### PRODLIST2 – Produktionszustand (0x18 Byte)

Ein Eintrag je Gebäude mit `HAUS_PRODTYP` (Werkstätten, Farmen, Minen, Jäger, Fischer, Markt, Kontor
…), immer am **Ursprungsfeld** des Gebäudes (Datentest: alle Einträge aller Spielstände). Die
Schreibroutine packt den 20-Byte-Laufzeiteintrag; die Zuordnung:

| Offset | Inhalt | Laufzeit (`0x0054A3B8` + 20 · i) |
|---|---|---|
| 0x00, 0x01, 0x02 | Inselnummer, x, y | +0, +1, +2 |
| 0x05 | u16 **Lager** in 1/32 t: fertige Ware, die auf den Träger wartet (Vergleich mit `Maxlager`) | +0x0C |
| 0x08 | u16 | +0x04 (Zähler, wird pro Takt heruntergezählt) |
| 0x0A, 0x0C | u16, u16 | +0x08, +0x0A |
| 0x0F | u8 **arbeitet** (0x80) – hat Rohstoff und produziert | +0x0E |
| 0x10 | u16 **Fortschritt** des laufenden Zyklus, 512 = fertig | +0x10 |
| 0x12 | u16 | +0x12 |
| 0x14 | Bit 0 **eingeschaltet**; Bit 1 → +0x0F Bit 6; Bits 2–5 **Animationsphase**; Bit 6 → +0x0F Bit 4 | +3 Bit 3, +3 Bits 4–7 |
| 0x15 | Bits 0–3 **Zyklen ohne Rohstoff** (bleibt bei 15 stehen; Vergleich mit `Maxnorohst`) | +0x0F Bits 0–3 |

Nicht gespeichert wird Bit 7 von +0x0F (Sonderzustand, Statussymbol ANIM 2). Merlin01: 6 von 18
Betrieben auf Insel 0 arbeiten, Lager in ganzen Tonnen, Fortschritt 0–514. Szenarien beginnen mit
lauter ruhenden Betrieben. Was der Zustand für die Darstellung bedeutet:
[production.md](../re/production.md).

