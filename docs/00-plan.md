# Anno 1602 – Open-Source-Engine: Projektplan

Stand: 18.10.2026 · Grundlage: GOG-Installation „Anno 1602 – Königsedition“ (de-DE)

## 1. Ziel

Eine eigenständige, plattformübergreifende Engine, die mit den **Originaldaten** einer legal erworbenen Kopie
läuft (Ansatz wie OpenRCT2, OpenTTD, Julius/Augustus, OpenMW). Es werden keine Original-Assets verteilt.
Die Engine soll originalgetreu spielbar sein, bekannte Originalbugs beheben und eine saubere Basis für
neue Features, QoL und Mods bieten.

### Anforderungen an Erweiterbarkeit und Modding

Festgelegt am 16.10.2026, verbindlich für jeden neuen Code. Die Architektur dazu steht in
[ADR 0007](architecture/0007-erweiterbarkeit-und-modding.md).

- Neue **Gebäude, Farben, Produktionsketten, Inseln, Landschaften, Fraktionen, Animationen,
  Grafiken, Musik und Sounds** lassen sich hinzufügen.
- Eigene **Kampagnen und Szenarien** lassen sich schreiben.
- Neue **Inseltypen und Inselgrößen** sind möglich, auch riesige Inseln.
- Der **Inselgenerator** ist flexibler und abwechslungsreicher als im Original.
- Die **Spielerzahl** ist nicht auf 4 (+ Händler, Piraten, Eingeborene) festgelegt. Man kann zum
  Beispiel auch zu fünft im Mehrspielermodus spielen, und neutrale Parteien wie Piraten und fahrende
  Händler skalieren mit.
- **Farben und Flaggen** sind erweiterbar, mit einem Flaggenauswahlmenü wie in Anno 1503.
- Das **Endlosspiel** lässt sich genauer einstellen als im Original, wie in späteren Anno-Teilen
  (zum Beispiel die Rückerstattung beim Abriss: keine, teilweise oder voll). Die Einstellungen
  stehen in `sim::Rules` ([ADR 0008](architecture/0008-spieleinstellungen.md)).

## 2. Bestandsaufnahme (erste Analyse des Spielordners)

| Datei / Ordner | Befund | Bedeutung |
|---|---|---|
| `1602.exe` (632 KB) | PE32, Intel 386, MSVC 6 (Linker 6.0), **nicht gepackt**, kein Kopierschutz | Ideal für statische Analyse |
| `.text` 0x94E70 (~595 KB Code) | geschätzt 2.000–3.000 Funktionen | überschaubarer Umfang |
| `.data` endet bei 0x49D000, `.rsrc` erst bei 0x72D000 | **~2,9 MB uninitialisierte Globals (BSS)** | Spielzustand liegt großteils in globalen Arrays → Speicherlayout gut rekonstruierbar |
| Imports `DDRAW`, `Maxsound.dll` (→ DSOUND), `Maxnet.dll` (→ DPLAYX), `smackw32.dll`, `Language.dll` | klare Subsystem-Grenzen | Einstiegspunkte für Grafik, Sound, Netzwerk, Video, Texte |
| `haeuser.cod`, `figuren.cod`, `Text.cod`, `Editor.cod` | Klartext-Skripte, **jedes Byte negiert** (`b = -b & 0xFF`) | Gebäude, Einheiten, Kosten, Texte sind **datengetrieben** – riesiger Vorteil |
| `GADDATA/*.GAD`, `Gadedit/*.GAD` | Klartext-UI-Definitionen (Gadgets) | UI-Layouts direkt übernehmbar |
| `GFX/`, `MGFX/`, `SGFX/` `*.BSH` | Sprite-Container (`BSH\0`-Header), 3 Zoomstufen | Grafik |
| `TOOLGFX/*.ZEI`, `*.COL` | Schriften (`ZEI\0`), Palette | Fonts, 8-Bit-Palette |
| `NORD/`, `SUED/`, `*NAT/`, `NOKLIMA/` `*.SCP` (677 Dateien) | Insel-Vorlagen, chunk-basiert | Kartengenerierung |
| `Szenes/*.SZS`, `*.SZM` (138) | Szenarien / Multiplayer-Karten | Gleiches Chunkformat wie Savegames (`.GAM`) |
| `VideoSmk/*.SMK` (51) | Smacker-Videos | via libsmacker / FFmpeg |
| `SAMPLES/`, `Speech8/`, `MUSIC8/` | WAV | direkt nutzbar |
| `1602Edit.exe` + `Editor.dll` | Karten-Editor | später, gleiche Codebasis |

Vorhandene Community-Arbeit (Formatwissen, Referenz – Lizenzen vor Übernahme prüfen):
`stwe/MDCII` (C++, Anno-1602-Welt-Rebuild, BSH/SCP-Parser), `stwe/Benno4j` (Java),
ein Python-Grammatik-/Parser-Generator für die `.cod`-Skripte.

