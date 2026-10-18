# engine/video

Videos without platform dependencies (replaces `smackw32.dll`): our own Smacker decoder
(`smacker.hpp`), written from the public format description. It delivers, frame by frame, the
palette indices and the palette (or RGBA directly) and each audio track as a whole for the mixer
(`audio::Pcm`). Display and timing are handled by the frontend.

Description: [docs/formats/smk.md](../../docs/formats/smk.md).
