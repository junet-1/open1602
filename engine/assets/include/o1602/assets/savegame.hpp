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

/// Warehouse stock is stored in 1/32 t (confirmed in the running game: 800 = 25 t).
inline constexpr std::uint32_t kStockUnitsPerTon = 32;

/// One good in a warehouse. The original identifies goods by the first building that produces
/// them (table 0x00609160), stored as Id - 20000.
struct WareSlot {
    std::uint8_t ware = 0;               ///< ware number of haeuser.cod (2..24)
    std::uint16_t producer_id = 0;       ///< Id of the producing building (e.g. 20513), 0 = unknown
    std::array<std::uint32_t, 4> raw{};  ///< the slot's fields; raw[3] low 16 bits = stock

    /// Stock in 1/32 t (kStockUnitsPerTon); fixed point keeps the simulation exact (ADR 0004).
    [[nodiscard]] std::uint32_t stock() const noexcept { return raw[3] & 0xFFFFU; }
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
/// the state of one production building, at the origin tile of the building. The saver packs the
/// 20-byte runtime entry into these bytes; the decoded fields follow that packing
/// (docs/formats/gam.md, docs/re/production.md).
struct ProductionRecord {
    std::uint8_t island = 0;  ///< island number (INSEL5 header)
    std::uint8_t x = 0;       ///< origin tile of the building on the island
    std::uint8_t y = 0;
    bool enabled = true;                 ///< switched on by the player (+0x14 bit 0; runtime +3 bit 3)
    bool working = false;                ///< has raw material and is producing (+0x0F, 0x80; runtime +0x0E)
    std::uint8_t anim_phase = 0;         ///< animation offset (+0x14 bits 2-5; runtime +3 bits 4-7)
    std::uint8_t idle_cycles = 0;        ///< cycles without raw material, saturates at 15 (+0x15 bits 0-3)
    std::uint16_t stock = 0;             ///< goods produced and not yet collected, 1/32 t (+0x05)
    std::uint16_t progress = 0;          ///< progress of the current cycle, 512 = done (+0x10)
    std::array<std::uint8_t, 21> raw{};  ///< bytes 3..23 as stored; the remaining fields are still open
};

/// HIRSCH2 entry (8 bytes, up to 8 per island, island + 0x6C; written by the island saver
/// 0x00468740): a deer on the island.
struct AnimalRecord {
    std::uint8_t island = 0;
    std::uint8_t x = 0;  ///< island tile
    std::uint8_t y = 0;
    std::uint32_t timer = 0;  ///< milliseconds, probably until the next action
};

/// SOLDAT3 record (0x44 bytes, written by 0x004600C0 from the soldier table 0x0051C678,
/// 400 x 0x1B0, with the soldier's figure): one land unit. Positions are stored in half tiles
/// (the figure's float position doubled and rounded).
struct SoldierRecord {
    std::uint16_t x2 = 0;                  ///< world position in half tiles (+0x00, figure +0x28 * 2)
    std::uint16_t y2 = 0;                  ///< (+0x02, figure +0x2C * 2)
    std::uint16_t figure = 0;              ///< FIGUR number (+0x06): SOLDAT1..4, KAVALERIE1..4, ..., SPEER1
    std::uint8_t owner = 7;                ///< original owner slot (+0x18), 7 = nobody
    std::uint8_t anim = 0;                 ///< ANIM index of the figure (+0x19)
    std::uint8_t direction = 0;            ///< 0..7 (+0x1B)
    std::array<std::uint8_t, 0x44> raw{};  ///< the whole record; the remaining fields are still open
};

/// Everything above, read from one file. Files without a chunk give empty lists.
struct GameState {
    std::vector<CityRecord> cities;
    std::vector<WarehouseRecord> warehouses;
    std::vector<ProductionRecord> production;
    std::vector<AnimalRecord> animals;
    std::vector<SoldierRecord> soldiers;
};

core::Result<GameState> load_game_state(const ChunkFile& file);

}  // namespace o1602::assets
