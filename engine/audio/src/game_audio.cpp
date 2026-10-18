// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/audio/game_audio.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace o1602::audio {

namespace {

// Files of the sound numbers as GameInit loads them (0x004954A6..0x00495973); numbers 54 and 68
// share the file of 55 and 53 (0x004957BE). Empty: the original leaves them silent.
constexpr std::array<const char*, kSampleCount> kSampleFiles = {
    nullptr,    "kanone5",  "kanone7",  "kanone6",  "kanone8",  "muskete1", "schwert1", "schwert2", "schwert3",
    "schwert4", "schwert5", "arg1",     nullptr,    "mahenW",   "dongelW",  "hack1",    "hack2",    "stein1",
    "stein2",   "muh1",     "muh2",     "muhle",    nullptr,    "schaf1",   "schaf2",   "schaf3",   "baumW",
    "vogel1",   "vogel2",   "vogel3",   "vogel4",   "vogel5",   "vogel6",   "wellenW",  "selectW",  "event",
    "standard", "start",    "markt",    "schloss",  "kapelle",  "kirche",   "dom",      "schule",   "theater",
    "burg",     "arzt",     "badehaus", "denkmal",  nullptr,    "fleischr", "forster",  nullptr,    "steinmtz",
    "schwertb", "schwertb", "gieserei", "jagdhtte", "plantage", "piraten",  nullptr,    "rumbnrei", "brunnen",
    "triumph",  nullptr,    nullptr,    "wirtshs",  "wohnhaus", "steinmtz", "vulkan1",  "vulkan2",  "vulkan3",
    nullptr,    "sdtattk1", "sdtattk2", "sdtattk3", "sdtattk4", "sdtattk5", "sdtattk6", "shpattk1", "shpattk2",
    "shpattk3", "shpattk4", "sdtmove1", "sdtmove2", "sdtmove3", "sdtmove4", "shpmove1", "shpmove2", "shpmove3",
    "shpmove4", "shpmove5",
};

// The messages' voices and videos (0x0049B210, 0x70 entries): kind, first voice, voices, videos,
// flags.
constexpr std::array<MessageAudio, 0x70> kMessages = {{
    {0, 0, 0, 0, 0},    {1, 282, 3, 1, 6},  {1, 295, 4, 1, 6},  {1, 187, 4, 1, 4},  {1, 198, 5, 1, 8},
    {5, 784, 5, 2, 34}, {0, 626, 5, 1, 8},  {0, 637, 2, 0, 0},  {5, 703, 1, 1, 34}, {5, 660, 2, 1, 32},
    {4, 598, 1, 0, 0},  {3, 651, 2, 0, 0},  {0, 729, 4, 0, 0},  {0, 768, 1, 0, 0},  {3, 530, 6, 0, 0},
    {5, 774, 4, 1, 2},  {5, 675, 4, 1, 2},  {2, 338, 1, 0, 0},  {2, 338, 1, 0, 0},  {2, 330, 1, 0, 0},
    {2, 332, 1, 0, 0},  {2, 334, 2, 1, 0},  {2, 342, 2, 0, 0},  {2, 346, 1, 0, 0},  {2, 348, 1, 0, 0},
    {2, 350, 1, 0, 0},  {2, 352, 1, 0, 0},  {2, 354, 1, 0, 0},  {2, 358, 1, 0, 0},  {2, 363, 1, 0, 0},
    {0, 360, 1, 0, 0},  {3, 560, 1, 0, 0},  {3, 558, 1, 0, 0},  {3, 542, 1, 0, 0},  {3, 544, 1, 0, 0},
    {3, 546, 1, 0, 0},  {3, 547, 1, 0, 0},  {3, 548, 1, 0, 0},  {3, 550, 1, 0, 0},  {3, 552, 1, 0, 0},
    {3, 554, 1, 0, 0},  {3, 556, 1, 0, 0},  {1, 133, 1, 0, 0},  {1, 141, 6, 1, 8},  {1, 153, 6, 1, 8},
    {1, 211, 7, 1, 8},  {1, 177, 4, 1, 1},  {1, 167, 3, 0, 0},  {1, 794, 4, 0, 0},  {1, 241, 2, 1, 8},
    {1, 252, 3, 1, 0},  {2, 464, 1, 0, 64}, {2, 460, 2, 0, 0},  {2, 478, 2, 0, 0},  {2, 474, 2, 0, 0},
    {2, 482, 2, 0, 0},  {2, 470, 2, 0, 0},  {2, 466, 2, 0, 0},  {2, 486, 2, 0, 0},  {2, 490, 2, 0, 0},
    {2, 420, 1, 0, 0},  {2, 426, 2, 0, 0},  {2, 438, 1, 0, 0},  {2, 442, 1, 0, 0},  {2, 448, 1, 0, 0},
    {2, 450, 2, 0, 0},  {2, 456, 1, 0, 0},  {2, 262, 3, 1, 1},  {2, 270, 3, 1, 1},  {0, 752, 3, 0, 16},
    {0, 756, 1, 0, 16}, {0, 641, 3, 0, 16}, {0, 381, 2, 0, 0},  {0, 564, 1, 0, 0},  {0, 562, 1, 0, 0},
    {0, 572, 1, 0, 0},  {0, 0, 0, 0, 0},    {7, 926, 3, 0, 0},  {7, 933, 3, 1, 8},  {7, 0, 0, 0, 0},
    {7, 915, 2, 0, 0},  {7, 911, 2, 1, 8},  {7, 0, 0, 0, 0},    {0, 1012, 3, 0, 0}, {0, 990, 3, 0, 0},
    {0, 988, 1, 1, 1},  {0, 986, 1, 1, 1},  {0, 992, 1, 1, 1},  {0, 984, 1, 1, 1},  {0, 957, 1, 0, 0},
    {0, 959, 1, 0, 0},  {0, 1145, 1, 0, 0}, {0, 1143, 1, 0, 0}, {0, 610, 1, 0, 0},  {0, 0, 1, 0, 2},
    {0, 2000, 1, 1, 1}, {0, 2010, 1, 1, 1}, {0, 2020, 1, 1, 1}, {0, 2030, 1, 1, 1}, {0, 2040, 1, 1, 1},
    {0, 2050, 1, 1, 1}, {0, 2060, 1, 1, 1}, {0, 2070, 1, 1, 1}, {0, 2080, 1, 1, 1}, {0, 2100, 1, 1, 1},
    {0, 2110, 1, 1, 1}, {0, 2120, 1, 1, 1}, {0, 2500, 1, 1, 1}, {0, 2510, 1, 1, 1}, {0, 2520, 1, 1, 1},
    {0, 2530, 1, 1, 1}, {0, 2990, 1, 1, 1},
}};

constexpr std::uint32_t kBattleMs = 25'000;       // back to normal music after the last shot
constexpr std::uint32_t kBattleStartMs = 10'000;  // a new battle track only shortly after a shot
constexpr std::int32_t kFadeRate = 42;            // Music_SetMode(.., 42): about 3 s
constexpr std::size_t kHistory = 8;

std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

/// "  Key: a, b, c" lines of Game.dat: the values after `key`, split at commas.
std::vector<std::vector<std::string>> lines_of(std::string_view text, std::string_view key) {
    std::vector<std::vector<std::string>> result;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string_view::npos) {
            continue;
        }
        line.remove_prefix(first);
        if (line.size() <= key.size() || lower(line.substr(0, key.size())) != lower(key) || line[key.size()] != ':') {
            continue;
        }
        std::vector<std::string> values;
        std::string_view rest = line.substr(key.size() + 1);
        while (!rest.empty()) {
            const std::size_t comma = rest.find(',');
            std::string_view v = rest.substr(0, comma);
            const std::size_t a = v.find_first_not_of(" \t\r\"");
            const std::size_t b = v.find_last_not_of(" \t\r\"");
            values.emplace_back(a == std::string_view::npos ? std::string() : std::string(v.substr(a, b - a + 1)));
            if (comma == std::string_view::npos) {
                break;
            }
            rest.remove_prefix(comma + 1);
        }
        result.push_back(std::move(values));
    }
    return result;
}

