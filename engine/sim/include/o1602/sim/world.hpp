// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/assets/scenario.hpp"
#include "o1602/core/result.hpp"
#include "o1602/sim/island_map.hpp"
#include "o1602/sim/player.hpp"
#include "o1602/sim/production.hpp"

namespace o1602::sim {

struct WorldIsland {
    assets::IslandHeader header;
    IslandMap map;
    std::uint32_t x = 0;  ///< position of the island's tile (0,0) on the world map
    std::uint32_t y = 0;
};

/// A city: the settlement of one player on one island (STADT4 in the original).
struct City {
    std::uint16_t island = 0;  ///< index into World::islands()
    std::uint8_t index = 0;    ///< city number on the island (MapCell::city)
    PlayerId owner = kNoPlayer;
    std::string name;  ///< Windows-1252
};

/// A wild animal standing on an island (HIRSCH2 in the original: up to 8 per island).
struct Animal {
    std::uint16_t island = 0;  ///< index into World::islands()
    std::uint16_t x = 0;       ///< island tile
    std::uint16_t y = 0;
    std::uint16_t figure = 0;  ///< FIGUR number (HIRSCH, ANTILOPE)
};

/// A land unit (SOLDAT3 in the original). The position is continuous in world tiles; the original
/// saves it in half tiles.
struct Soldier {
    double x = 0.0;
    double y = 0.0;
    /// FIGUR number of the unit type. For units drawn per player colour this is the first figure
    /// of the family (SOLDAT1, KAVALERIE1, ...): the colour follows the owner's banner, so any number
    /// of players works (ADR 0007).
    std::uint16_t figure = 0;
    PlayerId owner = kNoPlayer;
    std::uint8_t direction = 0;  ///< 0..7
    std::uint8_t anim = 0;       ///< ANIM index when saved
};

/// A ship on the world map. Position in whole tiles for now; the movement simulation will use
/// fixed point coordinates (ADR 0004).
struct Ship {
    std::string name;
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t figure = 0;  ///< FIGUR number (figuren.cod)
    PlayerId owner = kNoPlayer;
    std::uint8_t direction = 0;  ///< 0..7
};

/// The world map: open sea with islands placed on it (the original keeps one 4-byte cell per
/// world tile: island number, local x, local y, colour table - see rendering.md). The size is a
/// property of the map; the original always uses 500 x 350 tiles and at most 255 islands
/// (L-003), Open1602 neither (ADR 0007).
class World {
public:
    static constexpr std::uint32_t kOriginalWidth = 500;
    static constexpr std::uint32_t kOriginalHeight = 350;
    static constexpr std::uint16_t kSea = 0xFFFF;

    explicit World(std::uint32_t width = kOriginalWidth, std::uint32_t height = kOriginalHeight);

    [[nodiscard]] std::uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] std::uint32_t height() const noexcept { return m_height; }
    [[nodiscard]] bool contains(std::uint32_t x, std::uint32_t y) const noexcept { return x < m_width && y < m_height; }

    /// What lies at a world tile.
    struct Cell {
        std::uint16_t island = kSea;  ///< index into islands(), kSea for open sea
        std::uint16_t local_x = 0;
        std::uint16_t local_y = 0;
    };

    [[nodiscard]] const Cell& cell(std::uint32_t x, std::uint32_t y) const {
        return m_cells[std::size_t{y} * m_width + x];
    }
    [[nodiscard]] const std::vector<WorldIsland>& islands() const noexcept { return m_islands; }

    /// Adds an island at its header position. Fails if it leaves the world or overlaps another one.
    core::Status add_island(WorldIsland island);

    [[nodiscard]] const std::vector<Player>& players() const noexcept { return m_players; }
    [[nodiscard]] const std::vector<Ship>& ships() const noexcept { return m_ships; }
    /// Player of an owner number, nullptr for "nobody" or unknown slots.
    [[nodiscard]] const Player* player(PlayerId owner) const noexcept {
        return owner < m_players.size() ? &m_players[owner] : nullptr;
    }
    void set_players(std::vector<Player> players) { m_players = std::move(players); }
    void add_ship(Ship ship) { m_ships.push_back(std::move(ship)); }

    [[nodiscard]] const std::vector<City>& cities() const noexcept { return m_cities; }
    /// City `index` on island `island` (index into islands()), nullptr if there is none.
    [[nodiscard]] const City* city(std::uint16_t island, std::uint8_t index) const noexcept;
    void add_city(City city) { m_cities.push_back(std::move(city)); }

    [[nodiscard]] const std::vector<Animal>& animals() const noexcept { return m_animals; }
    void add_animal(Animal animal) { m_animals.push_back(animal); }

    [[nodiscard]] const std::vector<Soldier>& soldiers() const noexcept { return m_soldiers; }
    void add_soldier(const Soldier& soldier) { m_soldiers.push_back(soldier); }

    [[nodiscard]] const std::vector<Production>& productions() const noexcept { return m_productions; }
    /// Mutable for the simulation; entries keep their index (other entries may refer to it).
    [[nodiscard]] std::vector<Production>& productions() noexcept { return m_productions; }
    /// Production state of the building whose origin tile is (x, y) on island `island` (index into
    /// islands()), nullptr if it has none. Constant time: the renderer asks for every visible
    /// production building in every frame.
    [[nodiscard]] const Production* production(std::uint16_t island, std::uint16_t x, std::uint16_t y) const;
    /// Adds or replaces the state of the building at (p.island, p.x, p.y).
    void set_production(const Production& p);

    /// Index into islands() of the island with this number (INSEL5 header), if it is placed.
    [[nodiscard]] std::optional<std::uint16_t> island_index(std::uint32_t number) const noexcept;

private:
    std::uint32_t m_width;
    std::uint32_t m_height;
    std::vector<WorldIsland> m_islands;
    std::vector<Player> m_players;
    std::vector<Ship> m_ships;
    std::vector<City> m_cities;
    std::vector<Animal> m_animals;
    std::vector<Soldier> m_soldiers;
    std::vector<Production> m_productions;
    std::unordered_map<std::uint64_t, std::uint32_t> m_production_index;  ///< key: island, y, x
    std::vector<Cell> m_cells;
};

/// The player at this computer until there is a lobby: the first human player, kNoPlayer if none.
[[nodiscard]] PlayerId first_human_player(const World& world) noexcept;

/// Loads a scenario (.SZS/.SZM) or savegame (.GAM): all islands, each on its island template and
/// placed on the world map, the players and the ships. Problems are collected in `warnings`; islands that cannot be
/// placed are skipped.
core::Result<World> load_world(const assets::ChunkFile& file, const assets::BuildingTable& buildings,
                               const assets::GameDirectory& game, std::vector<std::string>* warnings = nullptr);

}  // namespace o1602::sim
