// SPDX-License-Identifier: GPL-3.0-or-later
//
// Game rules that differ between Open1602 and the original (ADR 0006, docs/re/bugs.md).
//
// Only points that change balance or feel are switchable. Real bugs and exploits are always fixed
// and have no switch here. Rules are part of the game state and of a multiplayer session, so all
// peers simulate with the same values.
#pragma once

#include <cstdint>

namespace o1602::sim {

struct Rules {
    /// L-001: ships per player. 0 = unlimited; the original hard limit is 33.
    std::uint32_t max_ships_per_player = 0;

    /// S-019: random worlds contain at least one deposit of every resource that a building needs
    /// (the original can generate endless games without gold).
    bool guarantee_resources = true;

    /// L-004: highest game speed factor (original: 4, with an unintended 8 via Shift+F8).
    std::uint32_t max_game_speed = 8;

    /// Defaults: corrected behaviour.
    [[nodiscard]] static constexpr Rules open1602() noexcept { return Rules{}; }

    /// Behaviour of the original game where it is switchable.
    [[nodiscard]] static constexpr Rules original() noexcept {
        Rules r;
        r.max_ships_per_player = 33;
        r.guarantee_resources = false;
        r.max_game_speed = 4;
        return r;
    }

    friend constexpr bool operator==(const Rules&, const Rules&) = default;
};

}  // namespace o1602::sim