bool truth(const std::string& v) {
    return lower(v) == "true" || v == "1";
}

std::optional<std::int32_t> number(const std::string& v) {
    try {
        std::size_t used = 0;
        const int n = std::stoi(v, &used);
        return used == v.size() ? std::optional<std::int32_t>(n) : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace

std::optional<std::string> sample_file(SampleId id) {
    if (id >= kSampleFiles.size() || kSampleFiles[id] == nullptr) {
        return std::nullopt;
    }
    return std::string("SAMPLES/") + kSampleFiles[id] + ".wav";
}

float gain_of(std::int32_t centibel) noexcept {
    if (centibel >= 0) {
        return 1.0F;
    }
    if (centibel <= -8000) {
        return 0.0F;
    }
    return std::pow(10.0F, static_cast<float>(centibel) / 2000.0F);
}

AudioSettings parse_audio_settings(std::string_view text) {
    AudioSettings s;
    for (const auto& v : lines_of(text, "Musik")) {
        if (!v.empty()) {
            s.music = truth(v[0]);
        }
    }
    for (const auto& v : lines_of(text, "Samples")) {
        if (!v.empty()) {
            s.effects = truth(v[0]);
        }
    }
    for (const auto& v : lines_of(text, "Random")) {
        if (!v.empty()) {
            s.random_music = truth(v[0]);
        }
    }
    for (const auto& v : lines_of(text, "Volume")) {
        if (v.size() >= 2) {
            s.music_volume = std::clamp(number(v[0]).value_or(s.music_volume), -4000, 0);
            s.effect_volume = std::clamp(number(v[1]).value_or(s.effect_volume), -4000, 0);
        }
    }
    for (const auto& v : lines_of(text, "Speach")) {
        // "Speach: k, TRUE" switches the voices of kind k off (0: all).
        if (v.size() >= 2) {
            const auto k = number(v[0]);
            if (k && *k >= 0 && *k < static_cast<std::int32_t>(s.speech.size())) {
                s.speech[static_cast<std::size_t>(*k)] = !truth(v[1]);
            }
        }
    }
    for (const auto& v : lines_of(text, "Video")) {
        // "Video: k, TRUE" switches the videos of kind k off (0: all).
        if (v.size() >= 2) {
            const auto k = number(v[0]);
            if (k && *k >= 0 && *k < static_cast<std::int32_t>(s.video.size())) {
                s.video[static_cast<std::size_t>(*k)] = !truth(v[1]);
            }
        }
    }
    return s;
}

// ---- Effects -------------------------------------------------------------------------------------

void Effects::request(SampleId id, float x, float y) {
    if (m_queue.size() < m_limit && sample_file(id)) {
        m_queue.push_back(Request{id, x, y});
    }
}

std::optional<std::pair<float, float>> Effects::placement(float x, float y) noexcept {
    if (std::fabs(x) >= 1.0F || std::fabs(y) >= 1.0F) {
        return std::nullopt;
    }
    // Source at (x, y, 0), listener at (0, 0, -0.5), DirectSound's minimum distance 1, rolloff 2.
    const float d = std::sqrt(x * x + y * y + 0.25F);
    const float volume = d <= 1.0F ? 1.0F : 1.0F / (d * d);
    // Left and right follow the direction in the horizontal plane; up and down do not pan.
    const float pan = x / std::sqrt(x * x + 0.25F);
    return std::make_pair(volume, pan);
}

void Effects::flush() {
    if (m_enabled) {
        for (const Request& r : m_queue) {
            const auto place = placement(r.x, r.y);
            const auto file = sample_file(r.id);
            if (!place || !file) {
                continue;
            }
            PlayOptions o;
            o.volume = place->first * m_gain;
            o.pan = place->second;
            m_mixer->play(m_bank->get(*file), o);
        }
    }
    m_queue.clear();
}

// ---- Voice messages ------------------------------------------------------------------------------

const MessageAudio* message_audio(std::uint16_t message) noexcept {
    return message < kMessages.size() ? &kMessages[message] : nullptr;
}

std::uint16_t Voices::message(std::uint16_t message, bool visible) {
    const MessageAudio* m = message_audio(message);
    if (m == nullptr) {
        return 0;
    }
    // Video first (Msg_Audio 0x00488E80): off screen, or always, or at a quarter of the times. The
    // random numbers are the interface's own, not the game's (S-112).
    const bool videos_on = m_settings.video[0] && (m->kind >= m_settings.video.size() || m_settings.video[m->kind]);
    if (m->videos > 0 && videos_on && m_video_check) {
        const bool chance = (m->flags & kMessageVideoQuarter) != 0 && (m_random() & 3U) == 0;
        if (!visible || (m->flags & kMessageVideoAlways) != 0 || chance) {
            const auto pick =
                static_cast<std::uint16_t>(std::uniform_int_distribution<int>(0, m->videos - 1)(m_random));
            const auto number = static_cast<std::uint16_t>(m->base + pick);
            if (m_video_check(number)) {
                m_video = number;
                return m->flags;
            }
        }
    }
    const bool allowed = m_settings.speech[0] && (m->kind >= m_settings.speech.size() || m_settings.speech[m->kind]);
    if (m->voices > 0 && allowed && (!visible || (m->flags & kMessageVoiceHidden) == 0)) {
        for (std::size_t i = 0; i < kSlots; ++i) {
            std::uint16_t& slot = m_queue[(m_cursor + i) % kSlots];
            if (slot == 0) {
                const auto pick =
                    static_cast<std::uint16_t>(std::uniform_int_distribution<int>(0, m->voices - 1)(m_random));
                slot = static_cast<std::uint16_t>(m->base + 2 * pick);
                return m->flags;
            }
        }
    }
    // No voice, or no room: the message sound in the middle.
    m_effects->request(kWavEvent, 0.0F, 0.0F);
    return m->flags;
}

void Voices::update() {
    if (m_mixer->playing(m_voice)) {
        return;
    }
    m_voice = Mixer::kNoVoice;
    for (std::size_t i = 0; i < kSlots; ++i) {
        const std::size_t at = (m_cursor + i) % kSlots;
        if (m_queue[at] == 0) {
            continue;
        }
        const std::uint16_t n = m_queue[at];
        m_queue[at] = 0;
        m_cursor = (at + 1) % kSlots;
        if (auto pcm = m_bank->load("Speech8/" + std::to_string(n) + ".wav")) {
            PlayOptions o;
            o.group = Group::Speech;
            // Voices are a quarter less attenuated than the effects (0x00488D50).
            o.volume = gain_of(m_settings.effect_volume - m_settings.effect_volume / 4);
            m_voice = m_mixer->play(std::move(pcm), o);
            return;
        }
        // A missing file (the original references some): try the next one.
    }
}

void Voices::stop_all() {
    m_queue.fill(0);
    m_mixer->stop(m_voice);
    m_voice = Mixer::kNoVoice;
}

std::size_t Voices::waiting() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(m_queue.begin(), m_queue.end(), [](std::uint16_t n) { return n != 0; }));
}

