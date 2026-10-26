// SPDX-License-Identifier: GPL-3.0-or-later
//
// Values of the game's balance that a content package may change (MD-9, ADR 0007: a mod changes
// data instead of code; docs/modding/balance.md). The defaults are the original's. A game keeps
// the values it started with: they are saved with it (section BALA, only when they differ), so a
// running game and every computer of a multiplayer game play with the host's.
//
// The same file changes values of the original's buildings (production, running and build costs):
// a game keeps them in the section BLDV and plays with a building table changed by them
// (apply_building_values).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace o1602::assets {
class BuildingTable;
}

namespace o1602::sim {

struct Rules;

/// A value of a building that a package may change (balance.toml, a table ["anno1602:<Id>"]).
enum class BuildingField : std::uint8_t {
    Produce,      ///< Prodmenge: t made per cycle
    Raw,          ///< Rohmenge: t of raw material per cycle
    Raw2,         ///< Workmenge: t of the second raw material per cycle
    Interval,     ///< Interval: game seconds per cycle
    MaxStock,     ///< Maxlager: t the building holds until it is collected
    RunningCost,  ///< Kosten: gold per minute while switched on
    IdleCost,     ///< Kosten, second value: gold per minute while switched off
    BuildMoney,   ///< HAUS_BAUKOST Money
    BuildTools,   ///< HAUS_BAUKOST Werkzeug, t
    BuildWood,    ///< HAUS_BAUKOST Holz, t
    BuildBricks,  ///< HAUS_BAUKOST Ziegel, t
};

/// A changed value of a building, in the units of balance.toml (tons, seconds, gold).
struct BuildingValue {
    std::uint32_t id = 0;  ///< HAUS Id of haeuser.cod
    BuildingField field = BuildingField::Produce;
    std::uint32_t value = 0;

    friend bool operator==(const BuildingValue&, const BuildingValue&) = default;
};

struct Balance {
    std::uint32_t trader_max_held = 50;     ///< t of a ware the free trader holds at most (0x00454B80)
    std::uint32_t trader_max_buy = 10;      ///< t of a ware he buys at most in one visit
    std::uint32_t trader_restock_s = 60;    ///< seconds until he takes on tools or ore again (S-078)
    std::uint32_t castle_interval_s = 10;   ///< seconds of a castle's training step (11 when idle)
    std::uint32_t castle_unload_range = 6;  ///< tiles from a ship where soldiers land (0x00448D90)
    std::uint32_t fire_spread = 19;         ///< chance of 128 per epoch that a fire spreads
    std::uint32_t plague_radius = 4;        ///< tiles a plague reaches to infect a neighbour
    /// Changed values of buildings, sorted by id and field, each at most once.
    std::vector<BuildingValue> buildings;

    friend bool operator==(const Balance&, const Balance&) = default;
};

/// One value of Balance as balance.toml names it ([table] key) and its limits.
struct BalanceField {
    std::string_view table;
    std::string_view key;
    std::uint32_t Balance::*value;
    std::uint32_t min;
    std::uint32_t max;
};

/// Every value, in the order of the section BALA.
inline constexpr std::array<BalanceField, 7> kBalanceFields = {{
    {"trader", "max_held", &Balance::trader_max_held, 1, 200},
    {"trader", "max_buy", &Balance::trader_max_buy, 1, 200},
    {"trader", "restock_seconds", &Balance::trader_restock_s, 1, 3600},
    {"castle", "interval_seconds", &Balance::castle_interval_s, 1, 600},
    {"castle", "unload_range", &Balance::castle_unload_range, 1, 16},
    {"disasters", "fire_spread", &Balance::fire_spread, 0, 128},
    {"disasters", "plague_radius", &Balance::plague_radius, 0, 16},
}};

/// A field of a building as balance.toml names it and its limits there.
struct BuildingFieldInfo {
    std::string_view key;
    BuildingField field;
    std::uint32_t min;
    std::uint32_t max;
};

/// Every field of a building, in the order of BuildingField.
inline constexpr std::array<BuildingFieldInfo, 11> kBuildingFields = {{
    {"produce", BuildingField::Produce, 1, 64},
    {"raw", BuildingField::Raw, 1, 64},
    {"raw2", BuildingField::Raw2, 1, 64},
    {"interval", BuildingField::Interval, 1, 3600},
    {"max_stock", BuildingField::MaxStock, 1, 255},
    {"running_cost", BuildingField::RunningCost, 0, 1000},
    {"idle_cost", BuildingField::IdleCost, 0, 1000},
    {"cost_money", BuildingField::BuildMoney, 0, 100000},
    {"cost_tools", BuildingField::BuildTools, 0, 255},
    {"cost_wood", BuildingField::BuildWood, 0, 255},
    {"cost_bricks", BuildingField::BuildBricks, 0, 255},
}};

/// Whether every value lies within its limits and the building values are sorted and each given
/// once (a savegame's are checked with it).
[[nodiscard]] bool valid(const Balance& balance) noexcept;

/// Whether the values of kBalanceFields are the original's (the building values not counted).
[[nodiscard]] bool original_numbers(const Balance& balance) noexcept;

/// Sets one value of a building, replacing the one given before.
void set_building_value(Balance& balance, BuildingValue value);

/// Adds the values of `under` that `balance` does not set itself (a rule's values under the mods').
void add_building_values(Balance& balance, const std::vector<BuildingValue>& under);

/// The file of a content package with its changes to the balance.
inline constexpr std::string_view kBalanceFile = "balance.toml";

/// Changes `balance` value by value as the balance.toml of `package` (UTF-8) says. Problems go to
/// `warnings` with the file and the line; a value out of its limits or of a wrong type is passed
/// over, a file that is no valid data file as a whole.
void apply_balance_file(Balance& balance, std::string_view package, std::string_view utf8,
                        std::vector<std::string>& warnings);

/// Drops the values of buildings `buildings` does not have, with a warning each.
void drop_unknown_buildings(Balance& balance, const assets::BuildingTable& buildings,
                            std::vector<std::string>& warnings);

/// Makes `table` the building table of a game with `balance`: every value a package may change is
/// first taken back from `original` (the same table as loaded), then the game's values are set.
/// The types keep their places, so pointers into the table stay valid.
void apply_building_values(assets::BuildingTable& table, const assets::BuildingTable& original, const Balance& balance);

/// The building values a rule brings (Rules::revised_bread: data/balance/revised_bread.toml,
/// compiled in), to go under those of the packages.
[[nodiscard]] std::vector<BuildingValue> rule_building_values(const Rules& rules);

}  // namespace o1602::sim
