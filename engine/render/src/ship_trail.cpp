// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/ship_trail.hpp"

#include <algorithm>
#include <cstdlib>

namespace o1602::render {

namespace {

/// A ship without a position lies on its tile's centre.
void now_at(const sim::Ship& ship, double& x, double& y) {
    const bool placed = ship.pos_x != 0 || ship.pos_y != 0;
    x = placed ? static_cast<double>(ship.pos_x) / sim::kShipUnit : ship.x + 0.5;
    y = placed ? static_cast<double>(ship.pos_y) / sim::kShipUnit : ship.y + 0.5;
}

}  // namespace

void ShipTrail::capture(const sim::World& world) {
    m_was.clear();
    for (const sim::Ship& s : world.ships()) {
        m_was.push_back({s.id, s.pos_x, s.pos_y});
    }
    std::sort(m_was.begin(), m_was.end(), [](const Was& a, const Was& b) { return a.id < b.id; });
}

void ShipTrail::position(const sim::Ship& ship, double& x, double& y) const {
    now_at(ship, x, y);
    const auto it =
        std::lower_bound(m_was.begin(), m_was.end(), ship.id, [](const Was& w, std::uint32_t id) { return w.id < id; });
    if (it == m_was.end() || it->id != ship.id || (it->x == 0 && it->y == 0) || (ship.pos_x == 0 && ship.pos_y == 0)) {
        return;
    }
    constexpr std::int32_t kJump = 2 * sim::kShipUnit;
    if (std::abs(ship.pos_x - it->x) > kJump || std::abs(ship.pos_y - it->y) > kJump) {
        return;
    }
    const double t = std::clamp(m_fraction, 0.0, 1.0);
    x = (static_cast<double>(it->x) + (ship.pos_x - it->x) * t) / sim::kShipUnit;
    y = (static_cast<double>(it->y) + (ship.pos_y - it->y) * t) / sim::kShipUnit;
}

}  // namespace o1602::render
