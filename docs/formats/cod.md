# COD-Skripte

`haeuser.cod` (Gebäude, Gelände, Produktion), `figuren.cod` (Figuren, Einheiten, Schiffe),
`Text.cod` / `Editor.cod` (Texte) sind Klartextdateien, bei denen **jedes Byte negiert** ist:

```
gespeichert = (-klartext) & 0xFF        klartext = (-gespeichert) & 0xFF
```

Die Kodierung ist Windows-1252 mit CRLF-Zeilenenden.

**Beleg im Binary:** `Text_LoadTextCod` (`0x0048F8D0`) liest `%sText.cod` per `ReadFile` und
negiert anschließend jedes Byte:

```
0048F94D  mov  cl, [eax+edi]
0048F950  neg  cl
0048F952  mov  [eax+edi], cl
0048F955  inc  eax
0048F956  cmp  eax, esi
0048F958  jl   0048F94D
```

## Aufbau von haeuser.cod / figuren.cod (erste Beobachtungen)

Die Dateien sind eine kleine Beschreibungssprache. Das Spiel interpretiert sie beim Start
(`Cod_LoadHaeuser` `0x00460750`, `Cod_LoadFiguren` `0x00441210`) – alle Werte für Gebäude,
Produktionsketten, Kosten und Einheiten kommen daraus. Das ist für Open1602 zentral: Inhalte
lassen sich datengetrieben laden, statt sie in Code zu gießen.

Elemente:

| Syntax | Bedeutung |
|---|---|
| `; Kommentar` | Kommentar bis Zeilenende |
| `NAME = Ausdruck` | Konstante, z. B. `IDBODEN = 20101`, `GFXHANG = GFXBODEN+432` |
| `Objekt: TYP` … `EndObj;` | Objektblock (verschachtelbar, z. B. `HAUS` mit `HAUS_PRODTYP`, `HAUS_BAUKOST`) |
| `Feld: Wert[, Wert…]` | Eigenschaft, z. B. `Size: 2, 2`, `Wegspeed: 145, 120, 170, 100` |
| `@Feld: +N` / `@Feld: -N` | Wert relativ zum vorherigen Eintrag (`@Nummer: +1`, `@Gfx: +40`) |
| `ObjFill: N` / `ObjFill: 0,MAX` | Eigenschaften von Eintrag N kopieren / Standardwerte für Bereich |
| `NAME = Nummer` | aktuelle Nummer als Konstante merken (z. B. `HAUSWACHS = Nummer`) |
| `Include: "TOOLS.INC"` | Datei einbinden (in `.GAD`) |

Zahlen können Dezimalbrüche sein (`Einhoffs: 2.0`, `Nahrung: 1.3`).

`haeuser.cod` enthält ca. 500 Einträge vom Typ `HAUS` (6331 Zeilen), `figuren.cod` ca. 3071 Zeilen.

**Offen:** vollständige Grammatik (Ausdrücke, Operatoren, Gültigkeitsbereich von Konstanten),
Bedeutung aller Felder. Die Feldliste mit Häufigkeiten erzeugt
`o1602-tool cod haeuser.cod | grep -oE '^\s*@?[A-Za-z_]+:' | sort | uniq -c`.

## Werkzeuge

```sh
o1602-tool cod haeuser.cod haeuser.txt        # nach UTF-8
python3 tools/python/anno_tool.py cod figuren.cod
```
