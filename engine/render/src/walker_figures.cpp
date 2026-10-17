// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/walker_figures.hpp"

#include <algorithm>

namespace o1602::render {

void WalkerFigures::update(const sim::Logistics& logistics, const sim::World& world, const assets::FigureTable& figures,
                           std::uint32_t dt_ms) {
    m_instances.clear();
    for (Slot& s : m_slots) {
        s.used = false;
    }
    logistics.walkers().for_each([&](sim::Handle h, const sim::Walker& w) {
        const assets::FigureType* type = figures.get(w.figure);
        if (type == nullptr || w.island >= world.islands().size()) {
            return;
        }
        if (w.kind == sim::WalkerKind::Robber && w.working() && w.animation == 1) {
            return;  // waits unseen in his house (0x00451590)
        }
        if (h.index >= m_slots.size()) {
            m_slots.resize(std::size_t{h.index} + 1);
        }
        Slot& slot = m_slots[h.index];
        const std::uint8_t anim = type->anim(w.anim()) != nullptr ? w.anim() : 0;
        if (!slot.active || slot.generation != h.generation || slot.requested != anim) {
            const bool change = slot.active && slot.generation == h.generation;
            slot.generation = h.generation;
            slot.requested = anim;
            slot.state = start_animation(*type, anim, h.index * 2654435761U + h.generation);
            if (change) {
                // A new animation starts from its first frame (0x00446D90): a shot deer falls.
                slot.state.frame = 0;
                slot.state.elapsed_ms = 0;
            }
        } else if (w.moving()) {
            // A frame lasts AnimSpeed * Wegspeed / 32 ms while walking.
            advance_animation(slot.state, *type, dt_ms * 32U / std::max<std::uint32_t>(w.walk_speed, 1));
        } else if (w.working()) {
            advance_animation(slot.state, *type, dt_ms);
        }
        slot.used = true;
        slot.active = true;
        const sim::WorldIsland& island = world.islands()[w.island];
        const sim::WalkerPosition p = sim::walker_position(w);
        FigureInstance f;
        f.x = island.x + p.x;
        f.y = island.y + p.y;
        f.figure = w.figure;
        f.direction = w.direction;
        f.anim = slot.state.anim;
        f.frame = slot.state.frame;
        m_instances.push_back(f);
    });
    for (Slot& s : m_slots) {
        s.active = s.used;
    }
}

}  // namespace o1602::render
