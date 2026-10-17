// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/land_figures.hpp"

#include <cmath>

namespace o1602::render {

namespace {
constexpr std::int32_t kLandPosoffs = 20;  // HIGHBODEN
constexpr std::int32_t kBeachRuin = 29;    // HAUS Kind STRANDRUINE
constexpr std::int32_t kPier = 30;
}  // namespace

double spawn_height(const sim::World& world, const assets::BuildingTable& buildings, double x, double y,
                    bool piers_as_land) {
    if (x < 0.0 || y < 0.0) {
        return 0.0;
    }
    const auto wx = static_cast<std::uint32_t>(x);
    const auto wy = static_cast<std::uint32_t>(y);
    if (!world.contains(wx, wy)) {
        return 0.0;
    }
    const sim::World::Cell& cell = world.cell(wx, wy);
    if (cell.island == sim::World::kSea || cell.island >= world.islands().size()) {
        return 0.0;
    }
    const sim::WorldIsland& island = world.islands()[cell.island];
    const std::uint32_t lx = wx - island.x;
    const std::uint32_t ly = wy - island.y;
    if (!island.map.contains(static_cast<std::int32_t>(lx), static_cast<std::int32_t>(ly))) {
        return 0.0;
    }
    const sim::MapCell& c = island.map.top(lx, ly);
    const assets::BuildingType* b = c.empty() ? nullptr : buildings.by_sprite(c.sprite);
    if (b == nullptr) {
        return 0.0;
    }
    if (piers_as_land && (b->kind == kBeachRuin || b->kind == kPier)) {
        return kLandPosoffs * kHeightPerPosoffs;
    }
    return b->posoffs * kHeightPerPosoffs;
}

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

void AnimalFigures::reset(const sim::World& world, const assets::BuildingTable& buildings,
                          const assets::FigureTable& figures) {
    clear();
    for (const sim::Animal& a : world.animals()) {
        const sim::WorldIsland& island = world.islands()[a.island];
        FigureInstance f;
        f.x = island.x + a.x + 0.5;
        f.y = island.y + a.y + 0.5;
        f.figure = a.figure;
        f.z = spawn_height(world, buildings, f.x, f.y);
        const std::uint32_t seed = (island.x + a.x) * 73856093U ^ (island.y + a.y) * 19349663U;
        f.direction = static_cast<std::uint8_t>((seed >> 3) & 7U);
        add(f, figures, kGrazeAnim, seed);
    }
}

void SoldierFigures::reset(const sim::World& world, const assets::BuildingTable& buildings,
                           const assets::FigureTable& figures) {
    clear();
    for (const sim::Soldier& s : world.soldiers()) {
        FigureInstance f;
        f.x = s.x;
        f.y = s.y;
        f.figure = s.figure;
        f.direction = s.direction;
        f.owner = s.owner;
        f.z = spawn_height(world, buildings, s.x, s.y, true);  // 0x0045E1F0 with 0x00451180
        f.sheet = assets::FigureSheet::Soldat;                 // draw kind 4 (0x0044FA90)
        const auto seed = static_cast<std::uint32_t>(std::lround(s.x * 2.0)) * 73856093U ^
                          static_cast<std::uint32_t>(std::lround(s.y * 2.0)) * 19349663U;
        add(f, figures, s.anim, seed);
    }
}

}  // namespace o1602::render
