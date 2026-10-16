// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/simulation.hpp"

#include <algorithm>

namespace o1602::sim {

namespace {

constexpr std::uint32_t kProductionPeriodMs = 1000;  // 0x0047DAF0: once per game second
constexpr std::size_t kMaxPendingEvents = 8192;      // events nobody takes are dropped, oldest first

}  // namespace

Simulation::Simulation(World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures,
                       Rules rules)
    : m_world(&world), m_buildings(&buildings), m_rules(rules), m_logistics(world, buildings, figures) {}

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
    // Figures last, as in the original (0x00451890 is the last subsystem of 0x00489670).
    m_logistics.tick();
}

std::vector<ProductionEvent> Simulation::take_production_events() {
    std::vector<ProductionEvent> events;
    events.swap(m_production_events);
    return events;
}

}  // namespace o1602::sim
