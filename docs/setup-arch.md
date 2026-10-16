# Entwicklungsumgebung unter Arch Linux

## 1. Build-Werkzeuge (Engine)

```sh
sudo pacman -Syu --needed base-devel git cmake ninja gcc clang gdb sdl3 python python-pillow
```

zlib wird absichtlich nicht explizit installiert: Auf Systemen mit `zlib-ng-compat` (z. B. CachyOS,
oder wenn Heroic es mitbringt) stellt dieses Paket `zlib.h` und `libz.so` bereit und kollidiert mit `zlib`.
CMake findet beide Varianten. Prüfen: `pacman -Qo /usr/include/zlib.h`.

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

### Kartenbetrachter `open1602` (SDL3)

Das Programm wird nur mit `-DO1602_BUILD_GAME=ON` gebaut und braucht das Paket `sdl3` (siehe oben):

```sh
cmake --preset release -DO1602_BUILD_GAME=ON
cmake --build --preset release
export ANNO1602_DIR="$HOME/Games/Heroic/Anno 1602"
./build/release/engine/app/open1602                                  # „Das Atoll“
./build/release/engine/app/open1602 --scenario "Szenes/Endlosspiel00.szs" --zoom 1
./build/release/engine/app/open1602 --savegame Merlin01           # Name aus dem Spiel (Game.dat)
./build/release/tools/o1602-tool/o1602-tool savegames               # alle Spielstände mit Namen
```

Steuerung: Pfeiltasten/WASD (mit Umschalt schneller) oder rechte/mittlere Maustaste ziehen zum
Scrollen · Mausrad zoomt an der Mausposition · 1/2/3 Zoomstufe · R / Umschalt+R drehen ·
+/− Pixelvergrößerung · O Besitzansicht (Gebiete in Spielerfarbe) · Leertaste hält Animationen an · Pos1 springt zur ersten Insel ·
F12 Bildschirmfoto · Esc/Q beenden. Die Titelleiste zeigt Feld unter der Maus, Bildrate und
Sprite-Anzahl. Mit `--frames N --screenshot bild.png` läuft das Programm N Bilder lang, speichert das
letzte und meldet die Bildrate (für Tests ohne Bedienung).

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
```

`rea setup` erst **nach** der Ghidra-Installation (nächster Abschnitt) ausführen, damit es
`GHIDRA_INSTALL_DIR` und das JDK in die MCP-Registrierung übernimmt.

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

Ghidra-Version prüfen (muss 12.1.x sein): `grep application.version "$GHIDRA_INSTALL_DIR/Ghidra/application.properties"`.
Das JDK muss vollständig sein (`javac` vorhanden) – `jdk21-openjdk`, nicht nur `jre21-openjdk`.

### rea einrichten

Claude Desktop vorher **komplett beenden** (nicht nur das Fenster schließen), sonst überschreibt es die Konfiguration.

```sh
source ~/.bashrc                  # GHIDRA_INSTALL_DIR muss in dieser Shell gesetzt sein
npx rea-agents setup              # Frage "Install Hopper?" mit **No** beantworten
npx rea-agents doctor             # prüft Ghidra-Version, analyzeHeadless, Java/javac
```

Hopper wird nicht gebraucht (wir nutzen Ghidra). Sein Arch-Paket kollidiert zudem mit `zlib-ng-compat`.

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