// ---- Music ---------------------------------------------------------------------------------------

Music::Music(Mixer& mixer, SoundBank& bank, std::vector<std::string> tracks, std::vector<std::string> fights,
             std::uint32_t seed)
    : m_mixer(&mixer),
      m_bank(&bank),
      m_tracks(std::move(tracks)),
      m_fights(std::move(fights)),
      m_enabled(m_tracks.size(), true),
      m_random(seed) {}

void Music::scan(const assets::GameDirectory& game, std::vector<std::string>& tracks,
                 std::vector<std::string>& fights) {
    tracks.clear();
    fights.clear();
    const auto dir = game.resolve("MUSIC8");
    std::error_code ec;
    if (!dir || !std::filesystem::is_directory(*dir, ec)) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(*dir, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.size() < 5 || lower(name.substr(name.size() - 4)) != ".wav") {
            continue;
        }
        const std::string title = name.substr(0, name.size() - 4);
        (lower(title).rfind("fight", 0) == 0 ? fights : tracks).push_back(title);
    }
    // By name, case ignored: the same order on every system (the original takes the file system's,
    // U-014); as many tracks as there are (L-009).
    const auto by_name = [](const std::string& a, const std::string& b) { return lower(a) < lower(b); };
    std::sort(tracks.begin(), tracks.end(), by_name);
    std::sort(fights.begin(), fights.end(), by_name);
}

