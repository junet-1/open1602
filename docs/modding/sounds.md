# Sounds and music of a package's own

A content package may bring its own music and the sounds of its messages as WAV, Ogg Vorbis or
FLAC files ([ADR 0007](../architecture/0007-extensibility-and-modding.md) section 2, item 4 of
[open-work.md](open-work.md)).

**Status:** Done are the file `music.toml` (tracks of a package beside the original's `MUSIC8`, as
normal or as battle music) and sounds of messages in all three formats
([messages.md](messages.md)). Still open: sounds of a package's buildings and figures (they sound
like the building or figure they copy).

## The formats

| Format | Read |
|---|---|
| WAV | PCM of 8 or 16 bits, and the original's 4-bit music |
| Ogg Vorbis (`.ogg`) | any quality |
| FLAC (`.flac`) | any bit depth |

The game tells the format by the file's first bytes, not by its name. Any sample rate up to
192,000 Hz is played; the mixer converts it to the device's. A file of more than two channels keeps
its front left and right, and a file longer than 20 minutes is cut there.

## Music

```
<user data folder>/mods/<package>/music.toml
```

Each table names one track:

```toml
# mods/shanty/music.toml
[sea]
file = "music/sea.ogg"
title = "Sea Shanty"

[storm]
file = "music/storm.flac"
battle = true
```

| Field | Limits | Meaning |
|---|---|---|
| `file` | a path in the package, folders split by `/` | the sound file of the track (required) |
| `title` | 1 to 64 bytes | what the music list in the options shows; the table's name if not given |
| `battle` | `true` or `false` | battle music, played while shots fall near the middle of the view as the original's `Fight` tracks; `false` if not given |

The packages' tracks come after the original's in the music list, in the packages' load order, and
are played at random or in order with them; the player turns them off there as the original's. A
track whose file is missing or is no sound file, or whose title another track has, is passed over
with a warning that names the file and the line. A track is decoded when it starts, beside the
game, as the original's are.

## Sounds of messages

A message of a package names its sound with `sound = "<path>"` ([messages.md](messages.md)); the
file may be WAV, Ogg Vorbis or FLAC. It is decoded once, when the game starts.

## In the code

`engine/audio/include/o1602/audio/sound_file.hpp`: `decode_sound` (the decoders are stb_vorbis and
dr_flac in `third_party`). `engine/audio/include/o1602/audio/package_music.hpp`: `apply_music_file`
and `MusicTrack`, which `audio::Music` plays beside `MUSIC8`'s tracks
(`app::GameSound::set_package_music`).
