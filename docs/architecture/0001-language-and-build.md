# 0001 – C++20, CMake, SDL3

Status: angenommen · 16.10.2026

## Kontext
Die Engine soll plattformübergreifend (Linux, Windows, macOS), schnell und über lange Sitzungen
stabil laufen. Das Original ist ein MSVC-6-C++-Programm.

## Entscheidung
- **C++20** für Engine und Werkzeuge; **Python 3** für Forschungs-/RE-Skripte.
- **CMake ≥ 3.25** mit Presets; Abhängigkeiten unter Linux aus dem System, unter Windows/macOS über **vcpkg**.
- **SDL3** als einzige Plattformschicht (Fenster, Eingabe, Audio, Timer, GPU-API).
- `assets` und `core` bleiben ohne externe Pflichtabhängigkeit (zlib optional).

## Begründung
Strukturen und Algorithmen des Originals lassen sich direkt in C++ abbilden; vergleichbare Projekte
(OpenRCT2, OpenTTD, OpenMW, Julius/Augustus) belegen die Tragfähigkeit. Rust wurde erwogen
(Speichersicherheit), aber wegen schwächerer Abbildung der Originalstrukturen und des kleineren
Spieleentwickler-Ökosystems verworfen.

## Folgen
Speichersicherheit muss durch Regeln erzwungen werden: RAII, keine besitzenden Rohzeiger,
bounds-geprüftes Parsen (`ByteReader`, `Result<T>`), Warnungen als Fehler in CI, ASan/UBSan-Builds.
