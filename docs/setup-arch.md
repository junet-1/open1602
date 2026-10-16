# Entwicklungsumgebung unter Arch Linux

## 1. Build-Werkzeuge (Engine)

```sh
sudo pacman -Syu --needed base-devel git cmake ninja gcc clang gdb zlib sdl3 python python-pillow
```

Bauen und testen:

```sh
cd "$HOME/Projekt Anno"
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Tests zusätzlich gegen die eigene Installation:
export ANNO1602_DIR="$HOME/Games/Heroic/Anno 1602"
ctest --preset debug
./build/debug/tools/o1602-tool/o1602-tool islands "$ANNO1602_DIR/NORD/big.scp"
```

Weitere Presets: `release`, `asan` (AddressSanitizer + UBSan), `ci` (Warnungen als Fehler).

## 2. Reverse-Engineering-Werkzeuge

### Node.js + rea

rea braucht Node.js 22.19+ / 24.11+ / 26+ (die Arch-Pakete sind aktuell genug).

```sh
sudo pacman -S --needed nodejs npm
# globale npm-Pakete ohne sudo:
npm config set prefix "$HOME/.local"
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc   # bzw. ~/.zshrc
source ~/.bashrc

npm install --global rea-agents
rea --help
npx rea-agents setup        # registriert den MCP-Server bei Claude Code u. a.
```

### Ghidra 12.1.x + JDK

rea verlangt **Ghidra 12.1.x** und das dazu passende 64-Bit-JDK (laut Ghidra-Release-Notes, derzeit JDK 21).

```sh
sudo pacman -S --needed jdk21-openjdk
sudo archlinux-java set java-21-openjdk

# Variante A: Arch-Paket (prüfen, ob die Version 12.1.x ist!)
pacman -Si ghidra | grep '^Version'
sudo pacman -S ghidra                # installiert nach /opt/ghidra

# Variante B: offizielles Release von GitHub (NationalSecurityAgency/ghidra), z. B. nach ~/opt
#   unzip ghidra_12.1.x_PUBLIC_*.zip -d ~/opt

export GHIDRA_INSTALL_DIR=/opt/ghidra   # bzw. ~/opt/ghidra_12.1.x_PUBLIC
echo "export GHIDRA_INSTALL_DIR=$GHIDRA_INSTALL_DIR" >> ~/.bashrc
```

Den Ghidra-Pfad fragt `rea` bei der Einrichtung bzw. beim ersten Ghidra-Aufruf ab (siehe `rea --help`).

### Machbarkeitstest

```sh
export ANNO1602_DIR="$HOME/Games/Heroic/Anno 1602"
re/scripts/rea-smoke-test.sh
```

Das Log landet in `re/notes/rea-smoke-test-*.log`. Bitte einchecken oder mir zeigen – davon hängt ab,
ob wir rea direkt nutzen oder Ghidra headless mit den Skripten aus `re/scripts/ghidra/`.

### Weitere Werkzeuge

```sh
sudo pacman -S --needed imhex rizin binutils    # Hex-Editor mit Pattern-Sprache, Disassembler, objdump
```

## 3. Original unter Wine (Referenz für Paritätstests)

```sh
sudo pacman -S --needed wine winetricks
# Falls pacman 'wine' nicht findet: [multilib] in /etc/pacman.conf aktivieren, dann pacman -Syu
```

Eigener Prefix nur für die Analyse (die Heroic-Installation bleibt unberührt):

```sh
export WINEPREFIX="$HOME/.wine-anno1602"
WINEARCH=win32 wineboot -i 2>/dev/null || wineboot -i
cd "$ANNO1602_DIR" && wine 1602.exe
```

Debugger: `winedbg --gdb 1602.exe` (GDB-Protokoll, z. B. mit `gdb` oder Ghidra-Debugger verbindbar).
Falls DirectDraw unter Wine zickt: [cnc-ddraw](https://github.com/FunkyFr3sh/cnc-ddraw) als `ddraw.dll`
in einen *Kopie* des Spielordners legen.
