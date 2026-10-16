# Anno 1602 – Open-Source-Engine: Projektplan

Stand: 16.10.2026 · Grundlage: GOG-Installation „Anno 1602 – Königsedition“ (de-DE)

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
| M5 | Vollständiges Spiel | KI, Militär, Piraten, Ereignisse, Szenarien/Missionsziele (bereits im erweiterbaren Modell: Fraktionen und Skalierung mit der Spielerzahl, ADR 0007) |
| M6 | Multiplayer | Lockstep, Lobby mit beliebiger Spielerzahl und Flaggenwahl, Desync-Erkennung, Reconnect |
| M7 | Verbesserungen | Bugfixes, beliebige Auflösungen, freier Zoom, Hotkeys, UI-Skalierung, Editor |
| M8 | Inhalte und Modding | Inhaltspakete mit Manifest, Inhaltsregister mit Kennungen, PNG/OGG-Import, Lua-Skripte, Kampagnen, neuer Inselgenerator (ADR 0007) |

## 7. Nächste Schritte

1. **Entscheidungen**: Lizenz (Vorschlag GPL-3.0 wie OpenRCT2/OpenTTD oder MIT), Sprache bestätigen,
   Git-Remote (GitHub?).
2. **Toolchain installieren**: Node ≥ 22.19 (vorhanden: 22.23), `npx rea-agents setup`, Ghidra 12.1.x + JDK 21,
   Wine (32 Bit), CMake, Compiler, SDL3 via vcpkg.
3. **Machbarkeitstest**: rea + Ghidra auf `1602.exe` → Funktionsliste, Dekompilat einer bekannten Funktion
   (z. B. GAD-Ladefunktion über String-Querverweis).
4. **Repository-Gerüst** anlegen (Struktur aus Abschnitt 5, CMake, CI).
5. **Erste Formate**: `.cod`-Decoder (bereits verstanden), COL-Palette, BSH → PNG.
6. **Funktionsinventar** und Subsystem-Zuordnung von `1602.exe` starten.

## 8. Entscheidungen und Fortschritt

**Entscheidungen (16.10.2026):** GPL-3.0-or-later · C++20 · Neuimplementierung statt Dekompilat ·
Projektname *Open1602* (Namensraum `o1602`) · Details in [docs/architecture](architecture/README.md).

| Schritt | Stand |
|---|---|
| M0: Git-Repository, CMake-Presets, CI (Linux/Windows/macOS, ASan), ADRs | ✅ erledigt |
| M0: rea + Ghidra Machbarkeitstest | ✅ bestanden (rea 6.5.0, Ghidra 12.1.2) – `re/notes/rea-smoke-test-result.md` |
| M1: Chunk-Container, `.cod`, Palette, BSH-Sprites | ✅ dokumentiert, implementiert, gegen Originaldaten getestet |
| M1: Inseln (`.SCP`) | 🟡 Kopf (Größe, Position) und Feldliste; Attribut-Bits offen |
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
| M3: Jäger und Hirsche (Tier-KI, Inselereignisse: Dürre, Fruchtbarkeit) | ⬜ |
| M3: Verbrauch der Einwohner aus dem Lager (Stadttakt `0x0047F8A0`, `MARKT2`) | ⬜ als Nächstes |
| M2: Gaukler (Einwohnerzahlen), Träger (Wirtschaft) | ⬜ mit der Wirtschaftssimulation |
| Fehlerkatalog der GOG-Ausgabe (51 Einträge), ADR 0006, `sim::Rules`, CI-Prüfung der Verweise | ✅ `docs/re/bugs.md` |
| M1: ZEI-Schriften, GAD, restliche Szenario-Chunks | ⬜ |
| RE: erste Symbole (`WinMain`, `Game_Init`, `.cod`-Lader, `Text.cod`-Entschlüsselung) | ✅ `re/symbols/` |
