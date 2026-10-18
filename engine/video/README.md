# engine/video

Videos ohne Plattformabhängigkeit (ersetzt `smackw32.dll`): ein eigener Smacker-Decoder
(`smacker.hpp`), geschrieben nach der öffentlichen Formatbeschreibung. Er liefert Bild für Bild die
Palettenindizes und die Palette (oder gleich RGBA) und jede Tonspur als Ganzes für den Mischer
(`audio::Pcm`). Anzeige und Zeitsteuerung übernimmt das Frontend.

Beschreibung: [docs/formats/smk.md](../../docs/formats/smk.md).
