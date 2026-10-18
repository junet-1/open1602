# Mitmachen bei Open1602

## Rechtliche Grundregeln

1. **Keine Originaldaten** im Repository – weder Spieldateien noch daraus erzeugte Exporte
   (PNGs, dekodierte `.cod`-Texte, Speicher-Dumps mit Spielinhalten).
2. **Kein dekompilierter Code** in `engine/`. Erkenntnisse werden als Spezifikation in `docs/`
   beschrieben und daraus neu implementiert (siehe ADR 0003).
3. Beiträge stehen unter GPL-3.0-or-later. Fremdcode nur mit kompatibler Lizenz, dokumentiert in
   `third_party/README.md`.

## Code

- C++20, Formatierung per `.clang-format` (`clang-format -i <dateien>`), 120 Zeichen.
- Bezeichner und Kommentare im Code auf **Englisch**; Projektdokumentation auf Deutsch.
  Originalbegriffe aus den Spieldaten (`Insel`, `Haus`, `Figur`) bleiben erkennbar.
- Namen: Typen `PascalCase`, Funktionen/Variablen `snake_case`, Member `m_name`, Konstanten `kName`.
- Fehlerbehandlung beim Laden von Daten über `core::Result<T>` – niemals abstürzen bei kaputten Dateien.
- Keine besitzenden Rohzeiger, kein `new`/`delete`; Container und RAII.
- Simulation (`engine/sim`): deterministisch, keine Gleitkommazahlen im Zustand (ADR 0004).
- Jede Datei beginnt mit `// SPDX-License-Identifier: GPL-3.0-or-later`.

## Reverse Engineering

- Neue Funktions-/Variablennamen in `re/symbols/*.csv` mit Konfidenz und Beleg eintragen.
- Gesicherte Formate/Strukturen in `docs/formats/` bzw. `docs/re/` dokumentieren.
- Gefundene Originalfehler in `docs/re/bugs.md` sammeln.

Die Symboltabellen (`re/symbols/`) und die Analyse des Originals (`docs/re/`) liegen nicht in
diesem Repository, sondern im privaten Repository `open1602-re`. Einrichten, neben diesem Ordner:

```sh
git clone <url-von-open1602-re> ../open1602-re
ln -s ../../open1602-re/docs/re docs/re
ln -s ../../open1602-re/re/symbols re/symbols
```

Beide Pfade stehen in der `.gitignore`; Änderungen daran werden in `open1602-re` committet.

## Commits

- Kleine, thematisch abgeschlossene Commits; Präfix nach Bereich, z. B. `assets: decode ZEI glyph overhang`,
  `re: name Cod_LoadHaeuser`, `docs: describe INSELHAUS bits`.
- Vor dem Commit: `cmake --build --preset debug && ctest --preset debug`.