void Music::set_settings(const AudioSettings& s) {
    m_settings = s;
    for (std::size_t i = 0; i < m_tracks.size(); ++i) {
        m_enabled[i] = std::none_of(s.disabled_songs.begin(), s.disabled_songs.end(),
                                    [&](const std::string& t) { return lower(t) == lower(m_tracks[i]); });
    }
    const bool playing_disabled =
        !m_title.empty() && std::any_of(s.disabled_songs.begin(), s.disabled_songs.end(),
                                        [&](const std::string& t) { return lower(t) == lower(m_title); });
    if (!s.music || (playing_disabled && !m_battle)) {
        // Off, or the track playing was switched off in the list: it stops at once.
        m_mixer->stop(m_voice);
        m_voice = Mixer::kNoVoice;
        m_title.clear();
    } else if (!m_fading) {
        m_mixer->set_volume(m_voice, gain_of(s.music_volume), 0.0F);
    }
}

void Music::shot(float x, float y, float view_width_px, float view_height_px, std::uint64_t game_ms) {
    // Near the middle of the view (0x004479B1): max + min / 4 of the pixel offsets within 1000.
    const float dx = std::fabs(x) * view_width_px / 2.0F;
    const float dy = std::fabs(y) * view_height_px / 2.0F;
    if (std::max(dx, dy) + std::min(dx, dy) / 4.0F > 1000.0F || m_fights.empty()) {
        return;
    }
    if (!m_battle) {
        m_battle = true;
        fade_out();
    }
    m_mode_ms = game_ms;
}

