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
    // The original takes the saved coverage of the houses; Open1602 recomputes it once, which
    // also restores what the savegame does not keep (gallows, fountains).
    update_services(world, buildings);
}

void Simulation::set_speed(std::uint32_t speed) noexcept {
    m_speed = std::min(speed, m_rules.max_game_speed);
}

void Simulation::advance(std::uint32_t real_ms) {
    if (m_speed == 0) {
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

void Simulation::tick() {
    m_time_ms += kTickMs;
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
    // Figures last, as in the original (0x00451890 is the last subsystem of 0x00489670).
    m_logistics.tick();
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
