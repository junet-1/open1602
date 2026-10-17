// SPDX-License-Identifier: GPL-3.0-or-later
//
// Soldiers, cannon towers and what their shots do (docs/re/military.md).
//
// Like ships, a soldier decides what to do only at the end of each run of one to three half-tile
// cells, after an attack or a short wait (0x00456D00): whether a hostile figure is in range, whether
// to chase one, whether it has arrived. Towers look for a target once a second (0x00458AC0). Shots
// land after the shooter's Shottime (combat.hpp); a soldier dies at 0 health, a building falls to
// ruins at its Maxenergy. The original decides kills and destruction only on the attacker's computer;
// Open1602 computes everything on every computer from the same state (ADR 0004).
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/core/byte_reader.hpp"
#include "o1602/core/byte_writer.hpp"
#include "o1602/sim/combat.hpp"
#include "o1602/sim/construction.hpp"
#include "o1602/sim/land_paths.hpp"
#include "o1602/sim/rules.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

class Fleet;

/// Sends soldiers somewhere (Order_Move 0x0044D540): to a place, to a building, against a unit.
struct SoldierOrder {
    PlayerId player = kNoPlayer;
    std::vector<std::uint32_t> soldiers;  ///< Soldier::id
    Target target;                        ///< a Spot, a building, a soldier or ship; none = stop
    /// With the attack cursor: an object of another player (not the free trader) means war with him.
    bool attack = false;
    /// FORMATION of figuren.cod to take up around a Spot, 0xFF = none (everyone walks to the spot).
    std::uint8_t formation = 0xFF;
};

/// Further commands to one unit (soldier ops 0x0C, 0x28, 0x29, 0x2B).
struct SoldierCommand {
    enum class Op : std::uint8_t {
        Stop,
        Disband,      ///< "Einheit auflösen": dies without fight (Rules::disband_cost decides the gold)
        SetWaypoint,  ///< `index` 0 or 1 becomes `point`
        Patrol,       ///< `on`: walk between the waypoints (once with `one_way`), or stop doing so
    };
    PlayerId player = kNoPlayer;
    std::uint32_t soldier = 0;
    Op op = Op::Stop;
    std::uint8_t index = 0;
    Target point;
    bool on = true;
    bool one_way = false;
};

/// What happened in a fight, for messages, sounds and effects.
struct MilitaryEvent {
    enum class Kind : std::uint8_t {
        Shot,               ///< `unit` fired at `target`
        Hit,                ///< a shot of `unit` (the shooter) landed on `target` at (x, y)
        SoldierKilled,      ///< `target` was killed by `player`'s unit
        BuildingDestroyed,  ///< `target` fell to ruins
        SettlementShelled,  ///< MELDUNG 15: ships attack a building of `owner`
        ShipAttacked,       ///< 8, or 5 when `pirates`
        ShipSunk,           ///< 9
        SoldierHealed,      ///< 13
        WarDeclared,        ///< the attack cursor declared war: `player` on `owner`
    };
    Kind kind = Kind::Shot;
    Target unit;
    Target target;
    PlayerId player = kNoPlayer;  ///< the acting player (attacker)
    PlayerId owner = kNoPlayer;   ///< the player it happened to
    std::int32_t x = 0;           ///< position, 1/4096 tile (hits)
    std::int32_t y = 0;
    std::uint16_t figure = 0;  ///< effect figure (Hitfignr) or the shooter's figure
    bool pirates = false;
};

class Military {
public:
    Military(World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures,
             const Rules& rules, Combat& combat);

    /// The fleet gets hit too: ships slow down when hit and sink at 0 health.
    void set_fleet(Fleet* fleet) noexcept { m_fleet = fleet; }

    /// One tick: soldiers walk and fight, towers fire, shots land, buildings heal.
    void tick(std::uint32_t dt_ms, std::vector<ConstructionEvent>* construction);

    bool order(const SoldierOrder& order);
    bool command(const SoldierCommand& command);

    /// A tower building was built or a city's territory changed: towers come and go with it.
    void update_towers(std::uint16_t island);

    [[nodiscard]] std::vector<MilitaryEvent> take_events() {
        std::vector<MilitaryEvent> e;
        e.swap(m_events);
        return e;
    }

    /// Damage of a building at its origin, 1/32 HP.
    [[nodiscard]] std::uint32_t building_damage(std::uint16_t island, std::uint16_t x, std::uint16_t y) const;
    /// Applies damage to a building (0x0047A650): destroyed at its Maxenergy.
    void damage_building(std::uint16_t island, std::uint16_t x, std::uint16_t y, std::uint32_t damage, bool cannon,
                         PlayerId attacker, std::vector<ConstructionEvent>* construction);

    /// State that is not in the world (clocks), for Open1602's savegame.
    void save(core::ByteWriter& out) const;
    bool load(core::ByteReader& in);

private:
    // Soldiers (soldiers.cpp)
    void advance(Soldier& s, std::uint32_t budget_us);
    void decide(Soldier& s);
    void start_wait(Soldier& s, std::uint32_t ms);
    bool start_next_run(Soldier& s);
    bool plan(Soldier& s, std::int32_t radius, bool hostile_goal);
    bool plan_to(Soldier& s, const Target& target, std::int32_t radius, bool hostile_goal);
    [[nodiscard]] bool target_valid(const Target& t) const;
    [[nodiscard]] std::optional<CellRect> target_cells(const Target& t) const;
    [[nodiscard]] PlayerId owner_of(const Target& t) const;
    [[nodiscard]] bool attackable(const Soldier& s, const Target& t) const;
    [[nodiscard]] std::optional<std::uint8_t> fire_direction(const Soldier& s, const assets::FigureType& type,
                                                             const Target& t) const;
    bool attack(Soldier& s, const assets::FigureType& type, const Target& t);
    bool arrived(Soldier& s);
    bool wander(Soldier& s);
    [[nodiscard]] const assets::FigureType* type_of(const Soldier& s) const;
    [[nodiscard]] std::uint32_t run_us(const Soldier& s, std::uint8_t direction, std::uint8_t cells) const;
    void kill(Soldier& s, PlayerId killer, bool disband);

    // Towers (towers.cpp)
    void towers_tick(std::uint32_t dt_ms);
    void tower_think(Tower& t);

    // Hits and buildings (combat side)
    void resolve_hits(std::vector<ConstructionEvent>* construction);
    void hit(const PendingHit& h, std::vector<ConstructionEvent>* construction);
    void regenerate();

    World* m_world;
    const assets::BuildingTable* m_buildings;
    const assets::FigureTable* m_figures;
    const Rules* m_rules;
    Combat* m_combat;
    Fleet* m_fleet = nullptr;
    LandPlanner m_planner;
    std::vector<MilitaryEvent> m_events;
    std::uint32_t m_regen_ms = 0;  ///< game time since the building damage last healed
};

}  // namespace o1602::sim
