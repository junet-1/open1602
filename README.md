# Open1602

**Open1602** is an open-source reimplementation of the engine of *Anno 1602* (1998, Max Design / Sunflowers).
The goal is a faithful game on modern systems (Linux, Windows, macOS), with the original's bugs fixed,
stable multiplayer and room for new features, quality-of-life improvements and mods.

> **Status (October 2026):** The simulation of the original is largely complete (construction, economy,
> population, disasters, ships and trade, military, diplomacy, pirates, computer opponents, new endless
> game, savegames in the original format). The game starts in the original's main menu and is played with
> its side panel; multiplayer and parts of the interface are still missing. What is done, open and
> unclear: [docs/stand.md](docs/stand.md) (German, being translated).

## Original game data

Open1602 contains **no** game files. You need your own copy of *Anno 1602*, for example the GOG
"Königsedition" (*Anno 1602 – Creation of a New World*). Tell Open1602 where the installation is with
the environment variable `ANNO1602_DIR` (or `--game-dir DIR` on the command line):

| Shell | Command |
|---|---|
| bash / zsh | `export ANNO1602_DIR="$HOME/Games/Heroic/Anno 1602"` |
| fish | `set -gx ANNO1602_DIR "$HOME/Games/Heroic/Anno 1602"` |
| PowerShell | `$env:ANNO1602_DIR = "C:\GOG Games\Anno 1602"` |
| cmd | `set ANNO1602_DIR=C:\GOG Games\Anno 1602` |

To keep it set, add the line to `~/.bashrc` / `~/.zshrc`, use `set -Ux` once in fish, or `setx ANNO1602_DIR "..."`
on Windows. On Linux and macOS the GOG installer can be unpacked without Windows:
`innoextract setup_anno_1602*.exe` (or install it through Heroic or Wine).

## Building

Requirements on every platform: CMake ≥ 3.25, Ninja, a 64-bit C++20 compiler (GCC 11+, Clang 14+,
MSVC 2022) and, for the game itself, **SDL 3**. zlib is optional. 32-bit platforms are not supported.

Without `-DO1602_BUILD_GAME=ON` only the libraries, the tests and `o1602-tool` are built, not the game.
The presets are in [CMakePresets.json](CMakePresets.json): `release` for playing, `debug` and `asan`
(AddressSanitizer + UBSan) for development, `ci` (warnings as errors) as in CI, `vcpkg` for Windows.

### Linux

Install the tools and SDL 3, for example:

```sh
sudo pacman -S --needed base-devel cmake ninja sdl3                 # Arch, CachyOS, Manjaro
sudo dnf install gcc-c++ cmake ninja-build SDL3-devel zlib-devel     # Fedora 41+
sudo apt install build-essential cmake ninja-build libsdl3-dev zlib1g-dev  # Debian 13, Ubuntu 25.04+
```

Ubuntu 24.04 has no SDL 3 package; build it from source as CI does
(see [.github/workflows/ci.yml](.github/workflows/ci.yml)) and pass `-DCMAKE_PREFIX_PATH=<sdl3 install dir>`.

```sh
cmake --preset release -DO1602_BUILD_GAME=ON
cmake --build --preset release
ctest --preset release
```

### macOS

With [Homebrew](https://brew.sh) (Apple Silicon and Intel):

```sh
xcode-select --install
brew install cmake ninja sdl3
cmake --preset release -DO1602_BUILD_GAME=ON
cmake --build --preset release
ctest --preset release
```

### Windows

Install Visual Studio 2022 (or its Build Tools) with the workload "Desktop development with C++"; it
brings CMake and Ninja. SDL 3 and zlib come from [vcpkg](https://vcpkg.io), which the `vcpkg` preset
uses. In an **x64 Native Tools Command Prompt for VS 2022** (or the matching Developer PowerShell):

```bat
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
set VCPKG_ROOT=C:\vcpkg

cmake --preset vcpkg
cmake --build --preset vcpkg
ctest --preset vcpkg
```

The `vcpkg` preset builds the game and copies `SDL3.dll` next to it.

## Running

| Platform | Game |
|---|---|
| Linux, macOS | `./build/release/engine/app/open1602` |
| Windows | `build\vcpkg\engine\app\open1602.exe` |

Without arguments the game starts in the main menu. Some more ways to start:

```sh
open1602 --intro                                   # the logos and the intro first, as the original
open1602 --scenario "Szenes/Das Atoll.szs"         # a scenario of the installation
open1602 --new-game 1 --name Merlin                # a new endless game, difficulty 0 (easy) .. 3 (hard)
open1602 --savegame NAME                           # a savegame of the original by its name
open1602 --load path/to/game.o1602                 # a savegame of Open1602
open1602 --help                                    # every option
```

In the game **F1** shows the keys (they can be changed in `keys.cfg`). Savegames, settings and key
settings are kept in the user's data folder, never in the installation:

| Platform | Data folder |
|---|---|
| Linux | `~/.local/share/Open1602/` |
| macOS | `~/Library/Application Support/Open1602/` |
| Windows | `%APPDATA%\Open1602\` |

Tests that need the original data run when `ANNO1602_DIR` is set and skip themselves otherwise.

## Tools

`o1602-tool` (in `build/<preset>/tools/o1602-tool/`) inspects and converts the original's files:

```sh
o1602-tool chunks     <file>                         # chunk structure of any binary file
o1602-tool cod        haeuser.cod haeuser.txt        # decrypt a .cod script
o1602-tool bsh-export GFX/SHIP.BSH out/ --sheet      # sprites as PNG
o1602-tool islands    NORD/big.scp                   # island data
o1602-tool cod-dump   haeuser.cod --type HAUS        # interpret a .cod script
o1602-tool render-island NORD/lar01.SCP island.png   # draw an island isometrically
o1602-tool render-world "Szenes/Das Atoll.szs" w.png # draw a whole world map
o1602-tool savegames                                 # the savegames of the installation with their names
```

## Project layout

| Folder | Contents |
|---|---|
| `engine/` | Engine modules: `core`, `assets`, `sim`, `render`, `platform`, `audio`, `video`, `ui`, `app` |
| `tools/` | `o1602-tool` (C++) and Python prototypes |
| `tests/` | Unit tests and tests against the original data |
| `docs/` | [Plan](docs/00-plan.md), [status](docs/stand.md), [file formats](docs/formats/README.md), [architecture decisions](docs/architecture/README.md) |
| `re/` | Analysis scripts and notes |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). The most important rules: no original files and no decompiled
code in the repository; every finding about the original is documented with its evidence.

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). *Anno* is a trademark of Ubisoft; this project is not
affiliated with Ubisoft, Max Design or Sunflowers.
