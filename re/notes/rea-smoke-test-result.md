# rea-Machbarkeitstest – Ergebnis

Datum: 16.10.2026 · rea 6.5.0 · Ghidra 12.1.2 · JDK 21.0.12 · CachyOS (Linux x64)

**Ergebnis: bestanden.** rea analysiert `1602.exe` (PE32, x86) vollständig über das Ghidra-Backend.
Alle Prüfschritte von `re/scripts/rea-smoke-test.sh` liefen fehlerfrei (Exit 0).

| Prüfung | Erwartung (aus objdump-Analyse) | rea/Ghidra |
|---|---|---|
| Format/Architektur | PE32 / i386 | `format: pe`, `architecture: x86` ✅ |
| Umfang | ~2000 Funktionen geschätzt | 1896 Funktionen, 1101 definierte Strings |
| WinMain `0x004081D0` | aufgerufen vom Entry Point, ruft `Game_Init` | Caller `entry` (`0x00495CE4`, Aufruf bei `0x00495E13`); Callees u. a. `GetMessageA`, `PeekMessageA`, `TranslateAcceleratorA`, `DispatchMessageA` und `0x00494870` ✅ |
| Text.cod-Lader `0x0048F8D0` | Datei lesen, jedes Byte negieren | Dekompilat zeigt `sprintf("%sText.cod")`, `ReadFile`, Negationsschleife, Globals `0x005B6FC0`/`0x005B6050`; danach Parser für `[Abschnitt]`-Blöcke ✅ |
| xrefs `haeuser.cod`, Suche `stadtfld`, Instruktion `0x0048F950` | | Exit 0 (Details im lokalen Log) |

## Folgen

- rea ist das Hauptwerkzeug für das Reverse Engineering (siehe `docs/re/README.md`).
- Die rea-Werkzeuge sind über Claude Desktop auch in Claude-Sitzungen verfügbar, die mit dem Rechner
  verbunden sind.
- rea arbeitet mit einer ephemeren Ghidra-Datenbank. Namen und Kommentare bleiben daher in
  `re/symbols/*.csv` die Quelle der Wahrheit. Umbenennen in der Sitzung (`set_addresses_names`) schlug
  über die Desktop-Verbindung mit einem Formatfehler fehl; `annotate_native_function` ist die Alternative.
- Das Log (`rea-smoke-test-*.log`) enthält Dekompilat und wird nicht eingecheckt.
