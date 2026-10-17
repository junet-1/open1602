// SPDX-License-Identifier: GPL-3.0-or-later
//
// Game state chunks of savegames and scenarios beyond islands, players and ships:
// cities (STADT4), warehouse stock (KONTOR2), production buildings (PRODLIST2) and the deer of the
// islands (HIRSCH2). Layouts follow the save routines of 1602.exe (docs/formats/gam.md); fields
// whose meaning is not yet known are kept as raw values so nothing is lost.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
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
    std::string name;                            ///< +0x87, Windows-1252, e.g. "Lemonhall"
    std::array<std::uint32_t, 5> inhabitants{};  ///< +0x5C: per class (pioneers .. aristocrats)
    std::array<std::uint8_t, 5> satisfaction{};  ///< +0x78: per class, 128 = content
    std::array<std::uint8_t, 5> tax{};           ///< +0x7F: tax rate per class, 128 = normal
    std::uint8_t food_ratio = 0;                 ///< +0x86: supply of food, 128 = full
    /// +0x18: goodwill of natives towards each player slot (city + 0x206; 49 per entry at the start).
    std::array<std::uint32_t, 7> goodwill{};
    bool material_lock = false;  ///< +0x04 bit 0: no building material for houses

    static constexpr std::uint8_t kNoCityOwner = 7;
};

/// SIEDLER record (16 bytes, writer 0x00485530, reader 0x00483EE0): one house and its inhabitants
/// (docs/re/houses.md).
struct HouseRecord {
    std::uint8_t island = 0;  ///< island number
    std::uint8_t x = 0;       ///< origin tile
    std::uint8_t y = 0;
    std::uint8_t epoch = 0;            ///< house tick epoch (3 bits)
    std::uint8_t queue_epoch = 0;      ///< city epoch when it queued for the next class (3 bits)
    std::uint8_t resident_class = 0;   ///< saved class (the loader takes the class of the building)
    std::uint8_t market_distance = 6;  ///< distance class to the nearest market, 6 = none
    std::uint16_t inhabitants64 = 0;   ///< inhabitants * 64
    std::uint8_t timer_group = 0;      ///< 0..15
    bool services_complete = false;    ///< has the public buildings its class needs
    bool market = false;               ///< a market is in range
    /// Public buildings in range, bit 2.. of the runtime word: Kapelle, Kirche, Wirtshaus, Schule,
    /// Badehaus, Theater, Hochschule, Klinik, Kontor (bits 2..10).
    std::uint16_t services = 0;
    std::array<std::uint8_t, 16> raw{};
};

/// MARKT2 record (0x104 bytes, writer 0x00485020): the consumption accounting of one city
/// (docs/re/consumption.md).
struct ConsumptionRecord {
    std::uint8_t island = 0;
    std::uint8_t city = 0;
    struct Good {
        std::uint16_t producer_id = 0;  ///< the good, as the Id of its first producer
        std::uint32_t demand = 0;       ///< A, 1/8192 t
        std::uint32_t supplied = 0;     ///< B, 1/8192 t
        std::uint8_t ratio = 0;         ///< C, 0..128
    };
    std::vector<Good> goods;
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
    /// Trade settings (city ware slot +8, docs/re/ships.md): prices per ton, whether the Kontor
    /// sells or buys the ware, the stock it keeps when selling and the stock it buys up to.
    [[nodiscard]] std::uint16_t sell_price() const noexcept { return static_cast<std::uint16_t>(raw[0] & 0x3FFU); }
    [[nodiscard]] std::uint16_t buy_price() const noexcept {
        return static_cast<std::uint16_t>((raw[0] >> 10) & 0x3FFU);
    }
    [[nodiscard]] bool selling() const noexcept { return (raw[0] >> 20 & 1U) != 0; }
    [[nodiscard]] bool buying() const noexcept { return (raw[0] >> 21 & 1U) != 0; }
    [[nodiscard]] std::uint32_t keep() const noexcept { return raw[2] & 0xFFFFU; }
    [[nodiscard]] std::uint32_t limit() const noexcept { return raw[2] >> 16; }
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
    bool enabled = true;                       ///< switched on by the player (+0x14 bit 0; runtime +3 bit 3)
    std::uint8_t anim_phase = 0;               ///< animation offset (+0x14 bits 2-5; runtime +3 bits 4-7)
    std::uint16_t stock = 0;                   ///< produced goods waiting for a carrier, 1/32 t (+0x05; runtime +0x0C)
    std::uint16_t countdown = 0;               ///< game seconds until the next cycle (+0x08; runtime +0x04)
    std::uint16_t raw2 = 0;                    ///< second raw material (Workstoff), 1/32 t (+0x0A; runtime +0x08)
    std::uint16_t raw = 0;                     ///< raw material (Rohstoff), 1/32 t (+0x0C; runtime +0x0A)
    std::uint8_t rate = 0;                     ///< working rate of the current cycle, 128 = full (+0x0F; runtime +0x0E)
    std::uint16_t produced = 0;                ///< goods produced in the statistics window, 1/32 t (+0x10)
    std::uint16_t elapsed = 0;                 ///< length of the statistics window, game seconds (+0x12)
    std::uint8_t idle_cycles = 0;              ///< cycles without raw material, saturates at 15 (+0x15 bits 0-3)
    std::uint8_t flags = 0;                    ///< runtime +0x0F bits 4 and 6 (+0x14 bits 6 and 1), meaning open
    std::array<std::uint8_t, 21> raw_bytes{};  ///< bytes 3..23 as stored
};

