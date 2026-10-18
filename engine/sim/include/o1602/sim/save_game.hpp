// SPDX-License-Identifier: GPL-3.0-or-later
//
// Open1602's own savegame (docs/formats/o1602-save.md): the whole state of a game in tagged
// sections behind a short header with a CRC-32, compressed with zlib. It has none of the limits of
// the original's format (players, ships, soldiers, towers, map size) and depends on no other file.
//
// The original's files (.SZS, .SZM, .GAM) are only read (world.hpp); a game can still be exported
// to a .GAM the original loads while it fits (savegame_writer.hpp).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/core/file.hpp"
#include "o1602/core/result.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

class Simulation;

/// File name extension of Open1602's savegames.
inline constexpr std::string_view kSaveExtension = ".o1602";

struct SaveOptions {
    /// Compress the sections with zlib (if the build has zlib; otherwise they are stored as they are).
    bool compress = true;
};

/// What the header of a savegame says, for a list of savegames.
struct SaveInfo {
    std::uint16_t version = 0;
    bool compressed = false;
    std::uint64_t size = 0;  ///< of the sections, uncompressed
};

/// Whether the bytes start like an Open1602 savegame.
[[nodiscard]] bool is_open1602_save(std::span<const std::uint8_t> bytes) noexcept;

/// The whole file. With the running `simulation` its clocks and figures are saved too, so the game
/// continues exactly; without one, a world loaded but not yet run keeps those it was loaded with.
[[nodiscard]] core::Result<core::Bytes> write_game(const World& world, const assets::BuildingTable& buildings,
                                                   const Simulation* simulation = nullptr,
                                                   const SaveOptions& options = {});

/// The header of a savegame (checks magic and version, not the data).
[[nodiscard]] core::Result<SaveInfo> save_info(std::span<const std::uint8_t> bytes);

/// Loads a savegame. Fails (with nothing loaded) if the file is damaged, of a newer Open1602 or
/// names buildings the game's data does not have.
[[nodiscard]] core::Result<World> read_game(std::span<const std::uint8_t> bytes, const assets::BuildingTable& buildings,
                                            std::vector<std::string>* warnings = nullptr);

}  // namespace o1602::sim
