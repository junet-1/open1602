# Architekturentscheidungen (ADRs)

Kurze, datierte Entscheidungen mit Begründung. Neue Entscheidung = neue Datei
`NNNN-titel.md`; überholte ADRs werden nicht gelöscht, sondern als „ersetzt durch NNNN“ markiert.

| # | Titel | Status |
|---|---|---|
| [0001](0001-language-and-build.md) | C++20, CMake, SDL3 | angenommen |
| [0002](0002-license-and-original-data.md) | GPL-3.0, keine Originaldaten im Repository | angenommen |
| [0003](0003-reimplementation.md) | Neuimplementierung statt Dekompilat | angenommen |
| [0004](0004-deterministic-simulation.md) | Deterministische, befehlsbasierte Simulation | angenommen |
| [0005](0005-module-structure.md) | Modulstruktur und Abhängigkeitsregeln | angenommen |
| [0006](0006-handling-original-bugs.md) | Umgang mit Fehlern des Originals, `sim::Rules` | angenommen |
| [0007](0007-extensibility-and-modding.md) | Erweiterbarkeit, Inhalte und Modding (Anforderungen, Banner, offene Spielerzahl) | angenommen |
| [0008](0008-game-settings.md) | Spieleinstellungen in `sim::Rules` (Endlosspiel einstellbar wie in späteren Anno-Teilen) | angenommen |
| [0009](0009-own-save-format.md) | Eigenes Speicherformat `.o1602`; Originaldateien nur lesen und exportieren | angenommen |
| [0010](0010-controls-and-interface.md) | Bedienung und Oberfläche: moderne Steuerung (Rechtsklick, Minikarte, Befehlsketten), freie Tasten, Rückfragen | angenommen |
