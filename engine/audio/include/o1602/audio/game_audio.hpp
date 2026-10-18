// SPDX-License-Identifier: GPL-3.0-or-later
//
// The sounds of the game as the original plays them (docs/re/audio.md): effects at a place on the
// screen, the voice messages and the music. Audio is presentation only: it follows the simulation's
// events and the camera, draws from its own random numbers (the original takes the game's rand(),
// S-112) and never changes the game.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "o1602/audio/mixer.hpp"
#include "o1602/audio/sound_bank.hpp"

namespace o1602::audio {

// ---- Effects -------------------------------------------------------------------------------------

/// The original's sound numbers (WAV_* of the .cod files, 0..91).
using SampleId = std::uint16_t;
inline constexpr std::size_t kSampleCount = 92;
inline constexpr SampleId kWavNothing = 0;
inline constexpr SampleId kWavDeadGuy = 11;        ///< a unit killed
inline constexpr SampleId kWavSelect = 34;         ///< a click on the panel
inline constexpr SampleId kWavEvent = 35;          ///< a message without voice
inline constexpr SampleId kWavSoldierAttack = 73;  ///< + 0..5
inline constexpr SampleId kWavShipAttack = 79;     ///< + 0..3
inline constexpr SampleId kWavSoldierMove = 83;    ///< + 0..3
inline constexpr SampleId kWavShipMove = 87;       ///< + 0..4

/// File of a sound relative to the game directory ("SAMPLES/kanone5.wav"), nothing for the
/// numbers the original leaves silent (GameInit 0x00494870).
[[nodiscard]] std::optional<std::string> sample_file(SampleId id);

/// Volume in the original's unit, 1/100 dB of attenuation (-4000 .. 0), as a gain.
[[nodiscard]] float gain_of(std::int32_t centibel) noexcept;

struct AudioSettings {
    bool music = true;
    bool effects = true;
    bool random_music = true;
    std::int32_t music_volume = -1000;  ///< 1/100 dB
    std::int32_t effect_volume = 0;
    /// Voice messages per kind (Text.cod [SPEECHKIND]): [0] all, [1..7] Ereignisse .. Diplomatie.
    std::array<bool, 8> speech{true, true, true, true, true, true, true, true};
    std::vector<std::string> disabled_songs;  ///< by track title (the original keeps indices)
};

/// The audio lines of the original's Game.dat (read only; Open1602 never writes it): Musik,
/// Samples, Random, Volume (the third value is the mission progress), Song, Speach.
[[nodiscard]] AudioSettings parse_audio_settings(std::string_view game_dat);

/// Effects at places on the screen (Maxsound.dll's MaxwavePlayOnce): the place is the offset from
/// the middle of the view, -1 .. 1 across its width and height. Outside the view a sound is not
/// played (the original culls by the whole screen around the view's middle, U-015); inside it
/// pans with x and softens towards the corners (DirectSound3D with the listener half a view behind
/// the screen and rolloff 2). At most `per_frame` requests per frame play, as in
/// the original (10, later ones are dropped).
class Effects {
public:
    Effects(Mixer& mixer, SoundBank& bank, std::size_t per_frame = 10)
        : m_mixer(&mixer), m_bank(&bank), m_limit(per_frame) {}

    void set_enabled(bool on) noexcept { m_enabled = on; }
    void set_volume(std::int32_t centibel) noexcept { m_gain = gain_of(centibel); }
    /// Queues a sound at (x, y) of the view.
    void request(SampleId id, float x, float y);
    /// Plays what was queued since the last call.
    void flush();