/// HIRSCH2 entry (8 bytes, up to 8 per island, island + 0x6C; written by the island saver
/// 0x00468740): a deer on the island.
struct AnimalRecord {
    std::uint8_t island = 0;
    std::uint8_t x = 0;  ///< island tile
    std::uint8_t y = 0;
    /// Game time (1/10 s, the clock of TIMERS +0xA0) before which the spot gets no new deer
    /// (docs/re/deer.md).
    std::uint32_t timer = 0;
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
/// ROHWACHS2 record (8 bytes, writer 0x00485C20): a growing field, tree, meadow or fish shoal.
struct GrowthRecord {
    std::uint8_t island = 0;  ///< island number
    std::uint8_t x = 0;       ///< island tile
    std::uint8_t y = 0;
    std::uint8_t growth_class = 0;  ///< 0..31: one stage lasts 40 s + 7 s * class
    std::uint8_t seen = 0;          ///< class counter (0..7) when the entry last grew
    std::uint8_t stage = 0;         ///< growth stage, 0..AnimAnz of the growing record
};

/// The growth clocks of TIMERS (0x00485D10): one per growth class.
struct GrowthClocks {
    std::array<std::uint8_t, 32> counter{};   ///< +0x64: class counter, counts 0..7
    std::array<std::uint32_t, 32> time_ms{};  ///< +0x218: game ms since the class last ticked
};

/// The house clocks of TIMERS: 16 timer groups (docs/re/houses.md).
struct HouseClocks {
    std::array<std::uint8_t, 16> epoch{};     ///< +0x44: epoch of each group (3 bits)
    std::array<std::uint32_t, 16> time_ms{};  ///< +0x198: game ms since the group last ticked
};

/// A building with running costs outside the production list: shipyards (WERFT, 0x18 bytes, on
/// = dword0 bit 27) and castles (MILITAR, 0x70 bytes).
struct CostRecord {
    std::uint8_t island = 0;
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    bool enabled = true;
    /// Shipyards (WERFT, docs/re/ships.md): +6 ship type ordered (0xFF none), +8 build points,
    /// +0x0A wood in the yard (1/32 t), +4 seconds to the end of the cycle, +0x0C ship slot being
    /// repaired (0xFFFF none).
    std::uint8_t order = 0xFF;
    std::uint16_t progress = 0;
    std::uint16_t wood = 0;
    std::uint16_t countdown = 0;
    std::uint16_t repair = 0xFFFF;
};

struct GameState {
    std::vector<CityRecord> cities;
    std::vector<WarehouseRecord> warehouses;
    std::vector<ProductionRecord> production;
    std::vector<AnimalRecord> animals;
    std::vector<SoldierRecord> soldiers;
    std::vector<GrowthRecord> growth;
    std::vector<ConsumptionRecord> consumption;
    std::vector<HouseRecord> houses;
    std::optional<HouseClocks> house_clocks;
    std::optional<std::uint32_t> game_time;  ///< TIMERS +0xA0: game time in 1/10 s
    /// TIMERS +0x01: phase of the island epochs (0..7, one step every 30 s, 0x005DBAB0).
    std::optional<std::uint8_t> island_phase;
    /// TIMERS +0xB0: next name of the [STAEDTE] list of Text.cod a new city gets (0x005B3168).
    std::optional<std::uint32_t> city_name_index;
    std::optional<std::uint8_t> difficulty;     ///< TIMERS +0xA6: 0 easy .. 3 hard (droughts)
    std::optional<std::uint32_t> settings;      ///< TIMERS +0xA8: game settings word, bit 2 no droughts
    std::optional<std::uint32_t> next_drought;  ///< TIMERS +0xB4: earliest drought roll, 1/10 s
    /// A volcano crater as the original refers to it (an object ref: type 0x33, island number, x, y).
    struct CraterRef {
        std::uint8_t island = 0;  ///< island number (INSEL5 header)
        std::uint8_t x = 0;       ///< origin of the volcano mountain
        std::uint8_t y = 0;
    };
    std::optional<CraterRef> running_volcano;   ///< TIMERS +0xC4: the eruption that runs
    std::optional<CraterRef> last_volcano;      ///< TIMERS +0xC8: the last eruption
    std::optional<std::uint32_t> next_volcano;  ///< TIMERS +0xCC: earliest eruption, 1/10 s
    std::optional<std::uint32_t> crater_count;  ///< TIMERS +0xD0: craters in the world, at most 20
    std::vector<CostRecord> shipyards;
    std::vector<CostRecord> castles;
    std::optional<GrowthClocks> growth_clocks;  ///< nothing without a TIMERS chunk
};

core::Result<GameState> load_game_state(const ChunkFile& file);

}  // namespace o1602::assets
