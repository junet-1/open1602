// SPDX-License-Identifier: GPL-3.0-or-later
//
// Game state chunks of savegames and scenarios beyond islands, players and ships:
// cities (STADT4), warehouse stock (KONTOR2), production buildings (PRODLIST2) and the deer of the
// islands (HIRSCH2). Layouts follow the save routines of 1602.exe (docs/formats/gam.md); fields
// whose meaning is not yet known are kept as raw values so nothing is lost.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "o1602/assets/chunk_file.hpp"
#include "o1602/core/result.hpp"

namespace o1602::assets {

/// STADT4 record (0xA8 bytes, written by 0x00485020 from the city table at 0x005DBAE0, 75 x 0x258).
struct CityRecord {
    std::uint8_t island = 0;  ///< island number (INSEL5 header)
    std::uint8_t index = 0;   ///< city index on the island (island tile attribute bits 14-16)
    std::uint8_t owner = kNoCityOwner;
    std::string name;  ///< +0x87, Windows-1252, e.g. "Lemonhall"

    static constexpr std::uint8_t kNoCityOwner = 7;
};

/// One good in a warehouse. The original identifies goods by the first building that produces
/// them (table 0x00609160), stored as Id - 20000.
struct WareSlot {
    std::uint8_t ware = 0;               ///< ware number of haeuser.cod (2..24)
    std::uint16_t producer_id = 0;       ///< Id of the producing building (e.g. 20513), 0 = unknown
    std::array<std::uint32_t, 4> raw{};  ///< stock, prices and flags; meaning still open
};

/// KONTOR2 record (0x3EC bytes, same writer): the goods of one city's warehouses.
struct WarehouseRecord {
    std::uint8_t island = 0;
    std::uint8_t x = 0;  ///< island tile of the warehouse
    std::uint8_t y = 0;
    std::uint8_t city = 0;        ///< city index on the island
    std::vector<WareSlot> wares;  ///< 23 slots, goods 2..24
};

/// PRODLIST2 record (0x18 bytes, written by 0x00485530 from the list at 0x0054A3B8, 4144 x 0x14):
/// one production building.
struct ProductionRecord {
    std::uint8_t island = 0;
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::array<std::uint8_t, 21> raw{};  ///< timers, stock and state flags; meaning still open
};

/// HIRSCH2 entry (8 bytes, up to 8 per island, island + 0x6C; written by the island saver
/// 0x00468740): a deer on the island.
struct AnimalRecord {
    std::uint8_t island = 0;
    std::uint8_t x = 0;  ///< island tile
    std::uint8_t y = 0;
    std::uint32_t timer = 0;  ///< milliseconds, probably until the next action
};

/// Everything above, read from one file. Files without a chunk give empty lists.
struct GameState {
    std::vector<CityRecord> cities;
    std::vector<WarehouseRecord> warehouses;
    std::vector<ProductionRecord> production;
    std::vector<AnimalRecord> animals;
};

core::Result<GameState> load_game_state(const ChunkFile& file);

}  // namespace o1602::assets