void Music::stop() {
    m_mixer->stop(m_voice);
    m_voice = Mixer::kNoVoice;
    m_fading = false;
    m_title.clear();
}

void Music::play(const std::string& title) {
    if (!m_settings.music) {
        return;
    }
    const auto it =
        std::find_if(m_tracks.begin(), m_tracks.end(), [&](const std::string& t) { return lower(t) == lower(title); });
    if (it == m_tracks.end()) {
        return;
    }
    stop();
    m_battle = false;
    m_current = static_cast<std::size_t>(it - m_tracks.begin());
    start(*it);
}

void Music::fade_out() {
    if (m_mixer->playing(m_voice) && !m_fading) {
        m_fading = true;
        m_fade_ms = 0;
    }
}

std::optional<std::string> Music::pick(std::uint64_t game_ms) {
    const auto recent = [&](const std::string& t, std::size_t window) {
        return std::find(m_history.end() - static_cast<std::ptrdiff_t>(std::min(window, m_history.size())),
                         m_history.end(), t) != m_history.end();
    };
    if (m_battle && game_ms - m_mode_ms <= kBattleStartMs && !m_fights.empty()) {
        // A battle track, not the last one again.
        for (int attempt = 0; attempt < 20; ++attempt) {
            const std::string& t =
                m_fights[std::uniform_int_distribution<std::size_t>(0, m_fights.size() - 1)(m_random)];
            if (m_fights.size() == 1 || !recent(t, 1)) {
                return t;
            }
        }
        return m_fights.front();
    }
    const auto enabled = static_cast<std::size_t>(std::count(m_enabled.begin(), m_enabled.end(), true));
    if (enabled == 0) {
        return std::nullopt;
    }
    if (m_settings.random_music) {
        // Not one of the last min(n / 2, 8) (Music_Update): only enabled tracks are drawn (U-016).
        std::vector<std::size_t> candidates;
        const std::size_t window = std::min<std::size_t>(enabled / 2, kHistory);
        for (std::size_t i = 0; i < m_tracks.size(); ++i) {
            if (m_enabled[i] && !recent(m_tracks[i], window)) {
                candidates.push_back(i);
            }
        }
        if (candidates.empty()) {
            return std::nullopt;
        }
        m_current = candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(m_random)];
        return m_tracks[*m_current];
    }
    // In order: the next enabled track after the current one.
    std::size_t i = m_current ? *m_current : m_tracks.size() - 1;
    for (std::size_t n = 0; n < m_tracks.size(); ++n) {
        i = (i + 1) % m_tracks.size();
        if (m_enabled[i]) {
            m_current = i;
            return m_tracks[i];
        }
    }
    return std::nullopt;
}

void Music::start(const std::string& title) {
    auto pcm = m_bank->load("MUSIC8/" + title + ".wav");
    m_history.push_back(title);
    if (m_history.size() > kHistory) {
        m_history.erase(m_history.begin());
    }
    if (pcm == nullptr) {
        return;
    }
    PlayOptions o;
    o.group = Group::Music;
    o.volume = gain_of(m_settings.music_volume);
    m_voice = m_mixer->play(std::move(pcm), o);
    m_title = title;
}

void Music::update(std::uint64_t game_ms, std::uint32_t real_ms) {
    if (m_battle && game_ms - m_mode_ms >= kBattleMs) {
        m_battle = false;
        m_mode_ms = game_ms;
        fade_out();
    }
    if (m_fading) {
        // vol = v - (v + 8000) * elapsed * rate / 128000 in 1/100 dB; gone below -79.99 dB.
        m_fade_ms += real_ms;
        const std::int64_t v = m_settings.music_volume;
        const std::int64_t now = v - (v + 8000) * m_fade_ms * kFadeRate / 128000;
        if (now <= -8000 || !m_mixer->playing(m_voice)) {
            m_mixer->stop(m_voice);
            m_voice = Mixer::kNoVoice;
            m_fading = false;
            m_title.clear();
        } else {
            m_mixer->set_volume(m_voice, gain_of(static_cast<std::int32_t>(now)), 0.0F);
            return;
        }
    }
    if (m_mixer->playing(m_voice)) {
        return;
    }
    m_voice = Mixer::kNoVoice;
    m_title.clear();
    if (!m_settings.music) {
        return;
    }
    if (const auto t = pick(game_ms)) {
        start(*t);
    }
}

std::string Music::playing() const {
    return m_mixer->playing(m_voice) ? m_title : std::string();
}

}  // namespace o1602::audio