## 3. Reverse-Engineering-Methodik

### 3.1 Grundsätze
- **Dokumentieren, nicht kopieren**: Erkenntnisse aus dem Binary werden als Spezifikation (Formate, Strukturen,
  Algorithmen, Formeln) festgehalten. Die Engine wird auf Basis dieser Doku **neu geschrieben** – kein
  Copy-Paste von Dekompilat. Das hält den Code lesbar und rechtlich sauberer.
- **Symbole sind Quellcode**: Funktions-/Variablennamen, Structs und Kommentare werden regelmäßig aus der
  Analyse-Datenbank exportiert und versioniert (`re/symbols/`). rea arbeitet mit einer *ephemeren*
  Ghidra-Datenbank – ohne Export gehen Annotationen verloren.
- **Jede Behauptung mit Beleg**: Adresse, Dekompilat-Ausschnitt, Testfall oder Speicher-Dump.

### 3.2 Werkzeugkette
| Zweck | Werkzeug |
|---|---|
| Agentengestützte Analyse | **rea** (`rea-agents`, MCP + CLI) mit Ghidra-Backend |
| Disassembler/Decompiler | **Ghidra 12.1.x** + JDK 21 (headless + GUI) |
| Original als Referenz ausführen | **Wine** (32-Bit-Prefix) auf Linux, ggf. dgVoodoo/cnc-ddraw für DirectDraw |
| Laufzeitbeobachtung | `winedbg`/x32dbg unter Wine, Speicher-Dumps der BSS-Region, Savegame-Diffs |
| Formatanalyse | Python 3 (+ `construct`/Kaitai Struct), ImHex mit Pattern-Files |
| Statische Schnellchecks | objdump, rizin (optional) |

**Risiko**: Die rea-Doku nennt 32-Bit-PE nicht explizit (Ghidra selbst unterstützt es vollständig).
Erster technischer Schritt ist daher ein Machbarkeitstest „rea öffnet `1602.exe`“. Fallback: Ghidra headless mit
eigenen Skripten, rea für die übrigen Ziele.

### 3.3 Ablauf
1. **Assets zuerst** (schnelle Erfolge, unabhängig vom Binary): `.cod`-Decoder, BSH+COL → PNG, ZEI-Fonts,
   SCP/SZS/GAM-Chunks, GAD-Parser. Ergebnis: Asset-Viewer und statischer Karten-Renderer.
2. **Funktionsinventar** von `1602.exe`: alle Funktionen erfassen, über Imports, Strings
   (`gaddata\*.gad`, `%sSavegame\%s.gam`, …) und Querverweise Subsystemen zuordnen.
3. **Globale Datenstrukturen** rekonstruieren: Karte/Inselfelder, Gebäude-, Figuren-, Schiffs-, Spieler- und
   Lagertabellen. Abgleich mit Savegame-Chunks und Laufzeit-Dumps unter Wine.
4. **Simulation verstehen**, Subsystem für Subsystem: Spielschleife & Tick, Bauen, Produktionsketten,
   Bevölkerung/Bedürfnisse/Steuern, Handel & Routen, Schiffe/Pfadfindung, KI, Militär, Piraten, Ereignisse,
   Missionsziele.
5. **Paritätstests**: Savegame laden → N Ticks im Original (Wine) und in der neuen Engine → Zustand vergleichen.
6. **Bugkatalog** (`docs/re/bugs.md`): Originalfehler mit Ursache dokumentieren; Fix in der Engine,
   abschaltbar per „Original-Verhalten“-Option, damit Parität messbar bleibt.

## 4. Technologie-Vorschlag

| Bereich | Wahl | Begründung |
|---|---|---|
| Engine-Sprache | **C++20** | Original ist MSVC-6-C++ → Strukturen/Klassen lassen sich direkt abbilden; bewährt bei vergleichbaren Projekten; maximale Performance; großes Ökosystem |
| Build | **CMake** + Presets, **vcpkg** (Manifest) | Linux/Windows/macOS aus einem Build |
| Plattform | **SDL3** (Fenster, Input, Audio, Timer) | eine Abstraktion für alle Systeme |
| Rendering | SDL3 GPU-API (Vulkan/Metal/D3D12) mit 2D-Sprite-Batching; Software-Fallback | hohe Auflösungen, Zoom, Skalierung |
| Audio | SDL3 Audio + miniaudio-Mixer, libsmacker für Videos | |
| Netzwerk | Deterministischer **Lockstep** über UDP (**ENet** oder GameNetworkingSockets) | ersetzt DirectPlay; geringe Bandbreite |
| Modding (später) | Lua 5.4 (sol2) + eigenes Datenformat (TOML/JSON) als Overlay zu den `.cod`-Dateien | Erweiterbarkeit ohne Neukompilieren |
| Debug-UI | Dear ImGui | Inspektoren für Sim-Zustand |
| Tests | Catch2 / doctest, Golden-File- und Paritätstests | |
| RE-/Asset-Tools | **Python 3** | schnelles Prototyping von Formatparsern, Ghidra-Skripte |
| CI | GitHub Actions (Linux, Windows, macOS), Sanitizer-Builds (ASan/UBSan) | Stabilität |

