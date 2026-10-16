// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/land_figures.hpp"

#include <cmath>

namespace o1602::render {

void LandFigures::clear() {
    m_instances.clear();
    m_states.clear();
}

void LandFigures::add(const FigureInstance& instance, const assets::FigureTable& figures, std::uint8_t anim,
                      std::uint32_t seed) {
    FigureInstance f = instance;
    AnimState state;
    if (const assets::FigureType* type = figures.get(f.figure)) {
        state = start_animation(*type, type->anim(anim) != nullptr ? anim : 0, seed);
    }
    f.anim = state.anim;
    f.frame = state.frame;
    m_instances.push_back(f);
    m_states.push_back(state);
}

void LandFigures::advance(std::uint32_t dt_ms, const assets::FigureTable& figures) {
    for (std::size_t i = 0; i < m_instances.size(); ++i) {
        if (const assets::FigureType* type = figures.get(m_instances[i].figure)) {
            advance_animation(m_states[i], *type, dt_ms);
            m_instances[i].anim = m_states[i].anim;
            m_instances[i].frame = m_states[i].frame;
        }
    }
}

void AnimalFigures::reset(const sim::World& world, const assets::FigureTable& figures) {
    clear();
    for (const sim::Animal& a : world.animals()) {
        const sim::WorldIsland& island = world.islands()[a.island];
        FigureInstance f;
        f.x = island.x + a.x + 0.5;
        f.y = island.y + a.y + 0.5;
        f.figure = a.figure;
        const std::uint32_t seed = (island.x + a.x) * 73856093U ^ (island.y + a.y) * 19349663U;
        f.direction = static_cast<std::uint8_t>((seed >> 3) & 7U);
        add(f, figures, kGrazeAnim, seed);
    }
}

void SoldierFigures::reset(const sim::World& world, const assets::FigureTable& figures) {
    clear();
    for (const sim::Soldier& s : world.soldiers()) {
        FigureInstance f;
        f.x = s.x;
        f.y = s.y;
        f.figure = s.figure;
        f.direction = s.direction;
        f.owner = s.owner;
        f.sheet = assets::FigureSheet::Soldat;  // draw kind 4 (0x0044FA90)
        const auto seed = static_cast<std::uint32_t>(std::lround(s.x * 2.0)) * 73856093U ^
                          static_cast<std::uint32_t>(std::lround(s.y * 2.0)) * 19349663U;
        add(f, figures, s.anim, seed);
    }
}

}  // namespace o1602::render
