// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_settings.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <utility>
#include <vector>

namespace o1602::app {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

template <class F>
void for_lines(std::string_view text, F f) {
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        f(trim(text.substr(0, nl)));
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
    }
}

bool flag(std::string_view v, bool fallback) {
    if (v == "on" || v == "1" || v == "true") {
        return true;
    }
    if (v == "off" || v == "0" || v == "false") {
        return false;
    }
    return fallback;
}

std::int32_t number(std::string_view v, std::int32_t fallback) {
    std::int32_t n = 0;
    const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
    return ec == std::errc{} && end == v.data() + v.size() ? n : fallback;
}

void flags(std::string_view v, std::array<bool, 8>& out) {
    std::size_t i = 0;
    for (const char c : v) {
        if ((c == '0' || c == '1') && i < out.size()) {
            out[i++] = c == '1';
        }
    }
}

std::string flags_text(const std::array<bool, 8>& a) {
    std::string s;
    for (const bool b : a) {
        s += s.empty() ? "" : " ";
        s += b ? '1' : '0';
    }
    return s;
}

}  // namespace

AppSettings parse_settings(std::string_view text, const AppSettings& defaults) {
    AppSettings s = defaults;
    bool songs_listed = false;
    for_lines(text, [&](std::string_view line) {
        if (line.empty() || line.front() == '#') {
            return;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            return;
        }
        const std::string_view key = trim(line.substr(0, eq));
        const std::string_view value = trim(line.substr(eq + 1));
        audio::AudioSettings& a = s.audio;
        if (key == "music") {
            a.music = flag(value, a.music);
        } else if (key == "effects") {
            a.effects = flag(value, a.effects);
        } else if (key == "random_music") {
            a.random_music = flag(value, a.random_music);
        } else if (key == "music_volume") {
            a.music_volume = std::clamp(number(value, a.music_volume), -10000, 0);
        } else if (key == "effect_volume") {
            a.effect_volume = std::clamp(number(value, a.effect_volume), -10000, 0);
        } else if (key == "speech") {
            flags(value, a.speech);
        } else if (key == "video") {
            flags(value, a.video);
        } else if (key == "disabled_song") {
            if (!songs_listed) {
                a.disabled_songs.clear();  // the file's list replaces the default one
                songs_listed = true;
            }
            a.disabled_songs.emplace_back(value);
        } else if (key == "player_name") {
            s.player_name = std::string(value);
        } else if (key == "mission_window") {
            s.mission_window = flag(value, s.mission_window);
        } else if (key == "sea_creatures_every") {
            s.sea_creatures_every = std::clamp(number(value, s.sea_creatures_every), 0, 3600);
        }
    });
    return s;
}

std::string write_settings(const AppSettings& s) {
    const audio::AudioSettings& a = s.audio;
    std::string out = "# Open1602: settings (written by the game)\n";
    out += std::string("music = ") + (a.music ? "on" : "off") + "\n";
    out += std::string("effects = ") + (a.effects ? "on" : "off") + "\n";
    out += std::string("random_music = ") + (a.random_music ? "on" : "off") + "\n";
    out += "music_volume = " + std::to_string(a.music_volume) + "\n";
    out += "effect_volume = " + std::to_string(a.effect_volume) + "\n";
    out += "speech = " + flags_text(a.speech) + "\n";
    out += "video = " + flags_text(a.video) + "\n";
    for (const std::string& t : a.disabled_songs) {
        out += "disabled_song = " + t + "\n";
    }
    out += std::string("mission_window = ") + (s.mission_window ? "on" : "off") + "\n";
    out +=
        "# Whales, dolphins, krakens: on average one per screen of sea every this many seconds (original 5, 0 = off)\n";
    out += "sea_creatures_every = " + std::to_string(s.sea_creatures_every) + "\n";
    if (!s.player_name.empty()) {
        out += "player_name = " + s.player_name + "\n";
    }
    return out;
}