Alternative **Rust**: stärkere Speichersicherheit, aber schwächere Abbildung der C++-Originalstrukturen und
kleineres Spieleentwickler-Ökosystem. Empfehlung bleibt C++20 mit strengen Regeln (RAII, keine rohen
Besitzzeiger, Sanitizer in CI).

### Architektur-Prinzipien (Performance, Stabilität, Multiplayer)
- **Simulation strikt getrennt** von Rendering/Plattform: `sim/` kennt weder SDL noch Grafik.
- **Deterministisch**: fester Tick, nur Ganzzahl-/Festkomma-Arithmetik, eigener seedbarer RNG, keine
  Abhängigkeit von Iterationsreihenfolge ungeordneter Container.
- **Command-basiert**: Spielereingaben werden zu serialisierten Befehlen → gleichzeitig Basis für
  Multiplayer-Lockstep, Replays, Undo im Editor und Headless-Tests.
- **Desync-Erkennung**: Zustands-Hash pro Tick im Multiplayer.
- **Speicher**: Pools/Arenen für Entitäten, keine Allokationen im Tick-Hotpath → stabil bei langen Sitzungen.
- **Datengetrieben**: Gebäude/Waren/Einheiten aus `.cod`-Daten, nicht hart codiert.
- **Keine festen Grenzen des Originals im Engine-Modell** (ADR 0007): Spielerzahl, Banner (Farben und
  Flaggen), Weltgröße, Inselgröße und Inselanzahl sind offen. Die Werte des Originals gelten nur beim
  Laden und Speichern der Originalformate.

## 5. Projektstruktur (`Projekt Anno/`)

```
Projekt Anno/
├── README.md, LICENSE, CMakeLists.txt, CMakePresets.json, vcpkg.json
├── docs/
│   ├── 00-plan.md              ← dieses Dokument
│   ├── architecture/           Architekturentscheidungen (ADRs)
│   ├── formats/                bsh.md, col.md, zei.md, cod.md, gad.md, scp.md, szs-gam.md, smk.md
│   └── re/                     subsystems.md, structs.md, game-loop.md, economy.md, ai.md, bugs.md
├── re/                         Reverse-Engineering-Arbeitsbereich
│   ├── symbols/                functions.csv, globals.csv, types.h  (Quelle der Wahrheit, versioniert)
│   ├── scripts/                Ghidra-Skripte, rea-Workflows, Export/Import der Symbole
│   ├── runtime/                Wine-Harness, Speicher-Dumper, Savegame-Diff
│   └── notes/                  Arbeitsnotizen pro Funktion/Subsystem
├── tools/                      Python: cod-decode, bsh2png, scp-dump, gad-parse, save-diff
├── engine/
│   ├── core/                   Logging, Dateisystem, Config, Festkomma, RNG, Speicherpools
│   ├── platform/               SDL3: Fenster, Input, Timing
│   ├── assets/                 Loader für alle Originalformate (eigenständige Bibliothek „libanno“)
│   ├── sim/                    Deterministische Simulation: Welt, Inseln, Gebäude, Wirtschaft,
│   │                           Bevölkerung, Handel, Figuren, Schiffe, KI, Militär, Szenarien
│   ├── render/                 Isometrischer Sprite-Renderer
│   ├── audio/                  Sound, Musik, Sprache, Videos
│   ├── ui/                     GAD-basierte Oberfläche
│   ├── net/                    Lockstep-Multiplayer
│   ├── script/                 Modding (später)
│   └── app/                    main, Spielzustände, Menüs
├── tests/                      unit/, golden/, parity/
├── third_party/
└── .github/workflows/
```

Originaldaten werden **nicht** ins Repository kopiert; die Engine bekommt den Pfad zur Installation
(hier `~/Games/Heroic/Anno 1602`) per Konfiguration.

## 6. Meilensteine

