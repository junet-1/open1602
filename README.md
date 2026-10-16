# Open1602

**Open1602** ist eine quelloffene Neuimplementierung der Engine von *Anno 1602* (1998, Max Design / Sunflowers).
Ziel ist ein originalgetreues Spiel auf modernen Systemen (Linux, Windows, macOS) – mit behobenen
Originalfehlern, stabilem Multiplayer und Raum für neue Features, Komfortfunktionen und Mods.

*Open1602 is an open-source reimplementation of the Anno 1602 engine. It requires the original game data.*

> **Status:** frühe Phase – Reverse Engineering und Dateiformate (Meilenstein M1).
> Noch nicht spielbar.

## Originaldaten

Open1602 enthält **keine** Spieldateien. Du brauchst eine eigene Kopie von *Anno 1602*
(z. B. GOG „Königsedition“ oder History Edition). Den Installationspfad übergibst du per
Umgebungsvariable:

```sh
export ANNO1602_DIR="$HOME/Games/Heroic/Anno 1602"
```

## Bauen

Voraussetzungen: CMake ≥ 3.25, Ninja, ein C++20-Compiler (GCC 11+, Clang 14+, MSVC 2022), optional zlib.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Einrichtung unter Arch Linux inkl. Reverse-Engineering-Werkzeugen: [docs/setup-arch.md](docs/setup-arch.md).

## Werkzeuge

```sh
o1602-tool chunks     <datei>                        # Chunk-Struktur jeder Binärdatei
o1602-tool cod        haeuser.cod haeuser.txt         # .cod-Skript entschlüsseln
o1602-tool bsh-export GFX/SHIP.BSH out/ --sheet       # Sprites als PNG
o1602-tool islands    NORD/big.scp                    # Inseldaten
```

## Projektstruktur

| Ordner | Inhalt |
|---|---|
| `engine/` | Engine-Module (`core`, `assets`, später `sim`, `render`, `audio`, `ui`, `net`, `app`) |
| `tools/` | `o1602-tool` (C++) und Python-Prototypen |
| `tests/` | Unit-Tests und Tests gegen Originaldaten |
| `docs/` | [Plan](docs/00-plan.md), [Dateiformate](docs/formats/README.md), [Reverse Engineering](docs/re/README.md), [Architektur](docs/architecture/README.md) |
| `re/` | Symboltabellen, Analyse-Skripte, Notizen |

## Mitmachen

Siehe [CONTRIBUTING.md](CONTRIBUTING.md). Wichtigste Regeln: keine Originaldateien und kein
dekompilierter Code im Repository; jede Erkenntnis über das Original wird dokumentiert und belegt.

## Lizenz

GPL-3.0-or-later, siehe [LICENSE](LICENSE). *Anno* ist eine Marke von Ubisoft; dieses Projekt steht
in keiner Verbindung zu Ubisoft, Max Design oder Sunflowers.