void SaveSlots::set(std::size_t slot, std::string name, std::uint8_t human_players) {
    if (slot >= kMaxCount) {
        return;
    }
    if (slot >= names.size()) {
        names.resize(slot + 1);
        humans.resize(slot + 1);
    }
    names[slot] = std::move(name);
    humans[slot] = human_players;
    // At least twelve, and one free slot after the last used one.
    std::size_t used = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (!names[i].empty()) {
            used = i + 1;
        }
    }
    const std::size_t count = std::min(std::max(kMinCount, used + 1), kMaxCount);
    names.resize(count);
    humans.resize(count);
}

SaveSlots parse_slots(std::string_view text) {
    SaveSlots slots;
    for_lines(text, [&](std::string_view line) {
        if (line.empty() || line.front() == '#') {
            return;
        }
        const std::size_t a = line.find(' ');
        const std::size_t b = a == std::string_view::npos ? a : line.find(' ', a + 1);
        if (b == std::string_view::npos) {
            return;
        }
        const std::int32_t slot = number(line.substr(0, a), -1);
        const std::int32_t humans = number(line.substr(a + 1, b - a - 1), 0);
        if (slot < 0 || static_cast<std::size_t>(slot) >= SaveSlots::kMaxCount) {
            return;
        }
        slots.set(static_cast<std::size_t>(slot), std::string(line.substr(b + 1)),
                  static_cast<std::uint8_t>(std::clamp(humans, 0, 255)));
    });
    return slots;
}

std::string write_slots(const SaveSlots& slots) {
    std::string out = "# Open1602: savegame names (slot, humans, name)\n";
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (!slots.names[i].empty()) {
            out += std::to_string(i) + " " + std::to_string(slots.humans[i]) + " " + slots.names[i] + "\n";
        }
    }
    return out;
}

std::filesystem::path slot_file(const std::filesystem::path& folder, std::size_t slot) {
    char name[32];
    std::snprintf(name, sizeof name, "spiel%02zu.o1602", slot);
    return folder / name;
}

std::string utf8_to_cp1252(std::string_view utf8) {
    // The letters of Windows-1252 at 0x80..0x9F that are not Latin-1.
    struct Letter {
        char32_t code;
        unsigned char byte;
    };
    static constexpr Letter kExtra[] = {
        {0x20AC, 0x80}, {0x201A, 0x82}, {0x0192, 0x83}, {0x201E, 0x84}, {0x2026, 0x85}, {0x2020, 0x86}, {0x2021, 0x87},
        {0x02C6, 0x88}, {0x2030, 0x89}, {0x0160, 0x8A}, {0x2039, 0x8B}, {0x0152, 0x8C}, {0x017D, 0x8E}, {0x2018, 0x91},
        {0x2019, 0x92}, {0x201C, 0x93}, {0x201D, 0x94}, {0x2022, 0x95}, {0x2013, 0x96}, {0x2014, 0x97}, {0x02DC, 0x98},
        {0x2122, 0x99}, {0x0161, 0x9A}, {0x203A, 0x9B}, {0x0153, 0x9C}, {0x017E, 0x9E}, {0x0178, 0x9F},
    };
    std::string out;
    for (std::size_t i = 0; i < utf8.size();) {
        const auto c = static_cast<unsigned char>(utf8[i]);
        char32_t cp = 0;
        std::size_t len = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1FU;
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0FU;
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07U;
            len = 4;
        } else {
            ++i;  // a stray continuation byte
            continue;
        }
        if (i + len > utf8.size()) {
            break;
        }
        bool ok = true;
        for (std::size_t k = 1; k < len; ++k) {
            const auto cc = static_cast<unsigned char>(utf8[i + k]);
            ok = ok && (cc & 0xC0) == 0x80;
            cp = (cp << 6U) | (cc & 0x3FU);
        }
        i += len;
        if (!ok) {
            continue;
        }
        if (cp < 0x80 || (cp >= 0xA0 && cp <= 0xFF)) {
            out.push_back(static_cast<char>(cp));
            continue;
        }
        for (const auto& [u, b] : kExtra) {
            if (u == cp) {
                out.push_back(static_cast<char>(b));
            }
        }
    }
    return out;
}

}  // namespace o1602::app