| # | Meilenstein | Ergebnis |
|---|---|---|
| M0 | Setup | Repo, Toolchain, rea+Ghidra-Machbarkeitstest, CI-Grundgerüst |
| M1 | Assets | Alle Formate dokumentiert, `tools/` + `libanno`, Asset-Viewer |
| M2 | Karten-Renderer | Szenario/Savegame laden und statisch isometrisch darstellen (3 Zoomstufen) |
| M3 | Kern-Simulation | Bauen, Produktion, Bevölkerung, Lager, Steuern – Paritätstests gegen Original |
| M4 | Spielbares Endlosspiel | Schiffe, Handel, Figuren, UI, Speichern/Laden (kompatibel zu `.GAM`) |
| M5 | Vollständiges Spiel | Militär, Piraten, Weltengenerator Stufe 1, KI, Ereignisse, Szenarien/Missionsziele (bereits im erweiterbaren Modell: Fraktionen und Skalierung mit der Spielerzahl, ADR 0007) |
| M6 | Multiplayer | Lockstep, Lobby mit beliebiger Spielerzahl und Flaggenwahl, Desync-Erkennung, Reconnect |
| M7 | Verbesserungen | Bugfixes, beliebige Auflösungen, freier Zoom, Hotkeys, UI-Skalierung, Editor |
| M8 | Inhalte und Modding | Inhaltspakete mit Manifest, Inhaltsregister mit Kennungen, PNG/OGG-Import, Lua-Skripte, Kampagnen, eigene Inselformen (Weltengenerator Stufe 2, ADR 0007) |

**Stand der Meilensteine (18.10.2026)**, Einzelheiten in [stand.md](stand.md): M0 ✅ · M1 🟡 (offen:
`.ZEI`-Schriften, `.GAD`-Oberflächen, Smacker-Videos, Teile von Inselkopf und Szenario-Chunks) · M2 ✅ ·
M3 ✅ · M4 🟡 (Simulation, Schiffe, Handel und Spielstände fertig; es fehlt die Oberfläche) · M5 🟡
(Militär, Piraten, Weltengenerator Stufe 1, KI und Aufträge mit Sieg und Niederlage fertig; offen:
Kampagnenfortschritt, Bestenliste, KI-Schalter der Szenarien; Ereignisse haben Szenarien nicht) · M6 ⬜ (Grundlagen: Befehle, Determinismus) · M7 ⬜ (einzelne Verbesserungen
schon da: Mausrad-Zoom, Bildschirmfoto, behobene Fehler) · M8 ⬜ (Banner mit eingefärbten Flaggen schon da).

## 7. Nächste Schritte

**Vereinbarte Reihenfolge (17.10.2026):** erst die Spielinhalte vervollständigen, dann der Mehrspieler.
Jedes neue System bleibt deterministisch und befehlsbasiert, damit der Mehrspieler später ohne Umbau
darauf aufsetzt.

1. **Schiffe** ✅: Fahren mit Wegsuche auf See, Laden und Entladen, Handelsrouten, freier Händler, Werften.
2. **Militär** ✅: Diplomatie (Friedens- und Handelsverträge, Ereignisse, Stimmung, Tribut), Soldaten
   und Kampf, schießende Türme, Seekampf mit Breitseite und Sinken, Gebäudeschaden, Ausbildung in der
   Burg, Einschiffen, Eroberung über ein zerstörtes Kontor, Fluch der Eingeborenen
   ([military.md](re/military.md), [diplomacy.md](re/diplomacy.md)). Burgen schießen im Original nicht,
   und Treffer legen keinen Brand. Offen: Fenster dafür in der Oberfläche (Schritt 5).
3. **Piraten und Eingeborene** ✅: Piratenschiffe nach Spielerstufe und Schwierigkeit, Beutesuche,
   weiße Flagge und Plünderung ins Nest, Verkauf im Nest, Stimmung und Krieg alle 5 s, Frieden kaufen und
   Bestechung; Wohlwollen, Verbrauch und Krieger der Eingeborenen ([pirates.md](re/pirates.md)). Das
   Einheitenlimit ist eine Regel (`Rules::max_army`, Standard 250, Original 100; L-008), ebenso die
   Flottengrenze (`Rules::max_ships_per_player`, Standard 100, Original 33; L-001). Nestgründung und
   die Diplomatie der Computer mit den Piraten kamen mit der KI (Schritt 5).
   **Schifffahrt abgeschlossen** (17.10.2026): Tempo, Diagonalen und Drehen gegen das Video des Originals
   geprüft, Insel erkunden mit Erz- und Goldsymbolen, beschädigte Rümpfe, Bugwelle, weiße Flagge und
   Sinken in der Darstellung, Stadttakt zur vollen 10. Sekunde ([ships.md](re/ships.md)). Offen sind nur
   Fenster der Oberfläche (Schritt 5) und der Mündungsrauch.
