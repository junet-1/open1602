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
    uint16_t id;      // + 20000 = "Id" in haeuser.cod (Hypothese, s. u.)
    uint8_t  x, y;    // Position auf der Insel
    uint32_t bits;    // gepackte Attribute, noch nicht entschlüsselt
};
```

**Hypothese `id + 20000`:** In `NORD/big.scp` sind die häufigsten IDs 2722, 1201, 1204, 1203 –
das entspricht `IDWEIDE+21`, `IDMEER+0`, `IDMEER+3`, `IDMEER+2` aus `haeuser.cod` (`IDMEER = 21201`).
Eine Insel aus überwiegend Grasland mit Flachwasser-Rand ist plausibel. Bestätigung über den
Lader im Binary steht aus.

Für `bits` vermutet die Community (u. a. MDCII) Felder wie Rotation, Animationsschritt, Insel-,
Stadt- und Spielernummer – **ungeprüft**, daher noch nicht implementiert.

## Szenarien und Spielstände

`.SZS` (Einzelspieler), `.SZM` (Mehrspieler) und `.GAM` (Spielstand) nutzen dieselben Chunks.
Pro Insel folgt ein Satz `INSEL5, INSELHAUS, HIRSCH2, PRODLIST2, WERFT, MILITAR, SIEDLER, ROHWACHS2,
STADT4, KONTOR2, MARKT2`, dazu globale Chunks (siehe [chunks.md](chunks.md)).

Beobachtung: In Szenarien enthalten manche `INSELHAUS`-Chunks 0 Einträge oder deutlich weniger als
Breite × Höhe. Vermutlich wird das Grundgelände aus der Vorlage (`.SCP`) geladen und nur
Abweichungen/Gebäude gespeichert. **Offen** – Klärung über den Lade-Code.

## Werkzeuge

```sh
o1602-tool islands NORD/big.scp
o1602-tool islands "Szenes/Das Atoll.szs"
```
