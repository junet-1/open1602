// SPDX-License-Identifier: GPL-3.0-or-later
//
// Settings of a game (ADR 0008): points where Open1602 lets the players choose, like the
// configurable endless games of later Anno titles, and points that differ between Open1602 and the
// original (ADR 0006, docs/re/bugs.md).
//
// Only points that change balance or feel are switchable. Real bugs and exploits are always fixed
// and have no switch here. Rules are part of the game state and of a multiplayer session, so all
// peers simulate with the same values.
#pragma once

#include <cstdint>

namespace o1602::sim {

/// What a player gets back when he demolishes a building.
enum class Refund : std::uint8_t {
    None,     ///< nothing, as in the original (0x00464B40)
    Partial,  ///< Rules::refund_percent of the building's costs
    Full,     ///< all the building's costs
};

/// How often droughts come (docs/re/growth.md#dürre). The original takes it from the difficulty of
/// the game; the rows are its drought tables.
enum class Droughts : std::uint8_t {
    Game,      ///< as the game's difficulty and settings say (original)
    Off,       ///< never (difficulty "easy", or the game setting "no droughts")
    Rare,      ///< 1 in 32 (the original's "average", by a stray byte before its table, S-043)
    Normal,    ///< 8 in 32, 10 minutes ("demanding")
    Frequent,  ///< 10 in 32, 15 minutes ("hard")
};

/// Whether volcanoes erupt (docs/re/volcanoes.md). The original has a game setting for it.
enum class Volcanoes : std::uint8_t {
    Game,  ///< as the game's settings say (original)
    Off,   ///< never
    On,    ///< always, whatever the game says
};

/// What the free trader pays for goods (docs/re/ships.md, "Freier Händler").
enum class TraderPrices : std::uint8_t {
    /// As players know it: about half the base price. The original means to follow the market but
    /// gathers its statistics at a rate that depends on the frame rate, so they are almost always
    /// empty (S-077).
    Original,
    /// What the original intends: three quarters of the average price Kontors pay for the ware;
    /// he stops buying a ware all traders together hold half again as much of as Kontors want.
    Market,
};

struct Rules {
    /// Refund of the build costs (gold and goods) when the player demolishes a building. The goods
    /// go back into the warehouse of the building's city as far as there is room.
    Refund demolish_refund = Refund::None;
    /// Share for Refund::Partial, in percent (1..100). 50 by default: a wrong building costs half
    /// its price, moving one stays affordable.
    std::uint8_t refund_percent = 50;

    /// L-001: ships per player. 0 = unlimited; the original hard limit is 33.
    std::uint32_t max_ships_per_player = 0;
    /// L-008: units per player (on land, in castles, on board). 0 = unlimited; the original allows 100.
    std::uint32_t max_army = 0;

    /// S-019: random worlds contain at least one deposit of every resource that a building needs
    /// (the original can generate endless games without gold).
    bool guarantee_resources = true;

    /// L-004: highest game speed factor (original: 4, with an unintended 8 via Shift+F8).
    std::uint32_t max_game_speed = 8;

    /// Droughts: as the game says, or chosen for the endless game.
    Droughts droughts = Droughts::Game;
    /// Volcanoes: as the game says, or switched on or off.
    Volcanoes volcanoes = Volcanoes::Game;
    /// Prices of the free trader.
    TraderPrices trader_prices = TraderPrices::Original;

    /// Defaults: corrected behaviour.
    [[nodiscard]] static constexpr Rules open1602() noexcept { return Rules{}; }

    /// Behaviour of the original game where it is switchable.
    [[nodiscard]] static constexpr Rules original() noexcept {
        Rules r;
        r.max_ships_per_player = 33;
        r.max_army = 100;
        r.guarantee_resources = false;
        r.max_game_speed = 4;
        r.demolish_refund = Refund::None;
        r.droughts = Droughts::Game;
        r.volcanoes = Volcanoes::Game;
        r.trader_prices = TraderPrices::Original;
        return r;
    }

    /// Share of the build costs a demolition gives back, in percent.
    [[nodiscard]] constexpr std::uint32_t refund_share() const noexcept {
        switch (demolish_refund) {
            case Refund::Full:
                return 100;
            case Refund::Partial:
                return refund_percent > 100 ? 100U : refund_percent;
            case Refund::None:
                break;
        }
        return 0;
    }

    friend constexpr bool operator==(const Rules&, const Rules&) = default;
};

}  // namespace o1602::sim
