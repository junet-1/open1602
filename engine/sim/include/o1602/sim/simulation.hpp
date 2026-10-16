// SPDX-License-Identifier: GPL-3.0-or-later
//
// The running game: advances the world in fixed ticks of game time (ADR 0004). The original
// advances its subsystems with the frame time times the game speed in steps of at most 200 ms
// (0x00489670): production (0x0047DAF0) once per game second, then settlers, figures, ships ...
// Open1602 uses fixed ticks so that every computer of a multiplayer game computes the same.
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/sim/production_cycle.hpp"
#include "o1602/sim/rules.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

class Simulation {
public:
    /// Game time per tick.
    static constexpr std::uint32_t kTickMs = 50;
    /// Real time beyond this is dropped instead of caught up (the original: frames longer than
    /// 3 s count as 50 ms, so a stalled computer does not fast-forward the game).
    static constexpr std::uint32_t kMaxCatchUpMs = 3000;

    Simulation(World& world, const assets::BuildingTable& buildings, Rules rules = Rules::open1602());

    /// Advances by `real_ms` of real time at the current speed; runs whole ticks and keeps the rest.
    void advance(std::uint32_t real_ms);
    /// One tick of game time.
    void tick();

    /// Game speed factor (1 = normal, up to Rules::max_game_speed); 0 pauses.
    void set_speed(std::uint32_t speed) noexcept;
    [[nodiscard]] std::uint32_t speed() const noexcept { return m_speed; }

    [[nodiscard]] std::uint64_t game_time_ms() const noexcept { return m_time_ms; }
    [[nodiscard]] const World& world() const noexcept { return *m_world; }
    [[nodiscard]] const Rules& rules() const noexcept { return m_rules; }

    /// Production events since the last call (floating texts, animation phases, later logistics).
    [[nodiscard]] std::vector<ProductionEvent> take_production_events();

private:
    World* m_world;
    const assets::BuildingTable* m_buildings;
    Rules m_rules;
    std::uint32_t m_speed = 1;
    std::uint64_t m_time_ms = 0;
    std::uint32_t m_pending_ms = 0;     ///< game time not yet run as a tick
    std::uint32_t m_production_ms = 0;  ///< game time since the last production second
    std::vector<ProductionEvent> m_production_events;
};

}  // namespace o1602::sim
