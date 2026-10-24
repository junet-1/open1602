// SPDX-License-Identifier: GPL-3.0-or-later
//
// Values of the game's balance that a content package may change (MD-9, ADR 0007: a mod changes
// data instead of code; docs/modding/balance.md). The defaults are the original's. A game keeps
// the values it started with: they are saved with it (section BALA, only when they differ), so a
// running game and every computer of a multiplayer game play with the host's.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace o1602::sim {

struct Balance {
    std::uint32_t trader_max_held = 50;     ///< t of a ware the free trader holds at most (0x00454B80)
    std::uint32_t trader_max_buy = 10;      ///< t of a ware he buys at most in one visit
    std::uint32_t trader_restock_s = 60;    ///< seconds until he takes on tools or ore again (S-078)
    std::uint32_t castle_interval_s = 10;   ///< seconds of a castle's training step (11 when idle)
    std::uint32_t castle_unload_range = 6;  ///< tiles from a ship where soldiers land (0x00448D90)
    std::uint32_t fire_spread = 19;         ///< chance of 128 per epoch that a fire spreads
    std::uint32_t plague_radius = 4;        ///< tiles a plague reaches to infect a neighbour

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

/// Whether every value lies within its limits (a savegame's are checked with it).
[[nodiscard]] bool valid(const Balance& balance) noexcept;

/// The file of a content package with its changes to the balance.
inline constexpr std::string_view kBalanceFile = "balance.toml";

/// Changes `balance` value by value as the balance.toml of `package` (UTF-8) says. Problems go to
/// `warnings` with the file and the line; a value out of its limits or of a wrong type is passed
/// over, a file that is no valid data file as a whole.
void apply_balance_file(Balance& balance, std::string_view package, std::string_view utf8,
                        std::vector<std::string>& warnings);

}  // namespace o1602::sim
