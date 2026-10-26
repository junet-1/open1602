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

/// How often droughts come (docs/re/growth.md#drought). The original takes it from the difficulty of
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

/// Whether pirates come (docs/re/pirates.md). The original has a game setting for it, which the
/// endless games of the two easy levels set: there they never come.
enum class Pirates : std::uint8_t {
    Game,  ///< as the game's settings say (original)
    Off,   ///< never
    On,    ///< always, whatever the game says
};

/// How many nests the pirates found (docs/re/pirates.md). The original has one for the whole world,
/// which suits its four players; with more players most of them would never meet a pirate.
enum class PirateNests : std::uint8_t {
    One,     ///< one nest, as in the original
    Scaled,  ///< as the pirates' faction grows them (one for every four players begun), spread over the map
};

/// Which towns players see from the start of a game, without a ship having come near their island
/// (docs/re/ships.md#sailing). An island counts as discovered for every player as soon as it
/// holds a city the setting names, also one founded later; what else is on it shows with it.
enum class Reveal : std::uint8_t {
    None,     ///< nothing: every island is discovered by ship, as in the original
    Players,  ///< the cities of human and computer players; native villages and pirate nests by ship
    All,      ///< the cities of human and computer players, native villages and pirate nests
};

/// What the free trader pays for goods (docs/re/ships.md, "Free trader").
enum class TraderPrices : std::uint8_t {
    /// As players know it: about half the base price. The original means to follow the market but
    /// gathers its statistics at a rate that depends on the frame rate, so they are almost always
    /// empty (S-077).
    Original,
    /// What the original intends: three quarters of the average price Kontors pay for the ware;
    /// he stops buying a ware all traders together hold half again as much of as Kontors want.
    Market,
};

/// How far the free trader looks for his next Kontor (docs/re/ships.md, "Free trader"). The
/// original weighs the 12 nearest Kontors by profit alone, whatever the distance; on the larger maps
/// of more than four players he then mostly sails across the world.
enum class TraderReach : std::uint8_t {
    Original,  ///< the 12 nearest Kontors, a random one when none promises profit
    Scaled,    ///< with more than four players (the faction's reach) only as many as four would have
};

/// The prestige buildings a condition asks for.
enum class Monument : std::uint8_t {
    None,
    Castle,     ///< a castle (Schloss)
    Cathedral,  ///< a cathedral
    Both,       ///< a castle and a cathedral
};

/// The conditions of a victory; 0 (or None, false) switches one off.
struct VictoryConditions {
    /// Einwohner of all own cities, as the city list counts them.
    std::uint32_t inhabitants = 0;
    /// With `class_inhabitants`: residents of `resident_class` (0 pioneers .. 4 aristocrats) or a
    /// higher class in all own cities.
    std::uint8_t resident_class = 4;
    std::uint32_t class_inhabitants = 0;
    /// Gold in the treasury.
    std::uint32_t money = 0;
    /// Balance per minute as the status bar shows it.
    std::uint32_t balance = 0;
    /// Every human and computer opponent defeated.
    bool eliminate = false;
    /// Share of all islands with an own city, in percent (1..100).
    std::uint8_t islands_percent = 0;
    /// Points of the score sheet.
    std::uint32_t score = 0;
    /// Prestige buildings built.
    Monument monument = Monument::None;
    /// Minutes of game time; then the player with the most points wins.
    std::uint32_t time_limit_minutes = 0;
    /// All conditions at the same time, or any one of them.
    bool all = true;
    /// Whether computer players can win too (they are measured by the same conditions).
    bool computers = true;

    /// Whether a condition other than the time limit is on.
    [[nodiscard]] constexpr bool goals() const noexcept {
        return inhabitants != 0 || class_inhabitants != 0 || money != 0 || balance != 0 || eliminate ||
               islands_percent != 0 || score != 0 || monument != Monument::None;
    }
    /// Whether the game can end by a win at all.
    [[nodiscard]] constexpr bool active() const noexcept { return goals() || time_limit_minutes != 0; }