4. **Weltengenerator, Stufe 1** ✅: eigene Welten für jede Kartengröße (200–2000 Felder je Seite) und
   Spielerzahl: Inselzahl und Größenmischung nach Fläche, Startgebiete mit Heimatinsel und gleich großen
   Nachbarinseln, Klima nach Lage mit beiden Klimata in Reichweite, Eisen auf jeder Heimatinsel, alle
   Pflanzen und Gold in Reichweite, Dörfer und Vulkaninseln nach Insel- und Spielerzahl; größere Welten im
   eigenen Chunk `O1602MAP` ([worldgen.md](re/worldgen.md#eigene-welten-weltengenerator-stufe-1)). Offen:
   mehr als vier Spieler und Piratenstärke nach Spielerzahl (mit dem Mehrspieler), Startdialog (Schritt 5).
5. **Computergegner (KI)** ✅: Aufgabenstapel, Stufen und Bedarfe wie im Original, Bauplatzsuche mit
   Straßenplan, Wartung, Siedeln und Kolonien, Versorgungsrouten und Streifen, Flotte, Handel, Anwerben,
   Türme und Burgen, Angriffe mit Landung, Inselsoldaten, Diplomatie, Piratennest; deterministisch auf
   jedem Rechner, mit dem Spielstand gespeichert ([ai.md](re/ai.md), S-095 bis S-102). In Probeläufen ohne
   aktiven Menschen erreichen die Computer nach sechs Stunden Kaufleute mit 500–870 Einwohnern. Offen:
   KI-Schalter der Szenarien und die Schiffsaufträge aus Originalständen. Als Nächstes Ereignisse
   (Vulkanausbrüche der Szenarien), Baumenü, Startdialog mit allen Einstellungen, übrige Oberfläche.
   **Nach der Bestandsaufnahme erledigt (18.10.2026):** eigenes Speicherformat `.o1602` ohne die Grenzen
   des Originals ([ADR 0009](architecture/0009-eigenes-speicherformat.md)); teureres Wasser bei feindlichen
   Türmen; Stadt- und Schiffsfenster als Befehle (Steuersatz, Name, Material, Kanonen, über Bord,
   Schiffsverkauf); Handelseinstellungen nach `Maxware`; `Grundflg` in der Wegsuche; Aufträge der
   Computerschiffe aus Originalständen; Reaktion der Computer auf eine Gründung. Danach die
   **Aufträge der Szenarien** ([missions.md](re/missions.md)): alle 18 Ziele und beide
   Verlustbedingungen, Sieg für alle Rechner gleich, Niederlage ohne Stadt und Schiff, Bankrott,
   Computer und Piraten, Triumphbogen für den Schuldigen, Handelsstatistik (S-105 bis S-111).
   Szenarien haben keine Ereignisse oder Skripte (belegt).
6. **Mehrspieler** einschließlich Endlosspiel (`.szm`).
7. **Weltengenerator, Stufe 2** (zurückgestellt, wenn alles andere fertig ist und läuft): eigene Inselformen,
   auch größer als die Vorlagen, mit Küsten-, Strand-, Berg- und Flussübergängen.

Originalformate laden und Standardszenarien im Original weiterspielen zu können ist erwünscht, aber kein
Anspruch: erweiterte Funktionen von Open1602 müssen nicht mit dem Original kompatibel sein.

## 8. Entscheidungen und Fortschritt

**Entscheidungen (16.10.2026):** GPL-3.0-or-later · C++20 · Neuimplementierung statt Dekompilat ·
Projektname *Open1602* (Namensraum `o1602`) · Details in [docs/architecture](architecture/README.md).

| Schritt | Stand |
|---|---|
| M0: Git-Repository, CMake-Presets, CI (Linux/Windows/macOS, ASan), ADRs | ✅ erledigt |
| M0: rea + Ghidra Machbarkeitstest | ✅ bestanden (rea 6.5.0, Ghidra 12.1.2) – `re/notes/rea-smoke-test-result.md` |
| M1: Chunk-Container, `.cod`, Palette, BSH-Sprites | ✅ dokumentiert, implementiert, gegen Originaldaten getestet |
| M1: Inseln (`.SCP`) | 🟡 Kopf (Größe, Position, Klima, Lagerstätten, Schalter) und Feldlisten beider Ebenen; offen: Attributbits 26–31 und Rest des Kopfes (roh mitgeführt) |
| M1: `.cod`-Interpreter (Grammatik aus der Exe, 685 Vordefinitionen, Parser + Tests) | ✅ `docs/formats/cod.md` |
| M1: Gebäudedatensatz (`haeuser.cod` → `g_haeuser`), Inselattribute, Platzierungsalgorithmus | ✅ `docs/re/haus-record.md` |
| M2: Renderer-Grundlage: Sprite-Formel, Maleralgorithmus, Software-Renderer, `render-island` | ✅ `docs/re/rendering.md` |
| M2: Weltkarte (500 × 350, offenes Meer, Animationstakt), Sprite-Atlas, Kamera, `render-world` | ✅ `docs/re/rendering.md` §5 |
| M2: SDL3-Kartenbetrachter `open1602` (Scrollen, Zoom an der Maus, Drehen, HiDPI) | ✅ `docs/setup-arch.md` |
| M2: Spielstände (`.GAM`) im Betrachter, Spieler (PLAYER4), Spielerfarben, Schiffe mit Flaggen, Besitzansicht | ✅ `docs/formats/gam.md` |
| M2: echter Spielstand geprüft (Merlin01): zwei Inselebenen, globale Chunks, Namen aus `Game.dat`, Flaggenwahl bestätigt | ✅ `docs/formats/gam.md` |
| M2: Flaggen auf Kontor, Markt und Turm in Besitzerfarbe | ✅ `docs/re/rendering.md` §6 |
| M2: Abgleich mit einem Bildschirmfoto des Originals (Merlin01, SGFX): Korrelation 0,955 | ✅ `docs/re/rendering.md` §8 |
| Anforderungen an Erweiterbarkeit und Modding (ADR 0007); Engine-Modell ohne feste Spielerzahl, Banner statt fester Farben, Weltgröße pro Karte | ✅ `docs/architecture/0007-erweiterbarkeit-und-modding.md` |
| Spielstand-Analyse (Merlin01 weitergespielt): alle Chunks mit Schreibroutine und Laufzeittabelle; Städte mit Namen, Kontorbestände, Produktionsliste, Hirsche geladen; SHIP4-Richtung korrigiert | ✅ `docs/formats/gam.md` |
| M2: Figuren-Animationssystem (ANIM), Tiere mit Schatten, Figuren-Sheets je Zoomstufe, Figuren in SGFX (abschaltbar) | ✅ `docs/re/figures.md` |
| Spielerflaggen als eingefärbte Einheitsflagge (beliebige Farben, 12 Vorgaben, Modder-Anleitung) | ✅ `docs/modding/flags.md` |
| M2: Produktionszustand aus dem Spielstand: Gebäudeanimationen nach Zustand (Mühlen, Füllstand, Marktstände), Rauch, Statussymbole; Figuren am Bezugspunkt Y wie im Original; Abgleich mit dem Bildschirmfoto jetzt 0,967 | ✅ `docs/re/production.md` |
| Prüfung: Soldaten in beliebigen Spielerfarben (Vorlage aus den vier Fassungen, Originalbilder für die vier Originalfarben) | ✅ `docs/re/figures.md` |
| M2: Soldaten aus dem Spielstand (`SOLDAT3`), in jeder Bannerfarbe (Originalbilder für die vier Originalfarben, sonst eingefärbte Vorlage) | ✅ `docs/re/figures.md` |
| M3: Simulation in festen Ticks (`sim::Simulation`), Produktionszyklus wie im Original (Rohstoff, Rate, Intervall, Lager, Leerlauf, Statistik, Nachschubanforderungen), Viewer lässt die Zeit laufen | ✅ `docs/re/production.md` |
| M3: Logistik, Stufe 1: Lager der Stadt (Bestand, Reservierung, Lagerraum), Wegesuche im Einzugskreis, Träger und Marktkarren mit Bewegung und Animation; Originalfehler S-022–S-025 behoben | ✅ `docs/re/logistics.md` |
| M3: Logistik, Stufe 2: Wachstum der Felder (Klassenuhren, `ROHWACHS2`), Bauern, Viehherden, Fischerboote, Lagerstätten der Minen | ✅ `docs/re/growth.md`, `docs/re/logistics.md` |
| M3: Bauen und Abreißen als Befehle der Simulation: Prüfungen (Gelände, Drehung, Gebiet, Einheiten, Freischaltung, Fruchtbarkeit), Kosten in Auftragsreihenfolge, Anmelden in allen Listen, Straßen an Nachbarn anpassen, Gebiet von Markt und Kontor, Stadtgründung vom Schiff, Abriss mit Gebietsprüfung; Baustufen im Stadttakt; Bauwerkzeug im Betrachter (B, X) | ✅ `docs/re/construction.md` |
| Spieleinstellungen in `sim::Rules` (ADR 0008), erste Wahl: Rückerstattung beim Abriss (keine, teilweise, voll) | ✅ `docs/architecture/0008-spieleinstellungen.md` |
| M3: Dürre (Wurf nach Schwierigkeit, Verdorren, Erholung, Ernte, Einstellung `Rules::droughts`) | ✅ |
| M3: Brände, Pest, Räuber: Stadttermin, Ausbreitung, Niederbrennen, Arzt und Feuerwehr, Räuber mit Verfolgung, Anzeige | ✅ `docs/re/disasters.md` |
| M3: Vulkane: Krater, Wurf, Ausbruch mit 141 Lavabrocken, Brand und Zerstörung, Anzeige, Einstellung `Rules::volcanoes`; S-042 behoben | ✅ `docs/re/volcanoes.md` |
| M4: Spielstände schreiben, die das Original lädt: alle Chunks in der Reihenfolge des Originals, Datensätze aus der Quelldatei mit Open1602s Zustand, WIFF; unverändert geschrieben Byte für Byte die Originaldatei, weitergespielt lädt er zum selben Zustand; F5 / `--save`, nie in die Installation | ✅ `docs/re/savegame-writer.md` |
| M4: Eigener Chunk `O1602` hinter `WIFF`: Einstellungen, Zufall, Uhren, alle Listen, Brände, Lava, Figuren, abweichende Felder; Fingerabdruck der Originalchunks; ein gespeichertes Spiel läuft genau weiter wie ohne Unterbrechung | ✅ `docs/formats/o1602-chunk.md` |
| M4: neues Endlosspiel wie im Original: Weltplan und Inseltabelle (`SZENE`, `RANDTAB`), Vorlagen, Dörfer, Pflanzen, Erz, Schätze, Startzustand nach dem Neustart; `open1602 --new-game D`; Inseln wahlweise aus dem ganzen Startwert; S-019 behoben (Regel), S-067; Prüfung im Original steht aus | ✅ `docs/re/worldgen.md`, `docs/re/newgame.md` |
| M3: Verbrauch der Einwohner aus dem Lager (Stadttakt `0x0047F8A0`): Bedarf aus haeuser.cod, Versorgungsgrade, Zufriedenheit mit Steuerkurve, Meldungen; Einwohner aus `STADT4`, Abrechnung aus `MARKT2` | ✅ `docs/re/consumption.md` |
| M3: Häuser (`SIEDLER`), Versorgung durch Märkte und öffentliche Gebäude, Wachstum, Auf- und Abstieg, Ruinen; Steuern, Betriebskosten, Sold; Zufallsquellen je Teilsystem, Piraten | ✅ `docs/re/houses.md` |
| M3: Hirsche und Jäger: Wildplätze, Zählung, Rückkehr, Grasen, Ruf des Jägers, Schuss, Beute | ✅ `docs/re/deer.md` |
| M3: Jagd geprüft: Zählregel gegen gespeicherte Wildplätze, eingesetzte Jagdhütte jagt mit Originaldaten; Merlin02 → Merlin03 (mit Video) läuft Feld für Feld wie im Original (Zielwahl des Hirschs mit der Wellensuche, Wildplätze, Statistik der Hütte) | ✅ `docs/re/deer.md` |
| M2: Leben im Bild: Spaziergänger, Wale, Delfine, Kraken, Gaukler, Geräuschquellen; nur Darstellung, eigener Zufall, ohne Einfluss aufs Spiel (S-065) | ✅ `docs/re/flavor.md` |
| M2: Zierfiguren auf allen Rechnern gleich ohne Netzverkehr (aus Startwert, Spielzeit und Welt, nicht aus Kamera und Bildrate) | ✅ `docs/re/flavor.md` |
| M2: Landfiguren mit Höhe (0,028 · Posoffs des Entstehungsfelds) und Feldmitte wie im Original, Sortierung nach Y in der Zeile; kein Abschneiden durch Nachbarfelder; Begegnungen ohne Ausweichen wie im Original | ✅ `docs/re/figures.md` |
| M2: Alle Figuren pixelgenau zu den Feldern (Abschneiden wie im Original, eine Zeile Versatz der Feldzeilen ausgeglichen), mit Bildschirmfoto geprüft; Symbol „abgeschaltet“ über Werften | ✅ `docs/re/rendering.md` |
| M4: Schiffe: Fahren von Feldmitte zu Feldmitte mit Drehen im Stand, Tempo nach Schaden und Ladung, Wegsuche auf See im Fenster mit Umwegen um andere Schiffe, Ankunft am Kontor im Radius 5, Entdecken von Inseln; Laden und Entladen am eigenen und an fremden Kontoren mit Preisen und Grenzen, Handel mit Eingeborenen gegen Zuneigung; Handelsrouten mit vier Stationen; freier Händler (Ankunft am Kartenrand, Zielwahl nach Gewinn, Kauf und Verkauf, Altern, Werkzeug aus dem Nichts); Werften mit Packesel, Bau und Reparatur; alles gespeichert (SHIP4, HANDLER, WERFT, KONTOR2, eigener Chunk); Fahrbefehl im Betrachter; S-005, S-013, S-070–S-083 | ✅ `docs/re/ships.md` |
| M2: Arbeiter und Tiere von Feldern und Bäumen verdeckt wie im Original (Holzfäller beim Fällen unsichtbar, Mäher bis zu den Knien im Getreide); Schiff mit Bildschirmfoto geprüft | ✅ `docs/re/figures.md` |
| Fehlerkatalog der GOG-Ausgabe (anfangs 51, jetzt 164 Einträge), ADR 0006, `sim::Rules`, CI-Prüfung der Verweise | ✅ `docs/re/bugs.md` |
| M5: Diplomatie und Militär (Verträge, Soldaten, Kampf, Türme, Burgen, Seekampf, Eroberung) | ✅ `docs/re/military.md`, `docs/re/diplomacy.md` |
| M5: Piraten und Eingeborene, Einheiten- und Flottengrenze als Regeln | ✅ `docs/re/pirates.md` |
| M5: Weltengenerator Stufe 1 (eigene Welten 200–2000 Felder, `O1602MAP`) | ✅ `docs/re/worldgen.md` |
| M5: Computergegner (deterministisch, gespeichert, S-095 bis S-102) | ✅ `docs/re/ai.md` |
| Bestandsaufnahme: was umgesetzt, offen und unklar ist, im Original und bei den Erweiterungen | ✅ `docs/stand.md` |
| M5: Aufträge der Szenarien, Sieg und Niederlage (S-105 bis S-111) | ✅ `docs/re/missions.md` |
| M1: ZEI-Schriften, GAD, restliche Szenario-Chunks | ⬜ |
| RE: erste Symbole (`WinMain`, `Game_Init`, `.cod`-Lader, `Text.cod`-Entschlüsselung) | ✅ `re/symbols/` |

## 9. Offene Punkte nach Abhängigkeit

Die vollständige Übersicht (umgesetzt, offen, Klärung nötig, Erweiterungen) steht in
[stand.md](stand.md); was ich dafür an Spielständen, Videos und Beobachtungen brauche, in
[eingaben.md](eingaben.md). Hier nur die Punkte, die auf ein anderes System warten. Wenn das System
kommt, werden sie abgehakt.

**Speichern**
- ~~Mehr Spieler, Schiffe, Soldaten und Türme, als das Original fasst~~: eigenes Format `.o1602`
  ([o1602-save.md](formats/o1602-save.md)).
- Liste der Spielstände (Abschnitt `META`) im Lade-Menü der Oberfläche.
- Originalspielstände nehmen für die Wahlpunkte die Werte des Originals: erfüllt, weil der Standard
  der Wahlpunkte das Original ist ([ADR 0008](architecture/0008-spieleinstellungen.md)); Stände von
  Open1602 bringen ihre Einstellungen mit.

**Szenarien**
- ~~Missionsziele, Sieg und Niederlage~~: erledigt ([missions.md](re/missions.md)). Ereignisse haben die
  Szenarien des Originals nicht.
- Kampagnenfortschritt (eigene Datei statt `Game.dat`), Bestenliste und Punkte, Endbildschirm mit Meldung
  und Video: mit der Oberfläche.
- KI-Schalter (Spieler +0x70), Szenario-Haltung der Diplomatie (`PLAYER4` +0x1C/+0x20).

**Oberfläche (Baumenü, Fenster, Meldungen)**
- Baumenü des Originals (GAD) mit Gebäudenamen aus Text.cod, Freischaltung als ausgegraute
  Knöpfe ([construction.md](re/construction.md)).
- Fenster für Kontor, Laderäume, Handelsrouten, Werft, Burg, Diplomatie, Einheiten, Statistik;
  Lebensbalken; Formationswahl.
- Warnung beim Abriss eines Markts, der Häuser die Grundlage nimmt (S-035).
- Startdialog für das Endlosspiel mit allen Spieleinstellungen ([ADR 0008](architecture/0008-spieleinstellungen.md)).
- U-013 noch einmal ansehen (Erkunden an Werft und Piratenkontor).
- Idee: volle Rückerstattung für wenige Sekunden nach dem Bau (Schutz vor Fehlklicks); dafür
  braucht jedes Gebäude seinen Bauzeitpunkt.
- Idee: eigenes Verhalten der Straßen-Wegsuche neben dem des Originals.

**Darstellung**
- Mündungs- und Schussrauch, Treffer, Wrack (SHIPSUNK); Werftsymbole „Material fehlt“ und „Brand“;
  Zierfiguren nur auf entdeckten Inseln; Helligkeits- und Farbstich-Tabellen.

**Ton und Videos**
- ~~Geräusche, Musik, Sprache~~: erledigt ([audio.md](re/audio.md)); offen die Arbeitsgeräusche der Figuren und Smacker-Videos.

**Ohne Abhängigkeit**
- ~~Symboltabelle `re/symbols/` ergänzen~~: erledigt (532 Funktionen).
- Erledigt am 18.10.2026: Handelsgrenzen beim Kontor-Ausbau, dringende Waren (S-104), Wasser bei
  feindlichen Türmen (S-103), Stadt- und Schiffsbefehle, Aufträge aus `SHIP4`, Reaktion der KI auf eine
  Gründung, `Grundflg` in der Wegsuche.

**Mehrspieler**
- Wie andere Rechner einen Hausaufstieg übernehmen (Nachricht 0x835, [houses.md](re/houses.md)).
- Zustands-Hash je Tick, Befehle über das Netz, Lobby (ADR 0004); mehr als vier Spieler beim Start.

**Nur Analyse**
- Bits +0x0F/4 und /6 der Produktion ([production.md](re/production.md)).
- Zweite Liste bei Insel +0x30 ([growth.md](re/growth.md)).
- Weitere Punkte in [stand.md](stand.md#13-klärung-nötig-verhalten-des-originals-nicht-belegt).
