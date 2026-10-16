# COD-Skripte

`haeuser.cod` (Gebäude, Gelände, Produktion), `figuren.cod` (Figuren, Einheiten, Schiffe) sind
Programme in einer kleinen Beschreibungssprache. Aus ihnen stammen alle Werte für Gebäude,
Produktionsketten, Kosten, Grafiken und Einheiten. `Text.cod` / `Editor.cod` enthalten Texte in
einem anderen, einfacheren Format (siehe unten).

Status: ✅ vollständig spezifiziert und implementiert (`engine/assets/cod_parser.*`),
getestet gegen `haeuser.cod` und `figuren.cod`.

## 1. Kodierung

Jedes Byte ist negiert gespeichert: `gespeichert = (-klartext) & 0xFF`. Text ist Windows-1252
mit CRLF. Die Entschlüsselung geschieht beim Lesen jedes Zeichens (`Cod_ReadChar` `0x00402ED0`),
wenn der Dateiname auf `.cod` endet (`Path_CheckExtension` `0x00490500` → `Cod_SetDecodeMode(1)`
`0x00402220`). `Text_LoadTextCod` (`0x0048F8D0`) entschlüsselt dagegen die ganze Datei auf einmal.

## 2. Ablauf beim Laden

`Cod_LoadHaeuser` (`0x00460750`) und `Cod_LoadFiguren` (`0x00441210`) arbeiten gleich:

1. Symboltabelle leeren (`Cod_ClearSymbols` `0x00402200`). **Jede Datei beginnt mit leerer Tabelle**,
   Konstanten aus `haeuser.cod` sind in `figuren.cod` nicht sichtbar.
2. Vordefinierte Konstanten eintragen (`Cod_DefineSymbol` `0x004020D0`): Objekttypen (`HAUS=1`,
   `HAUS_PRODTYP=2`, `HAUS_BAUKOST=3`, `BGRUPPE=5`, `BGRUPPE_WARE=6`, `BAUINFRA=7` bzw. `FIGUR=1`,
   `ANIM=2`, `FORMATION=3`), Größen (`MAXHAUS=500`, `MAXFIGUR=119`), Gebäudearten (`BODEN=11`,
   `MEER=19`, …), Waren (`NOWARE`…), Sounds (`WAV_*`), Infrastrukturstufen (`INFRA_*`) u. v. m.
   – 398 Einträge für Häuser, 287 für Figuren. Die vollständigen Listen erzeugt
   `re/scripts/extract_cod_predefines.py` aus der Exe (→ `engine/assets/src/cod_predefined.cpp`).
3. Zeile für Zeile `Cod_ParseLine` (`0x00402260`) aufrufen; das Ergebnis ist das Zielsymbol der Zeile.
4. Die Lader behandeln selbst nur die Strukturschlüsselwörter `Objekt`, `EndObj`, `Nummer`,
   `ObjFill` (Abschnitt 5) und übernehmen bekannte Feldnamen (`Id`, `Gfx`, `Kind`, `Size`, …) in
   ihre Tabellen. Unbekannte Felder werden ignoriert.

## 3. Symbole

Eine Hashtabelle mit **650 Einträgen** (`0x006C30C0`, lineare Sondierung, Namen groß-/klein-
schreibungssensitiv, max. 31 Zeichen). Ein Eintrag (`0x290` Byte):

| Offset | Typ | Inhalt |
|---|---|---|
| `0x000` | `int32` | Anzahl Werte der letzten Zuweisung |
| `0x004` | `char[32]` | Name |
| `0x024` | `uint8[8]` | Typ je Wert: 1 = Zahl, 2 = String |
| `0x030` | `double[8]` | Zahlenwert als Gleitkomma |
| `0x070` | `int32[8]` | Zahlenwert als Ganzzahl |
| `0x090` | `char[8][64]` | Stringwert |

Jedes Symbol hat also **bis zu 8 Werte**. Zahlen werden immer doppelt geführt (Ganzzahl und
Gleitkomma) und getrennt gerechnet: `1.3` ergibt Ganzzahl 1 (abgeschnitten) und Gleitkomma 1.3.
Ein unbekannter Bezeichner wird beim ersten Gebrauch mit dem Wert 0 angelegt – ohne Fehlermeldung.

## 4. Syntax einer Zeile (Cod_ParseLine)

Eine Anweisung endet am Zeilenende (`\n`) oder Dateiende. Zeichenweise:

| Zeichen | Wirkung |
|---|---|
| Buchstabe `A–Z a–z` | Bezeichner `[A-Za-z][A-Za-z0-9_]*`, optional `[n]` (Wertindex). Der **erste** Bezeichner der Zeile ist das Ziel; jeder weitere wird durch den Wert des Symbols ersetzt. |
| Ziffer | Zahl `[0-9]+` oder `[0-9]+.[0-9]*` (dann Gleitkomma) |
| `$` | Hexadezimalzahl, z. B. `$1F` |
| `%` | Binärzahl, z. B. `%0101` |
| `"…"` | String (ohne Escapes, darf Zeilenumbrüche enthalten) |
| `:` oder `=` | Zuweisung beginnt; Operator = Zuweisen |
| `,` | nächster Wert (Index + 1); Operator = Zuweisen |
| `+ - * / \| &` | Operator für die folgenden Werte |
| `@` | relative Zuweisung (siehe unten) |
| `;` | Kommentar bis Zeilenende |
| alles andere | wird ignoriert (Leerzeichen, Tab, CR, Klammern, …) |

Besonderheiten, die der Nachbau exakt übernimmt:

