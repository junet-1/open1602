// SPDX-License-Identifier: GPL-3.0-or-later
//
// Figures on land as the renderer sees them, with their animations running in real time. Until
// the simulation moves them, they stand where the savegame put them:
// - wild animals graze (deer: ANIM 1, which continues randomly with the eating animations),
// - soldiers keep the animation they were saved with (usually standing), facing their direction.
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/assets/figure_table.hpp"
#include "o1602/render/figure_animation.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::render {

/// A set of land figures with their animation states.
class LandFigures {
public:
    void advance(std::uint32_t dt_ms, const assets::FigureTable& figures);
    [[nodiscard]] const std::vector<FigureInstance>& instances() const noexcept { return m_instances; }

protected:
    void clear();
    void add(const FigureInstance& instance, const assets::FigureTable& figures, std::uint8_t anim, std::uint32_t seed);

private:
    std::vector<FigureInstance> m_instances;
    std::vector<AnimState> m_states;
};

class AnimalFigures : public LandFigures {
public:
    /// One figure per animal of `world`; direction and animation phase come from its position.
    void reset(const sim::World& world, const assets::FigureTable& figures);

    /// Animation the animals start with: grazing.
    static constexpr std::uint8_t kGrazeAnim = 1;
};

class SoldierFigures : public LandFigures {
public:
    /// One figure per soldier of `world`, drawn from SOLDAT.BSH in the colour of its owner.
    void reset(const sim::World& world, const assets::FigureTable& figures);
};

}  // namespace o1602::render
