# 0007 – Erweiterbarkeit, Inhalte und Modding

Status: angenommen · 16.10.2026

## Anforderungen

Vom Projektinhaber festgelegt (16.10.2026). Open1602 soll erlauben:

1. **Neue Inhalte:** Gebäude, Farben, Produktionsketten, Inseln, Landschaften, Fraktionen,
   Animationen und Grafiken, Musik und Sounds.
2. **Eigene Kampagnen und Szenarien.**
3. **Neue Inseltypen und Inselgrößen**, zum Beispiel riesige Inseln.
4. **Einen flexibleren, abwechslungsreicheren Inselgenerator.**
5. **Keine feste Spielerzahl.** Statt vier Spielern plus Händler, Piraten und Eingeborenen soll
   man zum Beispiel auch zu fünft im Mehrspielermodus spielen können. Das Spiel skaliert dann mit,
   etwa bei Piraten und fahrenden Händlern.
6. **Erweiterbare Farben und Flaggen** mit einem Flaggenauswahlmenü wie in Anno 1503.

Diese Anforderungen gelten für jeden neuen Code, auch wenn das betreffende Feature erst später kommt.
Feste Zahlen des Originals dürfen nur in den Ladern und Speicherern der Originalformate vorkommen.

## Entscheidung

### 1. Zwei Schichten: Originalformat und Engine-Modell

| Schicht | Module | Inhalt |
|---|---|---|
| Originalformat | `assets` | Liest die Dateien des Originals so, wie sie sind: 7 Spielerplätze, Farben 0–6, Besitzer 7 = niemand, Welt 500 × 350, höchstens 255 Inseln, `.cod`-Nummern |
| Engine-Modell | `sim`, `render`, `ui`, `net`, `script` | Kennt keine dieser Grenzen. Spieler, Banner (Farbe und Flagge), Gebäude, Waren und Figuren sind Einträge in Tabellen beliebiger Größe |

Die Übersetzung geschieht ausschließlich beim Laden (z. B. `sim::load_world`) und später beim
Speichern im Originalformat. Originalspielstände bleiben ladbar. Gespeichert wird im eigenen Format.
Ein Export ins Originalformat ist nur möglich, wenn der Zustand hineinpasst, andernfalls gibt es
eine klare Meldung.

### 2. Inhalte als Pakete mit stabilen Kennungen

- **Basisspiel = ein Inhaltspaket** (`anno1602`). Es wird zur Laufzeit aus der Originalinstallation
  erzeugt (`.cod`, `.bsh`, `.scp`, Sounds), damit keine Originaldaten verteilt werden (ADR 0002).
- **Mods sind weitere Pakete** (Ordner oder Archiv) mit einem Manifest: Kennung, Version,
  Abhängigkeiten, Ladereihenfolge. Sie enthalten Daten (TOML/JSON), Grafiken (PNG mit
  Atlas-Beschreibung), Audio (OGG/WAV/FLAC) und Skripte (Lua).
- **Kennungen sind Zeichenketten** mit Paketpräfix (`anno1602:kontor1`, `meinmod:leuchtturm`).
  Ein Paket kann Einträge hinzufügen, ändern (Overlay auf einzelne Felder) oder ersetzen.
- **Zur Laufzeit zählen nur Indizes.** Nach dem Laden werden die Tabellen zu flachen Arrays.
  Zeichenketten braucht man nur beim Laden, Speichern und für den Mehrspielerabgleich. Das kostet
  in der Simulation nichts (Performance-Vorgabe).

| Inhaltsart | Quelle im Original | Erweiterung |
|---|---|---|
| Gebäude, Produktion, Waren | `haeuser.cod` (`HAUS`, `HAUS_PRODTYP`, `HAUS_BAUKOST`) | eigene Datensätze und Produktionsketten, eigene Warenliste (keine feste Zahl an Waren) |
| Figuren, Animationen | `figuren.cod`, `*.BSH` | eigene Figuren, Sprites aus PNG-Atlanten, beliebige Bildanzahl |
| Banner (Farbe + Flaggen) | Farben 0–6, Flaggensprites in `SHIP.BSH` | beliebig viele Banner (Abschnitt 4) |
| Fraktionen | fest im Programm: Siedler, Händler, Piraten, Eingeborene | Fraktionen als Daten mit KI-Verhalten und Skalierungsregeln (Abschnitt 3) |
| Klimazonen, Landschaften | `NORD`, `SUED`, `*NAT`, Inselvorlagen | eigene Klimazonen, Boden- und Vegetationssätze |
| Inseln | 677 Vorlagen (`.scp`) in festen Größen | beliebige Größe, eigene Vorlagen, Generatorregeln (Abschnitt 5) |
| Musik, Sounds | `MUSIC8`, `SAMPLES`, `Speech8` | eigene Titel, Sounds pro Gebäude/Figur/Ereignis |
| Szenarien, Kampagnen | `.szs`/`.szm`, fest verdrahtete Ziele | Abschnitt 6 |

