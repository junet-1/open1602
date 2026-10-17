// SPDX-License-Identifier: GPL-3.0-or-later
//
// A new endless game as the original starts it (docs/re/newgame.md): the mission screen picks the
// difficulty, the scenario list one of three map layouts; the game loads Endlosspiel<D><V>.szs,
// which carries the world plan, four start ships, the money and the settings of that difficulty,
// makes the islands (world_generator.hpp) and resets what it does not take from the file: the
// players' control, colour and names, the game time and the clocks of droughts and volcanoes.
//
// Open1602 builds the scenario as the original would have it after that reset and loads it like
// any other, so the savegame writer has the right records to start from.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/core/result.hpp"
#include "o1602/sim/world.hpp"
#include "o1602/sim/world_generator.hpp"
#include "o1602/sim/world_plan.hpp"

namespace o1602::sim {

struct NewGameOptions {
    static constexpr std::uint8_t kDifficulties = 4;  ///< [ENDLESSKIND]: Einfach .. Schwer
    static constexpr std::uint8_t kLayouts = 3;       ///< map layouts per difficulty
    std::uint8_t difficulty = 0;                      ///< first digit of the file: money, resources, pirates
    std::uint8_t layout = 0;                          ///< second digit: the original rolls rand() % 3
    std::uint8_t colour = 0;                          ///< the human's banner, 0 red, 1 blue, 2 yellow, 3 white
    std::string name;                                 ///< the human's name (Windows-1252, at most 63 bytes)
    /// Identifier linking savegames and high scores (TIMERS +0xAC); new_game_id() makes one from
    /// the clock as the original does.
    std::uint32_t game_id = 0;
    /// How the islands are drawn; the original takes the RANDTAB from index rand() % 512.
    WorldStart world;
    /// An own world instead of the endless file's plan (world_plan.hpp): its size, islands and
    /// start areas, drawn from `world.seed`. Players, money and ships come from the level's file;
    /// `layout` does not matter then. At most four players until the multiplayer.
    std::optional<WorldSettings> custom;
};

/// The original's game id (0x00469410): year % 8, month, day, hour, minute and 8 random bits.
[[nodiscard]] std::uint32_t new_game_id(std::uint32_t year, std::uint32_t month, std::uint32_t day, std::uint32_t hour,
                                        std::uint32_t minute, std::uint32_t random) noexcept;

/// File of a difficulty and layout, relative to the game directory: Szenes/Endlosspiel<D><V>.szs.
[[nodiscard]] std::string endless_file(std::uint8_t difficulty, std::uint8_t layout);

/// The endless scenario as the original has it after the reset of a new game: control, colour and
/// names of the players, the gated TIMERS fields, no editor leftovers in the ships, NAME
/// "Endlosspiel". The world plan stays for load_world.
[[nodiscard]] core::Result<assets::ChunkFile> new_game_scenario(const assets::ChunkFile& endless,
                                                                const assets::TextTable& text,
                                                                const NewGameOptions& options);

/// The scenario of an own world: the players' ships at the plan's starts (none for players beyond
/// `players`), the map size if it is not the original's. The islands are still to be made.
[[nodiscard]] core::Result<assets::ChunkFile> own_world_scenario(const assets::ChunkFile& scenario,
                                                                 const WorldPlan& plan, std::uint8_t players);

/// Starts a new endless game: the scenario of new_game_scenario() with its islands made.
[[nodiscard]] core::Result<World> start_new_game(const assets::GameDirectory& game,
                                                 const assets::BuildingTable& buildings, const assets::TextTable& text,
                                                 const NewGameOptions& options,
                                                 std::vector<std::string>* warnings = nullptr);

}  // namespace o1602::sim
