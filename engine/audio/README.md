# engine/audio

Ton ohne Plattformabhängigkeit (ersetzt `Maxsound.dll`): WAVE-Dateien lesen, auch die 4-Bit-Musik
(`wav.hpp`), Software-Mischer (`mixer.hpp`), Dateien der Spielinstallation (`sound_bank.hpp`) und die
Töne des Spiels (`game_audio.hpp`: Geräusche auf dem Bildschirm, Sprachmeldungen, Musik). Das Gerät ist
`platform::AudioOutput`; der Betrachter verbindet beides in `engine/app/src/game_sound.*`.

Beschreibung: [docs/re/audio.md](../../docs/re/audio.md), [docs/formats/wav.md](../../docs/formats/wav.md).
Videos (Smacker) fehlen noch.