### 3. Spieler und Fraktionen

- `sim::PlayerId` ist ein Index in die Spielerliste der Partie (8 Bit). `sim::kNoPlayer` (0xFF) steht für
  „niemand“. Damit sind bis zu 254 Plätze möglich. Die wirkliche Obergrenze setzen Lobby und Spielregeln.
- **Steuerung und Fraktion sind getrennt.** Ein Platz wird von einem Menschen, einer KI oder
  niemandem gesteuert (neutral). Er gehört zu einer Fraktion: Siedler, Händler, Piraten, Eingeborene
  oder eine Fraktion aus einem Mod. Bis Fraktionen Daten werden, bildet `sim::PlayerRole` die Rollen des Originals ab (auch die Computergegner bauen darauf auf).
- **Neutrale Fraktionen skalieren mit der Spielerzahl.** Ihre Daten enthalten Regeln wie „Piraten:
  Schiffe = Grundwert + k · Siedlerspieler“ oder „Händler: eine Route je n Kontore“. So passt sich die
  Welt bei fünf oder acht Spielern an.
- Alles, was pro Spieler gilt (Geld, Lager, Statistiken, Diplomatie, Entdeckung), liegt in Containern,
  die mit der Spielerzahl wachsen. Matrizen wie die Diplomatie haben die Größe *n × n*, nicht 7 × 7.

### 4. Banner: Farben und Flaggen

- Ein **Banner** fasst die Erkennungsmerkmale eines Spielers zusammen: Farbe (Flagge, Minikarte,
  Besitzansicht, Oberfläche), Flaggenart und später die Farbvarianten der Soldaten. Spieler verweisen
  auf ein Banner (`sim::BannerId`), nicht auf eine feste Farbnummer.
- **Spielerflaggen sind eine einzige Flagge, die eingefärbt wird** (Vorschlag des Projektinhabers,
  16.10.2026). Im Flaggenmenü wählt man eine von zwölf Vorgabefarben oder eine beliebige RGB-Farbe wie
  in OpenRA; die Engine färbt die Flagge beim Laden der Grafik ein (`render::TintedFlagSource`, ein Satz
  aus 8 Bildern je Banner und Zoomstufe). Neue Spieler brauchen keine neuen Bilder.
- Die Vorlage folgt einer einfachen Regel: Reines Grau (R = G = B) ist Tuch, Grau 128 ist genau die
  Spielerfarbe, dunklere und hellere Grautöne schattieren. Alles andere bleibt, wie es ist
  (Anleitung für Modder: [docs/modding/flags.md](../modding/flags.md)).
- Das Basisspiel macht die Vorlage aus der weißen Originalflagge. Rot, Blau, Gelb und Weiß werden in
  den aus den Originalbildern ermittelten Farben eingefärbt; wer die Originalbilder möchte, schaltet
  sie ein (`BannerOptions::original_player_flags`, `--original-flags`).
