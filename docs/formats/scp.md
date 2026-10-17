# SCP – Insel-Vorlagen

677 Dateien in `NORD/`, `SUED/` (Klimazonen), `NORDNAT/`, `SUEDNAT/` (mit Eingeborenen?) und
`NOKLIMA/`. Dateinamen: `lit` (little), `mit` (middle), `med` (medium), `big`, `lar` (large) plus
laufende Nummer – vom Spiel per `%s%s%s%02d.SCP` zusammengesetzt (String @ `0x0049AE1C`).

Chunks: `INSEL3`, `INSEL4` oder `INSEL5` (Kopf; 73 / 66 / 538 der Vorlagen), danach `INSELHAUS` (Felder), teils `HIRSCH2`.

## Insel-Kopf (INSEL3: 40 Byte, INSEL4/INSEL5: 116 Byte)

Gesicherte Felder (gemeinsam für beide Versionen):

| Offset | Typ | Feld | Beleg |
|---|---|---|---|
| 0 | u8 | Inselnummer | |
| 1 | u8 | Breite (Felder) | alle `INSELHAUS`-x < Breite |
| 2 | u8 | Höhe (Felder) | alle `INSELHAUS`-y < Höhe |
| 3 | u8 | Flags? | bisher immer 0 |
| 4 | u16 | Position x auf der Weltkarte | Werte < 500 |
| 6 | u16 | Position y auf der Weltkarte | Werte < 350 |

Zusätzlich bei `INSEL4`/`INSEL5` (116 Byte) – Lagerstätten, Fruchtbarkeit und der Verweis auf die
Inselvorlage:

