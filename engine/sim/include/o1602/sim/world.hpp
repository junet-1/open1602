// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/assets/scenario.hpp"
#include "o1602/core/result.hpp"
#include "o1602/sim/island_map.hpp"

namespace o1602::sim {

struct WorldIsland {
    assets::IslandHeader header;
    IslandMap map;
    std::uint32_t x = 0;  ///< position of the island's tile (0,0) on the world map
    std::uint32_t y = 0;
};

/// A player slot (see assets::PlayerRecord). Owner numbers on tiles and ships index World::players().
struct Player {
    assets::PlayerKind kind = assets::PlayerKind::Human;
    assets::PlayerColor color = assets::PlayerColor::Red;
    std::int32_t money = 0;
    std::string name;
};

/// A ship on the world map. Position in whole tiles for now; the movement simulation will use
/// fixed point coordinates (ADR 0004).
struct Ship {
    std::string name;
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t figure = 0;  ///< FIGUR number (figuren.cod)
    std::uint8_t owner = MapCell::kNoOwner;
    std::uint8_t direction = 0;  ///< 0..7
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

    [[nodiscard]] const std::vector<Player>& players() const noexcept { return m_players; }
    [[nodiscard]] const std::vector<Ship>& ships() const noexcept { return m_ships; }
    /// Player of an owner number, nullptr for "nobody" or unknown slots.
    [[nodiscard]] const Player* player(std::uint8_t owner) const noexcept {
        return owner < m_players.size() ? &m_players[owner] : nullptr;
    }
    void set_players(std::vector<Player> players) { m_players = std::move(players); }
    void add_ship(Ship ship) { m_ships.push_back(std::move(ship)); }

private:
    std::vector<WorldIsland> m_islands;
    std::vector<Player> m_players;
    std::vector<Ship> m_ships;
    std::vector<Cell> m_cells;
};

/// Loads a scenario (.SZS/.SZM) or savegame (.GAM): all islands, each on its island template and
/// placed on the world map, the players and the ships. Problems are collected in `warnings`; islands that cannot be
/// placed are skipped.
core::Result<World> load_world(const assets::ChunkFile& file, const assets::BuildingTable& buildings,
                               const assets::GameDirectory& game, std::vector<std::string>* warnings = nullptr);

}  // namespace o1602::sim
