# engine/ui

Die Oberfläche ohne Plattformabhängigkeit: Schriften (`.ZEI`) und Textregeln (`font.hpp`), die
Gadget-Layouts des Originals im Betrieb (`panel.hpp`: Bereiche des Bildschirms, Zeichnen je Zustand,
Maus mit Umschaltern, Gruppen, Schiebern) und die Grafiken je Bildschirmgröße (`resources.hpp`).
Gelesen werden die Layouts in `engine/assets` (`gad.hpp`).

Beschreibung: [docs/re/ui.md](../../docs/re/ui.md), [docs/formats/gad.md](../../docs/formats/gad.md).
