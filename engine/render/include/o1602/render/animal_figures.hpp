// SPDX-License-Identifier: GPL-3.0-or-later
//
// The world's wild animals as land figures. Until the simulation lets them wander, they stand on
// their tile and graze (deer: ANIM 1, which continues randomly with the eating animations).
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/assets/figure_table.hpp"
#include "o1602/render/figure_animation.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::render {

class AnimalFigures {
public:
    /// One figure per animal of `world`; direction and animation phase come from its position.
    void reset(const sim::World& world, const assets::FigureTable& figures);
    void advance(std::uint32_t dt_ms, const assets::FigureTable& figures);
    [[nodiscard]] const std::vector<FigureInstance>& instances() const noexcept { return m_instances; }

    /// Animation the animals start with: grazing.
    static constexpr std::uint8_t kGrazeAnim = 1;

private:
    std::vector<FigureInstance> m_instances;
    std::vector<AnimState> m_states;
};

}  // namespace o1602::render
