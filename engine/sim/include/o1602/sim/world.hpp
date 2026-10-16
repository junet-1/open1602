// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/core/result.hpp"
#include "o1602/sim/island_map.hpp"

namespace o1602::sim {

struct WorldIsland {
    assets::IslandHeader header;
    IslandMap map;
    std::uint32_t x = 0;  ///< position of the island's tile (0,0) on the world map
    std::uint32_t y = 0;
};

/// The world map: 500 x 350 tiles of open sea with islands placed on it (the original keeps one
/// 4-byte cell per world tile: island number, local x, local y, colour table - see rendering.md).
class World {
public:
    static constexpr std::uint32_t kWidth = 500;
    static constexpr std::uint32_t kHeight = 350;
    static constexpr std::uint8_t kSea = 0xFF;  ///< also limits original maps to 255 islands (L-003)

    World();

    /// What lies at a world tile.
    struct Cell {
        std::uint8_t island = kSea;  ///< index into islands(), kSea for open sea
        std::uint8_t local_x = 0;
        std::uint8_t local_y = 0;
    };

    [[nodiscard]] const Cell& cell(std::uint32_t x, std::uint32_t y) const {
        return m_cells[std::size_t{y} * kWidth + x];
    }
    [[nodiscard]] const std::vector<WorldIsland>& islands() const noexcept { return m_islands; }

    /// Adds an island at its header position. Fails if it leaves the world or overlaps another one.
    core::Status add_island(WorldIsland island);

private:
    std::vector<WorldIsland> m_islands;
    std::vector<Cell> m_cells;
};

/// Loads all islands of a scenario (.SZS/.SZM) or savegame (.GAM), each on its island template,
/// and places them on the world map. Problems are collected in `warnings`; islands that cannot be
/// placed are skipped.
core::Result<World> load_world(const assets::ChunkFile& file, const assets::BuildingTable& buildings,
                               const assets::GameDirectory& game, std::vector<std::string>* warnings = nullptr);

}  // namespace o1602::sim
