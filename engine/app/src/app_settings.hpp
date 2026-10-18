// SPDX-License-Identifier: GPL-3.0-or-later
//
// Open1602's own settings and savegame names in the user's data folder (never Game.dat, never the
// installation): settings.cfg with the sounds of the panels MUSIK and ANNO, and saves/slots.cfg with
// the names of the twelve savegames of the panel DISK.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "o1602/audio/game_audio.hpp"

namespace o1602::app {

struct AppSettings {
    audio::AudioSettings audio;
    bool mission_window = false;  ///< "Auftrag ansehen"
    std::string player_name;      ///< the name typed in the menu (Windows-1252)
};

/// "key = value" lines; unknown keys are passed over, missing ones keep `defaults`.
[[nodiscard]] AppSettings parse_settings(std::string_view text, const AppSettings& defaults);
[[nodiscard]] std::string write_settings(const AppSettings& settings);

/// The savegames of the panel DISK: a name and the number of human players per slot.
struct SaveSlots {
    static constexpr std::size_t kCount = 12;
    std::array<std::string, kCount> names{};  ///< Windows-1252
    std::array<std::uint8_t, kCount> humans{};
};
/// Lines "slot humans name" (slot 0..11).
[[nodiscard]] SaveSlots parse_slots(std::string_view text);
[[nodiscard]] std::string write_slots(const SaveSlots& slots);
/// File of a slot in the savegame folder: spiel00.o1602 .. spiel11.o1602.
[[nodiscard]] std::filesystem::path slot_file(const std::filesystem::path& folder, std::size_t slot);

/// UTF-8 (typed text) to the Windows-1252 of the game's fonts; what it cannot show is left out.
[[nodiscard]] std::string utf8_to_cp1252(std::string_view utf8);

}  // namespace o1602::app
