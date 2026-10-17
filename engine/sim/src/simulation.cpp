// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/simulation.hpp"

#include <algorithm>

#include "o1602/sim/growth.hpp"

namespace o1602::sim {

namespace {

constexpr std::uint32_t kProductionPeriodMs = 1000;  // 0x0047DAF0: once per game second
constexpr std::size_t kMaxPendingEvents = 8192;      // events nobody takes are dropped, oldest first
constexpr std::uint32_t kUpkeepPeriodMs = 5000;      // 0x00476350: each player once per 5 s

}  // namespace

Simulation::Simulation(World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures,
                       Rules rules)
    : m_world(&world),
      m_buildings(&buildings),
      m_rules(rules),
      m_figures(&figures),
      m_logistics(world, buildings, figures) {
    m_time_ms = world.time_ms();
    m_island_ms = static_cast<std::uint32_t>(m_time_ms % Logistics::kIslandEpochMs);
    if (world.saved_rules()) {
        m_rules = *world.saved_rules();  // the settings the game was started with
    }
    recount_disasters(world);
    if (restore_state(world.take_simulation_state())) {
        return;  // a game saved by Open1602 continues where it was
    }
    // As when the original loads a game: every deer spot gets its deer.
    m_logistics.spawn_deer_at_spots();
    // The original takes the saved coverage of the houses; Open1602 recomputes it once, which
    // also restores what the savegame does not keep (gallows, fountains).
    update_services(world, buildings);
}

void Simulation::save_state(core::ByteWriter& out) const {
    constexpr std::uint16_t kVersion = 1;
    out.u16(kVersion);
    out.u32(m_production_ms);
    out.u32(m_city_ms);
    out.u32(m_disaster_ms);
    out.u32(m_upkeep_ms);
    out.u32(m_island_ms);
    m_logistics.save(out);
}

bool Simulation::restore_state(const std::vector<std::uint8_t>& state) {
    if (state.empty()) {
        return false;
    }
    core::ByteReader reader(state);
    StateIn in(reader);
    if (in.u16() != 1) {
        return false;
    }
    const std::uint32_t production = in.u32();
    const std::uint32_t city = in.u32();
    const std::uint32_t disaster = in.u32();
    const std::uint32_t upkeep = in.u32();
    const std::uint32_t island = in.u32();
    if (!in.ok() || !m_logistics.load(reader)) {
        return false;
    }
    m_production_ms = production;
    m_city_ms = city;
    m_disaster_ms = disaster;
    m_upkeep_ms = upkeep;
    m_island_ms = island;
    return true;
}

void Simulation::set_speed(std::uint32_t speed) noexcept {
    m_speed = std::min(speed, m_rules.max_game_speed);
}

void Simulation::advance(std::uint32_t real_ms) {
    if (m_speed == 0) {
        apply_commands();
        return;
    }
    std::uint64_t game_ms = std::uint64_t{real_ms} * m_speed;
    if (game_ms > kMaxCatchUpMs) {
        game_ms = kTickMs;
    }
    m_pending_ms += static_cast<std::uint32_t>(game_ms);
    while (m_pending_ms >= kTickMs) {
        m_pending_ms -= kTickMs;
        tick();
    }
}

std::uint64_t Simulation::submit(Command command) {
    const std::uint64_t sequence = m_next_command++;
    m_commands.emplace_back(sequence, std::move(command));
    return sequence;
}

void Simulation::apply_commands() {
    while (!m_commands.empty()) {
        auto [sequence, command] = std::move(m_commands.front());
        m_commands.pop_front();
        CommandResult result;
        result.sequence = sequence;
        const std::size_t first = m_construction_events.size();
        if (const auto* build = std::get_if<BuildOrder>(&command)) {
            result.build = execute_build(*m_world, *m_buildings, *build, m_city_names, &m_construction_events);
        } else if (const auto* demolish = std::get_if<DemolishOrder>(&command)) {
            const DemolishResult d =
                execute_demolish(*m_world, *m_buildings, *demolish, &m_construction_events, m_rules.refund_share());
            result.demolished = d.removed;
            result.refund = d.refund;
        }
        handle(m_construction_events, first);
        m_command_results.push_back(std::move(result));
    }
    if (m_construction_events.size() > kMaxPendingEvents) {
        m_construction_events.erase(m_construction_events.begin(),
                                    m_construction_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
    }
    if (m_command_results.size() > kMaxPendingEvents) {
        m_command_results.erase(m_command_results.begin(),
                                m_command_results.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
    }
}

void Simulation::handle(const std::vector<ConstructionEvent>& events, std::size_t first) {
    bool storage = false;
    for (std::size_t i = first; i < events.size(); ++i) {
        const ConstructionEvent& e = events[i];
        switch (e.kind) {
            case ConstructionEvent::Kind::Removed:
                if (e.production) {
                    // S-002: figures of a building that is gone end their trip; what they carry
                    // is lost and what they reserved is released.
                    m_logistics.building_removed(*e.production);
                }
                break;
            case ConstructionEvent::Kind::StorageChanged:
            case ConstructionEvent::Kind::CityFounded:
            case ConstructionEvent::Kind::CityDissolved:
                storage = true;
                break;
            case ConstructionEvent::Kind::Placed:
                break;
        }
    }
    if (storage) {
        m_logistics.update_storage();
        // A smaller warehouse loses what does not fit (0x00482120).
        for (std::size_t i = first; i < events.size(); ++i) {
            if (events[i].kind != ConstructionEvent::Kind::StorageChanged) {
                continue;
            }
            if (City* c = m_world->city(events[i].island, events[i].city)) {
                for (std::uint32_t& g : c->goods) {
                    g = std::min(g, c->capacity);
                }
            }
        }
    }
}

std::vector<CommandResult> Simulation::take_command_results() {
    std::vector<CommandResult> results;
    results.swap(m_command_results);
    return results;
}

std::vector<ConstructionEvent> Simulation::take_construction_events() {
    std::vector<ConstructionEvent> events;
    events.swap(m_construction_events);
    return events;
}

void Simulation::tick() {
    apply_commands();
    m_time_ms += kTickMs;
    m_world->set_time_ms(m_time_ms);
    // Growth first, as in the original (0x0047CA80 is the first subsystem of 0x00489670).
    advance_growth(*m_world, *m_buildings, kTickMs);
    m_production_ms += kTickMs;
    if (m_production_ms >= kProductionPeriodMs) {
        m_production_ms -= kProductionPeriodMs;
        const std::size_t first = m_production_events.size();
        advance_production(*m_world, *m_buildings, &m_production_events);
        for (std::size_t i = first; i < m_production_events.size(); ++i) {
            m_logistics.handle(m_production_events[i]);
        }
        if (m_production_events.size() > kMaxPendingEvents) {
            m_production_events.erase(m_production_events.begin(),
                                      m_production_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    // The cities consume every 10 s (0x0047F8A0 comes after the production in the original).
    m_city_ms += kTickMs;
    if (m_city_ms >= kCityTickMs) {
        m_city_ms -= kCityTickMs;
        advance_cities(*m_world, *m_buildings, &m_city_events);
        if (m_city_events.size() > kMaxPendingEvents) {
            m_city_events.erase(m_city_events.begin(),
                                m_city_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    // Every 60-67 s a city may catch the plague, send out a robber or catch fire (0x0047F8A0).
    advance_city_events(*m_world, *m_buildings, &m_disaster_events, &m_robbers);
    for (const RobberSpawn& r : m_robbers) {
        m_logistics.spawn_robber(r);
    }
    m_robbers.clear();
    // Houses after the cities (0x0047B9C0).
    advance_houses(*m_world, *m_buildings, kTickMs, &m_house_events);
    if (m_house_events.size() > kMaxPendingEvents) {
        m_house_events.erase(m_house_events.begin(),
                             m_house_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
    }
    // Soldiers' upkeep of human players every 5 s (0x00476350): a twelfth of the upkeep per minute.
    m_upkeep_ms += kTickMs;
    if (m_upkeep_ms >= kUpkeepPeriodMs) {
        m_upkeep_ms -= kUpkeepPeriodMs;
        pay_upkeep();
    }
    // Disasters in epochs of 10 s (0x0047A020): they spread, end or burn the building down.
    m_disaster_ms += kTickMs;
    if (m_disaster_ms >= kDisasterEpochMs) {
        m_disaster_ms -= kDisasterEpochMs;
        const std::size_t first = m_construction_events.size();
        advance_disasters(*m_world, *m_buildings, &m_disaster_events, &m_construction_events);
        handle(m_construction_events, first);
        if (m_disaster_events.size() > kMaxPendingEvents) {
            m_disaster_events.erase(m_disaster_events.begin(),
                                    m_disaster_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    // Island epochs of 30 s (0x0046B3E0): droughts, then deer come back and settle.
    m_island_ms += kTickMs;
    if (m_island_ms >= Logistics::kIslandEpochMs) {
        m_island_ms -= Logistics::kIslandEpochMs;
        advance_island_events(
            *m_world, *m_buildings, m_rules,
            [this](std::uint16_t island, TilePos tile) { return m_logistics.field_reserved(island, tile); },
            &m_island_events);
        // The eruption roll; the original makes it only while no drought roll is due (S-042).
        roll_eruptions(*m_world, m_rules, &m_volcano_events);
        if (m_island_events.size() > kMaxPendingEvents) {
            m_island_events.erase(m_island_events.begin(),
                                  m_island_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
        m_logistics.advance_islands();
    }
    // Eruptions and lava (figures of the original, kinds 0x20 and 0x21).
    {
        const std::size_t first = m_construction_events.size();
        const assets::FigureType* lava = m_figures->get(kFigureLava);
        advance_volcanoes(*m_world, *m_buildings, lava != nullptr && lava->speed != 0 ? lava->speed : kLavaSpeed,
                          &m_volcano_events, &m_disaster_events, &m_construction_events);
        handle(m_construction_events, first);
        if (m_volcano_events.size() > kMaxPendingEvents) {
            m_volcano_events.erase(m_volcano_events.begin(),
                                   m_volcano_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    // Figures last, as in the original (0x00451890 is the last subsystem of 0x00489670).
    m_logistics.tick();
    for (const DisasterEvent& e : m_logistics.take_disaster_events()) {
        m_disaster_events.push_back(e);
    }
}

void Simulation::pay_upkeep() {
    std::vector<std::uint32_t> upkeep(m_world->players().size(), 0);
    for (const Soldier& s : m_world->soldiers()) {
        const assets::FigureType* type = m_figures->get(s.figure);
        if (type != nullptr && s.owner < upkeep.size()) {
            upkeep[s.owner] += m_figures->unit_upkeep(type->kind);
        }
    }
    for (std::size_t p = 0; p < upkeep.size(); ++p) {
        Player* player = m_world->player(static_cast<PlayerId>(p));
        // Computer players pay nothing, as in the original (0x00475B00). Cannon towers are not
        // simulated yet.
        if (player != nullptr && player->role == PlayerRole::Human) {
            player->money -= static_cast<std::int32_t>(upkeep[p] / 12);
        }
    }
}

std::vector<HouseEvent> Simulation::take_house_events() {
    std::vector<HouseEvent> events;
    events.swap(m_house_events);
    return events;
}

std::vector<DisasterEvent> Simulation::take_disaster_events() {
    std::vector<DisasterEvent> events;
    events.swap(m_disaster_events);
    return events;
}

std::vector<VolcanoEvent> Simulation::take_volcano_events() {
    std::vector<VolcanoEvent> events;
    events.swap(m_volcano_events);
    return events;
}

std::vector<IslandEvent> Simulation::take_island_events() {
    std::vector<IslandEvent> events;
    events.swap(m_island_events);
    return events;
}

std::vector<CityEvent> Simulation::take_city_events() {
    std::vector<CityEvent> events;
    events.swap(m_city_events);
    return events;
}

std::vector<ProductionEvent> Simulation::take_production_events() {
    std::vector<ProductionEvent> events;
    events.swap(m_production_events);
    return events;
}

}  // namespace o1602::sim
