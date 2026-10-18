# engine/platform

Plattformschicht auf SDL3: Fenster, Renderer, Zeitgeber; später Eingabe-Abbildung und Dateidialoge.
Einzige Stelle mit Betriebssystem-Abhängigkeiten ([ADR 0005](../../docs/architecture/0005-module-structure.md)).
Alles ist RAII; SDL-Typen gelangen nie nach `sim` oder `assets`.

Das SDL3-Zeichen-Backend liegt in `engine/render/sdl` (eigenes Ziel `o1602::render_sdl`), damit
Werkzeuge und Tests ohne SDL auskommen.
