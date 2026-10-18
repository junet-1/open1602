// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ships drawn between two ticks (ADR 0004, review point 20): the simulation moves them 20 times a
// second; a screen shows 60 or 144 pictures. The trail keeps where each ship was before the last
// tick, and the picture puts it on the way from there to where it is now, by the share of the next
// tick that has passed. The simulation is not touched.
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/sim/world.hpp"

namespace o1602::render {

class ShipTrail {
public:
    /// Before a tick: the positions the ships leave.
    void capture(const sim::World& world);
    /// Share of the next tick that has passed (Simulation::tick_fraction).
    void set_fraction(double fraction) noexcept { m_fraction = fraction; }

    /// Where to draw the ship, in tiles: between its position before the last tick and its
    /// position now. A ship that was not there before, or jumped (more than two tiles), is drawn
    /// where it is.
    void position(const sim::Ship& ship, double& x, double& y) const;

private:
    struct Was {
        std::uint32_t id;
        std::int32_t x;
        std::int32_t y;
    };
    std::vector<Was> m_was;  ///< sorted by id
    double m_fraction = 1.0;
};

}  // namespace o1602::render