- **Keine Punkt-vor-Strich-Regel**: Werte werden strikt von links nach rechts verknüpft.
  `A = 2+3*4` ergibt 20.
- Ein Operator bleibt aktiv, bis ein anderer kommt (`A: 1 + 2 3` = 6).
- Nach einem Operatorzeichen werden alle Zeichen bis zum nächsten Buchstaben, der nächsten Ziffer
  oder dem nächsten Satzzeichen übersprungen – **auch Zeilenumbrüche**. Ein Operator am Zeilenende
  setzt die Anweisung also in der nächsten Zeile fort.
- `-5` ist keine negative Zahl, sondern „minus 5“: Ohne `@` wird das Ziel vorher auf 0 gesetzt,
  das Ergebnis ist trotzdem −5.
- `|` und `&` rechnen mit den Ganzzahlen; das Gleitkommafeld wird danach aus der Ganzzahl gesetzt.
- Division durch 0 bei Ganzzahlen bringt das Original zum Absturz (der Nachbau meldet sie und
  behält den Wert).
- String plus String verkettet, andere Operatoren auf Strings haben keine Wirkung.

### Auswertung

Am Zeilenende wird das Ziel neu berechnet:

1. Anzahl Werte = Anzahl der Kommas + 1.
2. Ohne `@` wird der Zielwert vorher auf 0 gesetzt; mit `@` wird **auf dem bisherigen Wert des
   Symbols** weitergerechnet. `@Gfx: +40` heißt also „letzter `Gfx`-Wert + 40“, egal in welchem
   Datensatz dieser stand.
3. Ein `,` geht zum nächsten Wertplatz (ohne `@` wird auch dieser auf 0 gesetzt).

## 5. Objekte und Datensätze (Lader)

| Anweisung | Wirkung |
|---|---|
| `Objekt: TYP` | neuer, verschachtelter Block vom Typ `TYP`; `Nummer` wird 0 |
| `EndObj` | Block schließen; `Nummer` erhält wieder den Wert des äußeren Blocks |
| `Nummer: n` / `@Nummer: +1` | wählt den Datensatz n innerhalb des Blocks |
| `ObjFill: q` | kopiert den kompletten Datensatz q (inkl. verschachtelter Blöcke) in den aktuellen |
| `ObjFill: q, n` | kopiert Datensatz q in die n Datensätze ab dem aktuellen (`ObjFill: 0,MAXHAUS` = Standardwerte für alle) |
| `Feld: Werte` | setzt eine Eigenschaft des aktuellen Datensatzes |

Verschachtelte Blöcke (`HAUS_PRODTYP`, `HAUS_BAUKOST` in `HAUS`) gehören zum aktuellen Datensatz des
äußeren Blocks. Außerhalb jedes Blocks gesetzte Felder sind globale Werte (`Nahrung: 1.3`,
`Soldat: …`). Manche Felder sind Listen: `figuren.cod` enthält z. B. eine `Soldat:`-Zeile pro
Einheitentyp; der Nachbau bewahrt deshalb alle Zuweisungen in Reihenfolge (`Object::history`).

`ObjFill` kopiert Datensätze, aber **keine Symbole**. Danach folgende `@Feld`-Zuweisungen beziehen
sich weiterhin auf den zuletzt geschriebenen Symbolwert, nicht auf den kopierten Datensatz.

Beispiel aus `haeuser.cod` (gekürzt):

```
Objekt: HAUS
  @Nummer:    0          ; Standardwerte in Datensatz 0
  Kind:       UNUSED
  Size:       1, 1
  ObjFill:    0,MAXHAUS  ; ... in alle 500 Datensätze kopieren

  @Nummer:    +1
  HAUSWACHS = Nummer     ; Nummer dieses Datensatzes merken
  Id:         IDWEIDE+0
  Gfx:        GFXBODEN+4

  @Nummer:    +1
  ObjFill:    HAUSWACHS  ; Datensatz HAUSWACHS kopieren ...
  @Id:        +1         ; ... und Id/Gfx relativ zum letzten Wert ändern
  @Gfx:       -36
```

## 6. Ergebnis für die Originaldateien

| Datei | Symbole | Datensätze | Auffälligkeiten |
|---|---|---|---|
| `haeuser.cod` | 617 von max. 650 | 500 `HAUS` (496 verschiedene `Id`), 30 `BAUINFRA`, 5 `BGRUPPE` | `WAV_BAECKER`, `FAHNEKONTOR` undefiniert → 0 (siehe [bugs.md](../re/bugs.md)) |
| `figuren.cod` | 371 | 119 `FIGUR`, 3 `FORMATION` | `FIGTYP_KANONTURM`, `FIGTYP_PIRATSHIP` undefiniert → 0 |

Gegenprobe: Alle Feld-IDs der Inselvorlage `NORD/big.scp` (+20000) existieren als `Id` in
`haeuser.cod`; deren `Gfx`-Werte ergeben mit `GFX/STADTFLD.BSH` eine stimmige Insel.

## 7. Text.cod / Editor.cod

Anderes Format: `Text_LoadTextCod` (`0x0048F8D0`) entschlüsselt die Datei komplett und zerlegt sie in
Abschnitte `[NAME]`, gefolgt von je einem Text pro Zeile. **Offen:** Zuordnung der Zeilen zu ihren
Verwendungsstellen.

## Werkzeuge

```sh
o1602-tool cod      haeuser.cod haeuser.txt     # nur entschlüsseln (UTF-8)
o1602-tool cod-dump haeuser.cod --type HAUS     # interpretieren und Datensätze ausgeben
o1602-tool cod-dump figuren.cod                 # Vordefinitionen werden am Dateinamen erkannt
```
