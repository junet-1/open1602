// SPDX-License-Identifier: GPL-3.0-or-later
//
// The running game: advances the world in fixed ticks of game time (ADR 0004). The original
// advances its subsystems with the frame time times the game speed in steps of at most 200 ms
// (0x00489670): production (0x0047DAF0) once per game second, then settlers, figures, ships ...
// Open1602 uses fixed ticks so that every computer of a multiplayer game computes the same.
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/sim/construction.hpp"
#include "o1602/sim/consumption.hpp"
#include "o1602/sim/disasters.hpp"
#include "o1602/sim/houses.hpp"
#include "o1602/sim/island_events.hpp"
#include "o1602/sim/logistics.hpp"
#include "o1602/sim/military.hpp"
#include "o1602/sim/production_cycle.hpp"
#include "o1602/sim/rules.hpp"
#include "o1602/sim/ships.hpp"
#include "o1602/sim/volcanoes.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

/// A player's action (ADR 0004): applied at the start of a tick, the same on every computer.
using Command = std::variant<BuildOrder, DemolishOrder, ShipOrder, CargoTransfer, RouteEdit, TradeSetting,
                             ShipyardOrder, DiplomacyOrder, SoldierOrder, SoldierCommand>;

/// What became of a command.
struct CommandResult {
    std::uint64_t sequence = 0;      ///< as returned by Simulation::submit()
    std::optional<BuildPlan> build;  ///< build orders: what was built and why the rest was not
    std::size_t demolished = 0;      ///< demolish orders: buildings removed
    BuildBill refund;                ///< demolish orders: what came back (Rules::demolish_refund)
    bool accepted = false;           ///< all other orders: carried out
    std::uint32_t moved = 0;         ///< cargo transfers: goods moved, 1/32 t
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
    /// Called at the end of every tick: for what has to follow the game tick by tick without
    /// being part of it, such as the townsfolk of render::Ambient. It must not change the world.
    void set_tick_observer(std::function<void(const World&)> observer) { m_tick_observer = std::move(observer); }

    /// Game speed factor (1 = normal, up to Rules::max_game_speed); 0 pauses.
    void set_speed(std::uint32_t speed) noexcept;
    [[nodiscard]] std::uint32_t speed() const noexcept { return m_speed; }

    [[nodiscard]] std::uint64_t game_time_ms() const noexcept { return m_time_ms; }
    [[nodiscard]] const World& world() const noexcept { return *m_world; }
    [[nodiscard]] const Rules& rules() const noexcept { return m_rules; }
    /// Carriers and carts on their way.
    [[nodiscard]] const Logistics& logistics() const noexcept { return m_logistics; }
    /// Ships, the free trader and shipyards.
    [[nodiscard]] const Fleet& fleet() const noexcept { return m_fleet; }
    /// Soldiers, towers and the hostile lists.
    [[nodiscard]] const Military& military() const noexcept { return m_military; }
    [[nodiscard]] const Combat& combat() const noexcept { return m_combat; }
    /// Shots, hits, deaths and destroyed buildings since the last call.
    [[nodiscard]] std::vector<MilitaryEvent> take_military_events();
    /// Treaties offered, signed or broken since the last call.
    [[nodiscard]] std::vector<DiplomacyNotice> take_diplomacy_notices() { return m_world->diplomacy().take_notices(); }
    /// What happened to ships since the last call.
    [[nodiscard]] std::vector<ShipEvent> take_ship_events();
    /// Names for new ships (Text.cod [SHIPS]).
    void set_ship_names(std::vector<std::string> names) { m_fleet.set_ship_names(std::move(names)); }

    /// Production events since the last call (floating texts, animation phases, later logistics).
    [[nodiscard]] std::vector<ProductionEvent> take_production_events();
    /// Shortages and hunger since the last call (messages for the player).
    [[nodiscard]] std::vector<CityEvent> take_city_events();
    /// Houses that rose, fell back or were abandoned since the last call.
    [[nodiscard]] std::vector<HouseEvent> take_house_events();
    /// Droughts and withered fields since the last call.
    [[nodiscard]] std::vector<IslandEvent> take_island_events();
    /// Fires and plagues that broke out, spread, ended or burned a building down since the last call.
    [[nodiscard]] std::vector<DisasterEvent> take_disaster_events();
    /// Clocks and figures for Open1602's own savegame chunk (savegame_extension.hpp). A Simulation
    /// built on a world that carries such state continues from it.
    void save_state(core::ByteWriter& out) const;

    /// Eruptions and what the lava hit since the last call.
    [[nodiscard]] std::vector<VolcanoEvent> take_volcano_events();

private:
    void pay_upkeep();
    bool restore_state(const std::vector<std::uint8_t>& state);
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
    std::vector<DisasterEvent> m_disaster_events;
    std::vector<VolcanoEvent> m_volcano_events;
    std::vector<RobberSpawn> m_robbers;
    std::uint32_t m_disaster_ms = 0;  ///< game time since the last disaster epoch
    const assets::FigureTable* m_figures;
    std::uint32_t m_upkeep_ms = 0;  ///< game time since the military upkeep was last paid
    std::uint32_t m_island_ms = 0;  ///< game time since the last island epoch
    Logistics m_logistics;
    Fleet m_fleet;
    std::vector<ShipEvent> m_ship_events;
    Combat m_combat;
    Military m_military;
    std::vector<MilitaryEvent> m_military_events;
    std::deque<std::pair<std::uint64_t, Command>> m_commands;
    std::uint64_t m_next_command = 1;
    std::vector<CommandResult> m_command_results;
    std::vector<ConstructionEvent> m_construction_events;
    std::vector<std::string> m_city_names;
    std::function<void(const World&)> m_tick_observer;
};

}  // namespace o1602::sim