    /// Volume and pan of a sound at (x, y) of the view; nothing outside it.
    [[nodiscard]] static std::optional<std::pair<float, float>> placement(float x, float y) noexcept;

private:
    struct Request {
        SampleId id = 0;
        float x = 0.0F;
        float y = 0.0F;
    };
    Mixer* m_mixer;
    SoundBank* m_bank;
    std::size_t m_limit;
    bool m_enabled = true;
    float m_gain = 1.0F;
    std::vector<Request> m_queue;
};

// ---- Voice messages ------------------------------------------------------------------------------

/// What a message (MELDUNG number) brings (table 0x0049B210): its kind of voice message, the first
/// voice file and how many there are (every second number), the videos and flags.
struct MessageAudio {
    std::uint8_t kind = 0;  ///< [SPEECHKIND], 0 = only the master switch
    std::uint16_t base = 0;
    std::uint8_t voices = 0;
    std::uint8_t videos = 0;
    std::uint16_t flags = 0;
};
inline constexpr std::uint16_t kMessageVideoAlways = 0x01;
inline constexpr std::uint16_t kMessageNormalSpeed = 0x02;     ///< the game falls back to normal speed
inline constexpr std::uint16_t kMessageVoiceHidden = 0x20;     ///< voice only if the place is off screen
inline constexpr std::uint16_t kMessageMultiplayerWin = 0x6B;  ///< + banner 0..3

[[nodiscard]] const MessageAudio* message_audio(std::uint16_t message) noexcept;

/// The voice of the messages (Speech8): four waiting at most, one speaking at a time, never cut
/// off (0x00488D00, 0x00488D50). A message that finds no room, or has no voice, sounds event.wav.
class Voices {
public:
    Voices(Mixer& mixer, SoundBank& bank, Effects& effects, std::uint32_t seed = 1602)
        : m_mixer(&mixer), m_bank(&bank), m_effects(&effects), m_random(seed) {}

    void set_settings(const AudioSettings& s) noexcept { m_settings = s; }
    /// A message for the local player; `visible` whether its place is on screen. Returns the
    /// message's flags (kMessageNormalSpeed asks the game to slow down).
    std::uint16_t message(std::uint16_t message, bool visible);
    /// Starts the next voice when the last one has ended.
    void update();
    void stop_all();
    [[nodiscard]] std::size_t waiting() const noexcept;
    [[nodiscard]] bool speaking() const noexcept { return m_mixer->playing(m_voice); }

private:
    static constexpr std::size_t kSlots = 4;
    Mixer* m_mixer;
    SoundBank* m_bank;
    Effects* m_effects;
    std::mt19937 m_random;
    AudioSettings m_settings;
    std::array<std::uint16_t, kSlots> m_queue{};  ///< voice file numbers, 0 = free
    std::size_t m_cursor = 0;
    Mixer::VoiceId m_voice = Mixer::kNoVoice;
};

// ---- Music ---------------------------------------------------------------------------------------

/// The music (Music_Update 0x00489190): the tracks of MUSIC8 except Fight1..3, at random (not
/// one of the last few) or in order; battle music while shots fall near the middle of the view, back
/// 25 s after the last; a change fades out over 3 s. The order of the tracks is by name, not by
/// the file system's (U-014).
class Music {
public:
    Music(Mixer& mixer, SoundBank& bank, std::vector<std::string> tracks, std::vector<std::string> fights,
          std::uint32_t seed = 1602);

    /// Titles of the MUSIC8 files (without ".wav"), sorted: normal tracks and the Fight tracks.
    static void scan(const assets::GameDirectory& game, std::vector<std::string>& tracks,
                     std::vector<std::string>& fights);

    void set_settings(const AudioSettings& s);
    /// A shot at (x, y) of the view, in view half sizes; near the middle it calls the battle music.
    void shot(float x, float y, float view_width_px, float view_height_px, std::uint64_t game_ms);
    /// Every frame: `game_ms` the game time (the modes follow it), `real_ms` the frame time (fading).
    void update(std::uint64_t game_ms, std::uint32_t real_ms);

    [[nodiscard]] const std::vector<std::string>& tracks() const noexcept { return m_tracks; }
    /// Title playing now, empty if none.
    [[nodiscard]] std::string playing() const;
    [[nodiscard]] bool battle() const noexcept { return m_battle; }

private:
    void start(const std::string& title);
    void fade_out();
    [[nodiscard]] std::optional<std::string> pick(std::uint64_t game_ms);

    Mixer* m_mixer;
    SoundBank* m_bank;
    std::vector<std::string> m_tracks;
    std::vector<std::string> m_fights;
    std::vector<bool> m_enabled;
    std::mt19937 m_random;
    AudioSettings m_settings;
    Mixer::VoiceId m_voice = Mixer::kNoVoice;
    std::string m_title;
    std::vector<std::string> m_history;  ///< last tracks played, newest last
    std::optional<std::size_t> m_current;
    bool m_battle = false;
    std::uint64_t m_mode_ms = 0;  ///< game time of the last shot or mode change
    bool m_fading = false;
    std::uint32_t m_fade_ms = 0;
};

}  // namespace o1602::audio