- **Einheiten in Spielerfarbe** (Soldaten, Reiter, Kanoniere) folgen derselben Regel. Weil die vier
  Originalfassungen einzeln gezeichnet und keine Umfärbungen sind (Messung in
  [figures.md](../re/figures.md#soldaten-und-spielerfarben)), behalten Rot, Blau, Gelb und Weiß die
  Originalbilder (`Banner::unit_variant`); jede weitere Farbe färbt eine beim Laden erzeugte Vorlage
  ein (`render::TintedUnitSource`). Soldaten speichern nur ihre Einheitenart, nicht die Farbe.
- Banner mit eigenem Motiv behalten fertige Bilder (`FlagStyle::Sprites`, z. B. der Totenkopf der
  Piraten); Eingeborene und Händler haben keine Flagge (`FlagStyle::None`).
- In einer Partie ist jedes Banner höchstens einmal vergeben. Das Menü warnt vor zu ähnlichen Farben.

### 5. Welt, Inseln und Inselgenerator

- **Die Weltgröße ist eine Eigenschaft der Karte** (`sim::World::width()/height()`). Originalkarten
  haben 500 × 350 Felder.
- **Inseln haben keine Größengrenze** außer dem Speicher. Feldkoordinaten und Inselnummern
  sind 16 Bit breit, das Original nutzt 8 Bit (L-003).
- **Inselgenerator:** deterministisch aus einem Startwert (Mehrspieler, Wiederholbarkeit). Er
  arbeitet mit Höhen- und Feuchtekarten aus Rauschfunktionen und erzeugt daraus Küsten, Berge,
  Flüsse, Wälder und Rohstoffe. Die Regeln (Klimazone, Größe, Form, Häufigkeiten) sind Daten.
  Die Originalvorlagen (`.scp`) bleiben als eine Quelle unter mehreren erhalten.
- Riesige Inseln und Karten müssen flüssig laufen. Der Renderer zeichnet nur den sichtbaren
  Ausschnitt (das ist schon umgesetzt), die Simulation arbeitet mit Regionen statt mit
  Durchläufen über die ganze Karte.

### 6. Szenarien und Kampagnen

- **Szenario** = Karte (Vorlage oder Generator mit Startwert) + Startbedingungen (Spieler, Banner,
  Waren, Gebäude) + Ziele + optional ein Skript.
- **Kampagne** = Folge von Szenarien mit Übergängen (Bedingungen, übernommene Werte, Texte, Videos).
- **Skripte** sind Lua mit einer deterministischen, abgeschotteten Schnittstelle: kein Dateizugriff,
  keine Uhrzeit, Zufall nur aus dem Spiel-RNG. Ereignisse (Tick, Bau, Handel, Kampf) lösen sie aus,
  und Änderungen laufen als `Command` (ADR 0004). Der Skriptzustand gehört zum Spielstand.
- Die Szenarien des Originals werden geladen, ihre fest verdrahteten Ziele werden in dieses Modell übersetzt.

### 7. Mehrspieler und Spielstände

- Die Liste der Inhaltspakete mit Versionen und Prüfsummen ist Teil von Spielstand und Lobby.
  Alle Teilnehmer brauchen dieselben Inhalte, sonst wird die Partie nicht gestartet.
- Spielstände speichern Kennungen (Zeichenketten), keine Laufzeitindizes. So bleiben sie gültig,
  wenn ein Paket Einträge ergänzt.

## Folgen

- Einige Stellen brauchen am Anfang mehr Aufwand (Tabellen statt Konstanten). Dafür ändert ein Mod
  Daten statt Code.
- Neue Features werden zuerst als Daten und Tabellen entworfen. Feste Zahlen im Engine-Code müssen
  begründet werden (Kommentar mit Verweis auf diese ADR).

## Feste Grenzen im Code und ihr Abbau

| Stelle | Grenze | Stand |
|---|---|---|
| `sim::MapCell::owner`, `sim::Ship::owner` | Besitzer 0–6, 7 = niemand | ✅ `sim::PlayerId`, `kNoPlayer`; Übersetzung beim Laden |
| `sim::Player` | Farbe als `assets::PlayerColor` (0–6), Art als `assets::PlayerKind` | ✅ `BannerId` und `PlayerRole` |
| `render::player_rgb`, `FigureTable::flag_for_color` im Renderer | 7 feste Farben, 5 Flaggen | ✅ `render::BannerTable` mit eingefärbter Einheitsflagge, beliebige Farben |
| Überlagerungsfarbe `DrawCommand::tint` | Index in die 7 Farben | ✅ RGBA direkt im Befehl |
| `sim::World::kWidth/kHeight`, `Cell` mit 8-Bit-Inselnummer und Koordinaten | 500 × 350, 255 Inseln, Inseln bis 255 × 255 | ✅ Größe pro Welt, 16 Bit |
| Gebäude, Figuren | `.cod`-Nummern als Index | ⬜ Inhaltsregister mit Kennungen (mit den ersten eigenen Inhalten) |
| Waren | feste Liste des Originals | ⬜ mit der Wirtschaftssimulation |
| Fraktionen, Skalierung neutraler Spieler | – | ⬜ `sim::PlayerRole` ist weiter ein festes enum; die Computergegner (18.10.2026) bauen darauf auf, Fraktionen als Daten stehen noch aus |
| Spielerzahl beim Spielstart, Speichern | 4 Spieler beim Start, 7 Spieler, 150 Schiffe, 400 Soldaten, 350 Türme im Spielstand | ⬜ der Weltplan kann 16 Spieler; Speichern über die Originalgrenzen kommt in den eigenen Chunk ([status.md](../status.md)) |
| Inselgenerator | – | 🟡 Stufe 1: eigene Welten aus den Originalvorlagen (200–2000 Felder, [worldgen.md](../re/worldgen.md)); Stufe 2 mit eigenen Inselformen zurückgestellt |
| Lua, Kampagnen, Inhaltspakete | – | ⬜ eigene Meilensteine (M8) |