| Offset | Typ | Feld | Beleg |
|---|---|---|---|
| `0x03` | u8 | Bit 0: Fluch der Eingeborenen, wird im nächsten Inseltakt zur Dürre (Insel +0x44 Bit 5); Bit 2: ein Vulkan der Insel ist ausgebrochen (+0x44 Bit 6) | `0x0046B3E0`, [growth.md](../re/growth.md#dürre) |
| `0x17` | u8 | Anzahl der Vulkankrater (Liste bei `0x3C`, 8 Byte je Krater: Byte 1 x, Byte 2 y = Ursprung des Bergs 21108, Byte 6–7 verbleibende Würfe; höchstens 2) | Schreiber `0x00468740`, [volcanoes.md](../re/volcanoes.md) |
| `0x08` | u16 | Inseltakte seit der letzten Wildzählung, 0–3 (Insel +0x68) | `0x0046B3E0`, [deer.md](../re/deer.md) |
| `0x0A` | u16 | Phase des Inseltakts, in der die Insel zuletzt dran war (Insel +0x64); weicht sie von `TIMERS` +0x01 ab, ist die Insel sofort dran | [newgame.md](../re/newgame.md#abgleich-mit-dem-original) |
| `0x1A` | u8 | Anzahl Lagerstätten (höchstens 2) | Schreiber `0x00468740` |
| `0x1B` | u8 | Insel +0x42: Bit p = Spieler p hat die Anbaumöglichkeiten **erkundet** ([ships.md](../re/ships.md#erkunden)) | `0x0046ABF0` |
| `0x1C` | 2 × 8 | Lagerstätten: Ware, x, y (Ursprung des Bergs), Byte 3 (Bit p = Spieler p kennt die Lagerstätte: erkundet oder vom Computer gebunden), Art (0 große, 1 kleine Ader), 0, u16 Menge in 1/32 t ([logistics.md](../re/logistics.md#bergwerke-und-lagerstätten)) | `0x0046C010` |
| `0x5C` | u32 | Fruchtbarkeit: Bit (Ware − 45) für Getreide, Tabak, …, Gras, Baum, Fische | `0x0046AFF0` |
| `0x68` | u32 | Bit p: Spieler p hat die Insel entdeckt (Insel +0x43) | [ships.md](../re/ships.md) |
| `0x66` | u8 | Stärke einer laufenden Dürre (Insel +0x45): 0 keine, 0x40 Zufall, 0x26 Fluch | `0x0046B3E0` |
| `0x6C` | u32 | Ende der Dürre als Spielzeit in 1/10 s (Insel +0x60) | `0x0046A630` |
| `0x60` | u16 | Vorlagennummer (`0xFFFF` = keine) | Lader bei `0x00469BCA`, Pfadbau `0x00469690` |
| `0x62` | u16 | Größenklasse: 0 `lit`, 1 `mit`, 2 `med`, 3 `big`, 4 `lar` | Tabelle `0x0049A890` |
| `0x64` | u8 | Klima: 0 `Nord`, 1 `Sued` | Tabelle `0x0049A8A4` |
| `0x65` | u8 | ≠ 0: Insel ist vollständig gespeichert, keine Vorlage laden | Sprung bei `0x00469BB0` |

Die Weltkarte ist **500 × 350** Felder groß: `Game_Init` übergibt `0x1F4, 0x15E` an eine
Initialisierungsfunktion (`0x004860C0`). Der Rest des Kopfes (Rohstoffe, Fruchtbarkeit, Besitzer …)
ist noch nicht entschlüsselt und wird roh mitgeführt (`IslandHeader::raw`).

## INSELHAUS (8 Byte je Eintrag)

```c
struct IslandTile {
    uint16_t id;      // + 20000 = "Id" eines HAUS-Datensatzes in haeuser.cod (verifiziert)
    uint8_t  x, y;    // Position auf der Insel
    uint32_t bits;    // gepackte Attribute, siehe unten
};
```

| Bits | Inhalt | Beleg |
|---|---|---|
| 0–1 | Rotation 0–3 | `Build_PlaceBuildings` `0x004084D0`, `Haus_GetRotatedSize` `0x00463830` |
| 2–5 | Animationsphase (→ Bits 15–18 im Inselraster) | Übergabe an `0x004653A0` |
| 6–13 | Inselnummer | stimmt mit dem Inselkopf überein (z. B. 19 in `NORD/big.scp`) |
| 14–16 | Stadt (7 = keine) | `0x004084D0` setzt bei fehlender Stadt `0x1C000` |
| 17–21 | Zufallswert | Übergabe an `0x00481450` |
| 22–25 | Spieler | Index in die Spielertabelle (`0x005B7680`, Abstand `0x280`) |
| 26–31 | unbekannt | |

**Ein Eintrag pro Gebäude**: Mehrfeldrige Gebäude stehen nur einmal an ihrer Ursprungsposition;
beim Laden werden sie mit dem Platzierungsalgorithmus aus [rendering.md](../re/rendering.md) auf alle
Felder verteilt.

**`id + 20000` = `Id` in `haeuser.cod` – verifiziert:** Alle 23 verschiedenen IDs aus `NORD/big.scp`
existieren als `Id` eines `HAUS`-Datensatzes (Test `data: island tile ids ...`). Zeichnet man jedes
Feld mit dem `Gfx`-Index dieses Datensatzes aus `GFX/STADTFLD.BSH` isometrisch (64 × 31, Versatz
`Posoffs`), entsteht eine stimmige Insel aus Grasland, Wald, Küste und Wasser.

**Bodenebene beim Laden** (`0x00468550`): Jeder Eintrag kommt in die obere Ebene. Hat er keine Stadt
(Stadtbits 7), ist er keine Ruine (Kind 12, 29) und gehört er nicht zum Land einer Stadt
(`0x00480B70`: Straßen, Stege, Felder, Türme, Betriebe, Häuser, Kontore), kommt er zusätzlich in die
Bodenebene (`0x00463E10`, Insel +0xAFC). Spielstände schreiben deshalb zwei Lagen: zuerst die ganze
Bodenebene, alle Einträge mit Stadt und Besitzer 7, dann die Felder der oberen Ebene, die vom Boden
abweichen ([savegame-writer.md](../re/savegame-writer.md)). Open1602 lädt genauso.

Kuriosum: `NORD/big.scp` (ohne Nummer) hat Lücken in Form von „???“ – dort fehlen schlicht Einträge.
Die Datei ist offenbar eine Platzhalter-Vorlage; die echten Inseln heißen `big00`–`big66`.


## Szenarien und Spielstände

`.SZS` (Einzelspieler), `.SZM` (Mehrspieler) und `.GAM` (Spielstand) nutzen dieselben Chunks.
Pro Insel folgt ein Satz `INSEL5, INSELHAUS, HIRSCH2, PRODLIST2, WERFT, MILITAR, SIEDLER, ROHWACHS2,
STADT4, KONTOR2, MARKT2`, dazu globale Chunks (siehe [chunks.md](chunks.md)).

**Inseln in Szenarien und Spielständen speichern nur ihre Abweichungen von einer Inselvorlage.**
Ist im Kopf eine Vorlagennummer gesetzt und das Flag `0x65` = 0, lädt das Original zuerst
`INSELHAUS` aus `<Klima>\<Größe><Nummer:02>.SCP` (z. B. `Nord\big12.SCP`) und legt danach die
Einträge des Szenarios darüber. Beispiel „Das Atoll“, Insel 0: 1488 eigene Einträge auf der Vorlage
`NORD/big12.scp` (4200 Felder). Alle 34 Inseln von `Das Atoll.szs` und alle 31 von `.szm` lassen
sich so lückenlos aufbauen (Test `data: islands of a scenario ...`).

Auch die Dateien in `NORDNAT/`/`SUEDNAT/` nutzen das: Sie enthalten nur ein Eingeborenendorf und
verweisen auf die passende Basisinsel (z. B. `NORDNAT/BIG01.SCP` → `NORD/big01.scp`).
Inseln mit 0 Einträgen sind unveränderte Vorlagen.

> Korrektur: Eine frühere Fassung dieses Dokuments meinte, Szenarien enthielten das vollständige
> Gelände außer dem Meer. Das war falsch – die Darstellung zeigte abgeschnittene Inselteile.

Spielstände (`.GAM`) haben dasselbe Format wie Szenarien; Spieler und Schiffe stehen in
[gam.md](gam.md).

Die Weltkarte (500 × 350) entsteht, indem jede so aufgebaute Insel an `pos_x/pos_y` ihres Kopfs
gelegt wird (`sim::load_world`, Vorlagen werden zwischengespeichert, da sich viele Inseln eine teilen).
Alles außerhalb der Inseln ist offenes Meer, siehe [rendering.md](../re/rendering.md#5-weltkarte-offenes-meer-und-animationstakt).

## Werkzeuge

```sh
o1602-tool islands NORD/big.scp
o1602-tool islands "Szenes/Das Atoll.szs"
o1602-tool render-world "Szenes/Das Atoll.szs" welt.png
```
