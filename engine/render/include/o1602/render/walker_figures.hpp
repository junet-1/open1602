// SPDX-License-Identifier: GPL-3.0-or-later
//
// Carriers and carts of the logistics as the renderer sees them. The simulation moves them; here
// only their walking animations run: empty or loaded (ANIM 1 / 0 of TRAEGER and KARREN), slower
// on slow ground like in the original (0x0045D0B0 stretches a frame by Wegspeed / 32).
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/render/figure_animation.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/logistics.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::render {

class WalkerFigures {
public:
    /// Brings the figures in line with the walkers of `logistics` and advances their animations
    /// by `dt_ms` of game time.
    void update(const sim::Logistics& logistics, const sim::World& world, const assets::BuildingTable& buildings,
                const assets::FigureTable& figures, std::uint32_t dt_ms);
    [[nodiscard]] const std::vector<FigureInstance>& instances() const noexcept { return m_instances; }

private:
    struct Slot {
        std::uint32_t generation = 0;
        bool used = false;    ///< seen in this update
        bool active = false;  ///< `state` belongs to the walker in this slot
        /// Animation the walker asked for; `state` may have moved on to a successor (RANDOM and
        /// JUMPTO animations: grazing deer look up, a shot deer falls and lies).
        std::uint8_t requested = 0;
        AnimState state;
        double z = 0.0;  ///< height, from the tile it was first seen on (render::spawn_height)
    };
    std::vector<Slot> m_slots;  ///< by walker handle index
    std::vector<FigureInstance> m_instances;
};

}  // namespace o1602::render