    friend constexpr bool operator==(const VictoryConditions&, const VictoryConditions&) = default;
};

struct Rules {
    /// Refund of the build costs (gold and goods) when the player demolishes a building. The goods
    /// go back into the warehouse of the building's city as far as there is room.
    Refund demolish_refund = Refund::None;
    /// Share for Refund::Partial, in percent (1..100). 50 by default: a wrong building costs half
    /// its price, moving one stays affordable.
    std::uint8_t refund_percent = 50;

    /// L-001: ships per player, shipyard orders included. 0 = unlimited; the original allows 33,
    /// Open1602 100.
    std::uint32_t max_ships_per_player = 100;
    /// L-008: units per player (on land, in castles, on board). 0 = unlimited; the original allows 100,
    /// Open1602 250.
    std::uint32_t max_army = 250;

    /// S-019: random worlds contain at least one deposit of every resource that a building needs
    /// (the original can generate endless games without gold).
    bool guarantee_resources = true;

    /// L-004: highest game speed factor (original: 4, with an unintended 8 via Shift+F8).
    std::uint32_t max_game_speed = 8;

    /// Droughts: as the game says, or chosen for the endless game.
    Droughts droughts = Droughts::Game;
    /// Volcanoes: as the game says, or switched on or off.
    Volcanoes volcanoes = Volcanoes::Game;
    /// Pirates: as the game says, or switched on or off.
    Pirates pirates = Pirates::Game;
    /// Pirate nests: one, or as the faction scales them. Up to four players both are the original.
    PirateNests pirate_nests = PirateNests::Scaled;
    /// Prices of the free trader.
    TraderPrices trader_prices = TraderPrices::Original;
    /// How far the free trader looks. Up to four players both are the original.
    TraderReach trader_reach = TraderReach::Scaled;
    /// Large iron veins never run out (docs/re/logistics.md, "Mines and deposits"). The ore mine
    /// still digs only down to 120 t; below that the deep mine digs on, as in the original, but the
    /// vein stays at the last ton above it instead of being used up. Small veins run out as in the
    /// original, gold never does anyway. Off by default: the original's 240 t.
    bool endless_veins = false;

    /// The revised bread chain (docs/modding/balance.md, data/balance/revised_bread.toml): a grain
    /// farm makes two tons of grain from a field, a bakery two tons of bread from two tons of flour
    /// at 15 gold a minute instead of 5. The original's chain gives 1.5 t of bread a minute from 20
    /// fields where two cattle farms and a butcher give 2.4 t of meat; revised it gives 6.3 t at
    /// the same running cost per ton as meat. Its values go under those of the mods' balance.toml.
    /// Off by default, as the original.
    bool revised_bread = false;

    /// The original's computer players with their bugs S-097 to S-099 (docs/re/ai.md): while a
    /// settlement ship sails no city develops, a city whose colony search failed waits for it for
    /// ever, an unreachable site is searched afresh (and found again), the fisher looks in two
    /// directions only, mines and quarries only within seven tiles of the road runs, castles never
    /// count and bakeries are asked for without a mill. Their computers stall after about two hours
    /// where Open1602's grow on (docs/original-comparison.md, section 3).
    bool classic_ai = false;

    /// Which towns are discovered from the start (players, natives, pirates) or only by ship.
    Reveal reveal = Reveal::None;

    /// Victory conditions of an endless game (victory.hpp); all off by default, as the original's
    /// endless game, which no one wins.
    VictoryConditions victory{};

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
        r.pirates = Pirates::Game;
        r.pirate_nests = PirateNests::One;
        r.trader_prices = TraderPrices::Original;
        r.trader_reach = TraderReach::Original;
        r.endless_veins = false;
        r.revised_bread = false;
        r.classic_ai = true;
        r.reveal = Reveal::None;
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
