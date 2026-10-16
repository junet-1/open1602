// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/animal_figures.hpp"

namespace o1602::render {

void AnimalFigures::reset(const sim::World& world, const assets::FigureTable& figures) {
    m_instances.clear();
    m_states.clear();
    for (const sim::Animal& a : world.animals()) {
        const sim::WorldIsland& island = world.islands()[a.island];
        FigureInstance f;
        f.x = island.x + a.x + 0.5;
        f.y = island.y + a.y + 0.5;
        f.figure = a.figure;
        const std::uint32_t seed = (island.x + a.x) * 73856093U ^ (island.y + a.y) * 19349663U;
        f.direction = static_cast<std::uint8_t>((seed >> 3) & 7U);
        AnimState state;
        if (const assets::FigureType* type = figures.get(a.figure)) {
            state = start_animation(*type, type->anim(kGrazeAnim) != nullptr ? kGrazeAnim : 0, seed);
        }
        f.anim = state.anim;
        f.frame = state.frame;
        m_instances.push_back(f);
        m_states.push_back(state);
    }
}

void AnimalFigures::advance(std::uint32_t dt_ms, const assets::FigureTable& figures) {
    for (std::size_t i = 0; i < m_instances.size(); ++i) {
        if (const assets::FigureType* type = figures.get(m_instances[i].figure)) {
            advance_animation(m_states[i], *type, dt_ms);
            m_instances[i].anim = m_states[i].anim;
            m_instances[i].frame = m_states[i].frame;
        }
    }
}

}  // namespace o1602::render
