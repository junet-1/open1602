# 0002 – GPL-3.0, keine Originaldaten im Repository

Status: angenommen · 16.10.2026

## Entscheidung
- Lizenz: **GPL-3.0-or-later** (wie OpenRCT2 und OpenTTD). Jede Quelldatei trägt
  `SPDX-License-Identifier: GPL-3.0-or-later`.
- Das Repository enthält **keine** Dateien des Originalspiels (Grafik, Sound, Video, Texte, Karten,
  Binaries), keine daraus erzeugten Exporte (PNG-Dumps, dekodierte `.cod`-Texte) und kein Dekompilat.
- Die Engine benötigt eine legal erworbene Installation; der Pfad wird konfiguriert
  (`ANNO1602_DIR` bzw. später Einstellungsdialog).

## Begründung
Gängiges, rechtlich etabliertes Modell für Engine-Neuimplementierungen. Die GPL stellt sicher, dass
Verbesserungen der Community offen bleiben.

## Folgen
- `.gitignore` schließt typische Spieldateien und Exportordner aus.
- Tests gegen Originaldaten laufen nur, wenn `ANNO1602_DIR` gesetzt ist (in CI übersprungen).
- Fremdcode nur mit GPL-3.0-kompatibler Lizenz; Herkunft in `third_party/README.md` dokumentieren.
