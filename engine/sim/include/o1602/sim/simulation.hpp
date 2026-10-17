// SPDX-License-Identifier: GPL-3.0-or-later
//
// The running game: advances the world in fixed ticks of game time (ADR 0004). The original
// advances its subsystems with the frame time times the game speed in steps of at most 200 ms
// (0x00489670): production (0x0047DAF0) once per game second, then settlers, figures, ships ...
// Open1602 uses fixed ticks so that every computer of a multiplayer game computes the same.
#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/sim/construction.hpp"
#include "o1602/sim/consumption.hpp"
#include "o1602/sim/houses.hpp"
#include "o1602/sim/island_events.hpp"
#include "o1602/sim/logistics.hpp"
#include "o1602/sim/production_cycle.hpp"
#include "o1602/sim/rules.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

/// A player's action (ADR 0004): applied at the start of a tick, the same on every computer.
using Command = std::variant<BuildOrder, DemolishOrder>;

/// What became of a command.
struct CommandResult {
    std::uint64_t sequence = 0;      ///< as returned by Simulation::submit()
    std::optional<BuildPlan> build;  ///< build orders: what was built and why the rest was not
    std::size_t demolished = 0;      ///< demolish orders: buildings removed
    BuildBill refund;                ///< demolish orders: what came back (Rules::demolish_refund)
};

class Simulation {
public:
    /// Game time per tick.
    static constexpr std::uint32_t kTickMs = 50;
    /// Real time beyond this is dropped instead of caught up (the original: frames longer than
    /// 3 s count as 50 ms, so a stalled computer does not fast-forward the game).
    static constexpr std::uint32_t kMaxCatchUpMs = 3000;

    Simulation(World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures,
               Rules rules = Rules::open1602());

    /// Advances by `real_ms` of real time at the current speed; runs whole ticks and keeps the rest.
    /// Commands are applied even while the game is paused (the player may build then).
    void advance(std::uint32_t real_ms);

    /// Queues a command for the start of the next tick; returns its sequence number.
    std::uint64_t submit(Command command);
    /// Applies the queued commands now (the start of a tick does this).
    void apply_commands();
    /// Results of the applied commands since the last call.
    [[nodiscard]] std::vector<CommandResult> take_command_results();
    /// What building and demolishing changed since the last call.
    [[nodiscard]] std::vector<ConstructionEvent> take_construction_events();
    /// Names for new cities (Text.cod [STAEDTE]).
    void set_city_names(std::vector<std::string> names) { m_city_names = std::move(names); }
    /// One tick of game time.
    void tick();

    /// Game speed factor (1 = normal, up to Rules::max_game_speed); 0 pauses.
    void set_speed(std::uint32_t speed) noexcept;
    [[nodiscard]] std::uint32_t speed() const noexcept { return m_speed; }

    [[nodiscard]] std::uint64_t game_time_ms() const noexcept { return m_time_ms; }
    [[nodiscard]] const World& world() const noexcept { return *m_world; }
    [[nodiscard]] const Rules& rules() const noexcept { return m_rules; }
    /// Carriers and carts on their way.
    [[nodiscard]] const Logistics& logistics() const noexcept { return m_logistics; }

    /// Production events since the last call (floating texts, animation phases, later logistics).
    [[nodiscard]] std::vector<ProductionEvent> take_production_events();
    /// Shortages and hunger since the last call (messages for the player).
    [[nodiscard]] std::vector<CityEvent> take_city_events();
    /// Houses that rose, fell back or were abandoned since the last call.
    [[nodiscard]] std::vector<HouseEvent> take_house_events();
    /// Droughts and withered fields since the last call.
    [[nodiscard]] std::vector<IslandEvent> take_island_events();

private:
    void pay_upkeep();
    void handle(const std::vector<ConstructionEvent>& events, std::size_t first);

    World* m_world;
    const assets::BuildingTable* m_buildings;
    Rules m_rules;
    std::uint32_t m_speed = 1;
    std::uint64_t m_time_ms = 0;
    std::uint32_t m_pending_ms = 0;     ///< game time not yet run as a tick
    std::uint32_t m_production_ms = 0;  ///< game time since the last production second
    std::uint32_t m_city_ms = 0;        ///< game time since the cities last consumed
    std::vector<ProductionEvent> m_production_events;
    std::vector<CityEvent> m_city_events;
    std::vector<HouseEvent> m_house_events;
    std::vector<IslandEvent> m_island_events;
    const assets::FigureTable* m_figures;
    std::uint32_t m_upkeep_ms = 0;  ///< game time since the military upkeep was last paid
    std::uint32_t m_island_ms = 0;  ///< game time since the last island epoch
    Logistics m_logistics;
    std::deque<std::pair<std::uint64_t, Command>> m_commands;
    std::uint64_t m_next_command = 1;
    std::vector<CommandResult> m_command_results;
    std::vector<ConstructionEvent> m_construction_events;
    std::vector<std::string> m_city_names;
};

}  // namespace o1602::sim
