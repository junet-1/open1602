// SPDX-License-Identifier: GPL-3.0-or-later
//
// The settings of a new endless game (ADR 0008): the rules of the game and, if wanted, a world of
// its own instead of the original's maps. The start dialog shows them as items with a few values
// each, a click choosing the next; settings.cfg keeps the last choice ("endless.*" lines).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "o1602/sim/rules.hpp"
#include "o1602/sim/world_plan.hpp"

namespace o1602::app {

struct EndlessSetup {
    sim::Rules rules = sim::Rules::open1602();
    bool own_world = false;      ///< a world of its own (`world`) instead of the level's map
    sim::WorldSettings world{};  ///< size, players, islands, villages, volcanoes, starts (seed and level aside)

    /// The original's rules on the original's maps.
    [[nodiscard]] static EndlessSetup original();

    friend bool operator==(const EndlessSetup& a, const EndlessSetup& b) noexcept;
};

/// The items of the start dialog, in its order.
enum class SetupItem : std::uint8_t {
    // Economy
    Refund,
    TraderPrices,
    // Limits
    Ships,
    Units,
    GameSpeed,
    // Disasters
    Droughts,
    Volcanoes,
    // World
    Map,
    WorldSize,
    Players,
    Islands,
    IslandSizes,
    Villages,
    VolcanoIslands,
    Starts,
    Resources,
    Count,
};

inline constexpr std::size_t kSetupItems = static_cast<std::size_t>(SetupItem::Count);

/// How many values an item offers.
[[nodiscard]] std::size_t setup_choices(SetupItem item) noexcept;
/// The value the setup has, or the nearest one an item offers (a setting from settings.cfg or the
/// command line may lie between them).
[[nodiscard]] std::size_t setup_choice(const EndlessSetup& setup, SetupItem item) noexcept;
/// Sets an item to one of its values (an index beyond them is ignored).
void choose(EndlessSetup& setup, SetupItem item, std::size_t choice);
/// The next value, after the last the first.
void choose_next(EndlessSetup& setup, SetupItem item);
/// Whether the item counts: the world's items only for a world of its own.
[[nodiscard]] bool setup_item_used(const EndlessSetup& setup, SetupItem item) noexcept;

/// The item's name and a value's text in the language of the interface (Windows-1252).
[[nodiscard]] std::string setup_label(SetupItem item);
[[nodiscard]] std::string setup_value(SetupItem item, std::size_t choice);

/// One "endless.<key> = value" line of settings.cfg (key without the prefix); false if the key is
/// not one of them. A value it does not know keeps the setting.
bool read_setup_line(EndlessSetup& setup, std::string_view key, std::string_view value);
/// The lines for settings.cfg.
[[nodiscard]] std::string write_setup(const EndlessSetup& setup);

}  // namespace o1602::app
