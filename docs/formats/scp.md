# SCP – Insel-Vorlagen

677 Dateien in `NORD/`, `SUED/` (Klimazonen), `NORDNAT/`, `SUEDNAT/` (mit Eingeborenen?) und
`NOKLIMA/`. Dateinamen: `lit` (little), `mit` (middle), `med` (medium), `big`, `lar` (large) plus
laufende Nummer – vom Spiel per `%s%s%s%02d.SCP` zusammengesetzt (String @ `0x0049AE1C`).

Chunks: `INSEL3` oder `INSEL5` (Kopf), danach `INSELHAUS` (Felder), teils `HIRSCH2`.

## Insel-Kopf (INSEL3: 40 Byte, INSEL5: 116 Byte)

Gesicherte Felder (gemeinsam für beide Versionen):

| Offset | Typ | Feld | Beleg |
|---|---|---|---|
| 0 | u8 | Inselnummer | |
| 1 | u8 | Breite (Felder) | alle `INSELHAUS`-x < Breite |
| 2 | u8 | Höhe (Felder) | alle `INSELHAUS`-y < Höhe |
| 3 | u8 | Flags? | bisher immer 0 |
| 4 | u16 | Position x auf der Weltkarte | Werte < 500 |
| 6 | u16 | Position y auf der Weltkarte | Werte < 350 |

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

Kuriosum: `NORD/big.scp` (ohne Nummer) hat Lücken in Form von „???“ – dort fehlen schlicht Einträge.
Die Datei ist offenbar eine Platzhalter-Vorlage; die echten Inseln heißen `big00`–`big66`.


## Szenarien und Spielstände

`.SZS` (Einzelspieler), `.SZM` (Mehrspieler) und `.GAM` (Spielstand) nutzen dieselben Chunks.
Pro Insel folgt ein Satz `INSEL5, INSELHAUS, HIRSCH2, PRODLIST2, WERFT, MILITAR, SIEDLER, ROHWACHS2,
STADT4, KONTOR2, MARKT2`, dazu globale Chunks (siehe [chunks.md](chunks.md)).

In Szenarien enthalten die `INSELHAUS`-Chunks weniger Einträge als Breite × Höhe, weil (a) jedes
Gebäude nur einmal gespeichert ist und (b) **offenes Meer nicht gespeichert wird** – der Renderer
zeichnet es aus der Weltkarte. Die Insel 0 von „Das Atoll“ (70 × 60) ergibt so 1488 Einträge bzw.
1613 belegte Felder und wird mit `o1602-tool render-island` vollständig mit Städten dargestellt.
Inseln mit 0 Einträgen sind vermutlich ungenutzte Plätze. `NORDNAT/`/`SUEDNAT/` enthalten nur
wenige Einträge pro Insel (Eingeborenendörfer zum Einblenden in Vorlagen).

## Werkzeuge

```sh
o1602-tool islands NORD/big.scp
o1602-tool islands "Szenes/Das Atoll.szs"
```
