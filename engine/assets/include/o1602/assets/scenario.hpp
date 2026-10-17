// SPDX-License-Identifier: GPL-3.0-or-later
//
// Players and ships of scenarios (.szs/.szm) and savegames (.gam). Both file types use the same
// chunk set (one chunk name table in 1602.exe at 0x00498244); see docs/formats/gam.md.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "o1602/assets/chunk_file.hpp"
#include "o1602/core/result.hpp"

namespace o1602::assets {

/// Controller of a player slot (PLAYER4 byte +4).
enum class PlayerKind : std::uint8_t {
    Human = 0x00,
    Natives = 0x0B,
    Computer = 0x0C,
    Trader = 0x0D,  ///< free trader
    Pirate = 0x0E,
};

/// Colour of a player (PLAYER4 byte +7). Selects the flag figure (table at 0x00498F2C):
/// 0..3 FAHNE1..FAHNE4 (red, blue, yellow, white), 4 FAHNEPIRAT; 5 and 6 have no flag.
enum class PlayerColor : std::uint8_t {
    Red = 0,
    Blue = 1,
    Yellow = 2,
    White = 3,
    Pirate = 4,
    Natives = 5,
    Trader = 6,
};
inline constexpr std::uint8_t kPlayerColorCount = 7;

/// One player slot. The original stores 7 slots of 0x430 bytes: four players (human or
/// computer), the free trader, the pirates and the natives.
struct PlayerRecord {
    std::int32_t money = 0;
    PlayerKind kind = PlayerKind::Human;
    std::uint8_t slot = 0;  ///< index of the record, also used as owner number on tiles and ships
    PlayerColor color = PlayerColor::Red;
    std::string name;  ///< +0x3C0, Windows-1252; empty in scenarios, the human's name in savegames
    /// Unlocked build levels: bit n - 1 = INFRA number n (+0x34; runtime player +0x6C, set by the
    /// city tick 0x0047F8A0 and never cleared).
    std::uint32_t infra_mask = 0;
    std::uint8_t castles_built = 0;       ///< +0x0E (runtime +0x87): Schloss built, at most one
    std::uint8_t cathedrals_built = 0;    ///< +0x0F (+0x88): Kathedrale built, at most one
    std::uint16_t monuments_allowed = 0;  ///< +0x38 (+0x8A): Denkmal allowances
    std::uint16_t monuments_built = 0;    ///< +0x3A (+0x8C)
    std::uint16_t arches_allowed = 0;     ///< +0x10 (+0x8E): Triumphbogen allowances
    std::uint16_t arches_built = 0;       ///< +0x12 (+0x90)
};

/// Owner value meaning "nobody" (island tile attribute bits 22..25, ship byte +0x4B).
inline constexpr std::uint8_t kNoOwner = 7;
/// Number of PLAYER4 records in the original files (4 players, free trader, pirates, natives).
/// Only the file format is limited to this; the engine is not (ADR 0007).
inline constexpr std::uint8_t kOriginalPlayerSlots = 7;

/// One ship (SHIP4 record, 0x1B4 bytes; written by 0x004600C0 from the ship table at 0x004CF358,
/// 150 x 0x218). Position = the display figure's floating point position, truncated.
struct ShipRecord {
    std::string name;
    std::uint16_t x = 0;  ///< world tile
    std::uint16_t y = 0;
    std::uint16_t figure = 0;  ///< FIGUR number in figuren.cod (HANDEL1 = 21, PIRAT = 31, ...)
    std::uint8_t owner = kNoOwner;
    std::uint8_t direction = 0;  ///< +0x50: byte 3 of the display figure, 0..7 in 45 degree steps
    /// +0x4D: island at the ship's position (0x0046A570, e.g. lying at a warehouse), 0xFF = open sea.
    std::uint8_t island = 0xFF;
    /// +0x174: eight cargo slots of 8 bytes: the good as the Id of the building that produces it
    /// (like KONTOR2) and the amount in 1/32 t. Empty slots have producer 0.
    struct Cargo {
        std::uint32_t producer = 0;  ///< haeuser.cod Id (20000 + stored value), 0 = empty
        std::uint16_t amount = 0;
    };
    std::array<Cargo, 8> cargo{};
};

/// PLAYER4 chunk. Files without it (older formats PLAYER2/PLAYER3) yield an empty list.
core::Result<std::vector<PlayerRecord>> load_players(const ChunkFile& file);

/// SHIP4 chunk. Files without it yield an empty list.
core::Result<std::vector<ShipRecord>> load_ships(const ChunkFile& file);

/// The world plan of a scenario that makes its islands at the start (SZENE chunk, 0x494 bytes; the
/// endless games). For each island its number, place, size class and climate; the template, which
/// islands get a native village, the crops and the ore and gold deposits are drawn when the game
/// starts (docs/re/worldgen.md).
struct ScenarioPlan {
    static constexpr std::uint16_t kRandomTemplate = 0xFFFF;
    static constexpr std::uint8_t kClimateByPosition = 2;
    struct Island {
        std::uint8_t climate = 0;  ///< 0 north, 1 south, 2 by position on the map
        std::uint8_t size = 0;     ///< 0 lit, 1 mit, 2 med, 3 big, 4 lar
        std::uint8_t flags = 0;    ///< bit 0: native village (set by the generator)
        std::uint8_t number = 0;   ///< island number
        std::uint16_t template_number = kRandomTemplate;
        std::uint32_t x = 0;  ///< world tile of the top left corner
        std::uint32_t y = 0;
    };
    /// An entry of the ore, crop or treasure list: a haeuser.cod building whose Ware names what
    /// is placed, how many times, and (ore) 0 big vein, 1 small vein.
    struct Resource {
        std::uint32_t building = 0;  ///< haeuser.cod Id
        std::uint16_t count = 0;
        std::uint32_t type = 0;
    };
    std::array<std::uint32_t, 2> natives{};  ///< native villages to place in the north, the south
    std::uint32_t max_crops = 0;             ///< crops per island in the main passes, 0 = 2
    std::uint32_t gold_min_size = 0;         ///< size classes for gold in the first passes
    std::uint32_t gold_max_size = 0;
    std::array<Resource, 4> ores{};
    std::array<Resource, 12> crops{};
    std::array<Resource, 4> treasures{};
    std::vector<Island> islands;
};

/// The SZENE chunk, nothing if the file has none.
[[nodiscard]] core::Result<std::optional<ScenarioPlan>> load_scenario_plan(const ChunkFile& file);

/// RANDTAB: the 512 random numbers the world plan draws from (a saved rand() table), nothing if
/// the file has none. A shorter chunk repeats, as the original copies it.
[[nodiscard]] std::optional<std::array<std::uint16_t, 512>> load_random_table(const ChunkFile& file);

/// SZENE_MISSNR: 1 marks the endless game of the single player menu, 0 that of the multiplayer
/// menu; nothing if the file has none.
[[nodiscard]] std::optional<std::uint32_t> load_mission_number(const ChunkFile& file);

/// A savegame slot as listed in Game.dat ("Objekt: SPIELNAME", "Name: slot, \"text\", flag").
/// Slot n is stored as SAVEGAME/gameNN.gam.
struct SavegameSlot {
    std::uint32_t slot = 0;
    std::string name;  ///< Windows-1252
    std::string file;  ///< relative path, e.g. "Savegame/game00.gam"
};

/// Savegame names from the text file Game.dat (settings and savegame names of the original).
/// Unused slots carry names like "Leer01" and are returned as well.
std::vector<SavegameSlot> parse_savegame_slots(std::string_view game_dat);

}  // namespace o1602::assets
