// SPDX-License-Identifier: GPL-3.0-or-later
#include "build_tool.hpp"

#include <algorithm>
#include <cstdlib>

namespace o1602::app {

namespace {

constexpr std::int32_t kKindRoad = 1;
constexpr std::int32_t kKindWall = 4;
constexpr std::int32_t kKindGround = 11;
constexpr std::int32_t kKindSquare = 13;
constexpr std::int32_t kKindHq = 35;
constexpr std::int32_t kProdGrowing = 10;
constexpr std::size_t kMaxItems = 512;  // the original's drag list holds 0x200 entries

bool line_kind(const assets::BuildingType& b) noexcept {
    return b.kind == kKindRoad || b.kind == kKindWall;
}
bool area_kind(const assets::BuildingType& b) noexcept {
    return b.prod_kind == kProdGrowing || b.kind == kKindGround || b.kind == kKindSquare;
}

}  // namespace

BuildTool::BuildTool(const assets::BuildingTable& buildings) : m_buildings(&buildings) {
    for (const assets::BuildingType& b : buildings.types()) {
        const auto& c = b.build_cost;
        if (b.destroyed || (c.money == 0 && c.tools == 0 && c.wood == 0 && c.bricks == 0)) {
            continue;  // terrain, ruins and other things nobody builds
        }
        m_list.push_back(&b);
    }
    std::stable_sort(m_list.begin(), m_list.end(), [](const assets::BuildingType* a, const assets::BuildingType* b) {
        return a->build_gfx < b->build_gfx;
    });
}

void BuildTool::set_mode(Mode mode) noexcept {
    m_mode = mode;
    m_drag_start.reset();
}

void BuildTool::cycle(int step) noexcept {
    if (m_list.empty()) {
        return;
    }
    const auto n = static_cast<std::int64_t>(m_list.size());
    m_selected = static_cast<std::size_t>(((static_cast<std::int64_t>(m_selected) + step) % n + n) % n);
}

void BuildTool::rotate(int step) noexcept {
    m_rotation = static_cast<std::uint8_t>((m_rotation + step) & 3);
}

void BuildTool::press(const Spot& spot) noexcept {
    m_drag_start = spot;
}

const assets::BuildingType* BuildTool::selected() const noexcept {
    return m_list.empty() ? nullptr : m_list[m_selected];
}

std::optional<sim::BuildOrder> BuildTool::build_order(const sim::World& world, sim::PlayerId player,
                                                      const Spot& spot) const {
    const assets::BuildingType* b = selected();
    if (m_mode != Mode::Build || b == nullptr) {
        return std::nullopt;
    }
    sim::BuildOrder order;
    order.player = player;
    order.island = spot.island;
    const Spot start = m_drag_start && m_drag_start->island == spot.island ? *m_drag_start : spot;
    const auto add = [&](std::int32_t x, std::int32_t y) {
        if (order.items.size() < kMaxItems) {
            order.items.push_back(sim::BuildItem{b->id, x, y, m_rotation, 0});
        }
    };
    if (m_drag_start && line_kind(*b)) {
        // Along x first, then along y (the original searches a path; a corner will do here).
        const std::int32_t sx = start.x < spot.x ? 1 : -1;
        for (std::int32_t x = start.x; x != spot.x; x += sx) {
            add(x, start.y);
        }
        const std::int32_t sy = start.y < spot.y ? 1 : -1;
        for (std::int32_t y = start.y; y != spot.y; y += sy) {
            add(spot.x, y);
        }
        add(spot.x, spot.y);
    } else if (m_drag_start && area_kind(*b)) {
        for (std::int32_t y = std::min(start.y, spot.y); y <= std::max(start.y, spot.y); ++y) {
            for (std::int32_t x = std::min(start.x, spot.x); x <= std::max(start.x, spot.x); ++x) {
                add(x, y);
            }
        }
    } else {
        add(spot.x, spot.y);
    }
    // A Kontor outside every city is built from the player's nearest ship.
    if (b->kind == kKindHq) {
        const sim::WorldIsland& island = world.islands()[spot.island];
        const std::int64_t wx = static_cast<std::int64_t>(island.x) + spot.x;
        const std::int64_t wy = static_cast<std::int64_t>(island.y) + spot.y;
        std::int64_t best = -1;
        for (std::size_t i = 0; i < world.ships().size(); ++i) {
            const sim::Ship& s = world.ships()[i];
            if (s.owner != player) {
                continue;
            }
            const std::int64_t d = std::abs(s.x - wx) + std::abs(s.y - wy);
            if (best < 0 || d < best) {
                best = d;
                order.ship = i;
            }
        }
        const sim::MapCell& cell =
            island.map.contains(spot.x, spot.y)
                ? island.map.top(static_cast<std::uint32_t>(spot.x), static_cast<std::uint32_t>(spot.y))
                : sim::MapCell{};
        if (cell.city != sim::MapCell::kNoCity) {
            order.ship.reset();  // an upgrade in the city pays from the warehouse
        }
    }
    return order;
}

std::optional<sim::DemolishOrder> BuildTool::demolish_order(sim::PlayerId player, const Spot& spot) const {
    if (m_mode != Mode::Demolish) {
        return std::nullopt;
    }
    const Spot start = m_drag_start && m_drag_start->island == spot.island ? *m_drag_start : spot;
    return sim::DemolishOrder{player, spot.island, {start.x, start.y}, {spot.x, spot.y}};
}

std::string BuildTool::describe() const {
    if (m_mode == Mode::Demolish) {
        return "demolish (drag: roads, fields, trees; click: buildings)";
    }
    const assets::BuildingType* b = selected();
    if (m_mode != Mode::Build || b == nullptr) {
        return {};
    }
    const auto& c = b->build_cost;
    std::string s = "build " + std::to_string(b->id) + " (" + std::to_string(m_selected + 1) + "/" +
                    std::to_string(m_list.size()) + ", rotation " + std::to_string(m_rotation) + ") " +
                    std::to_string(c.money) + " gold";
    const auto goods = [&](std::uint32_t amount, const char* name) {
        if (amount != 0) {
            s += ", " + std::to_string(amount / 32) + " " + name;
        }
    };
    goods(c.tools, "tools");
    goods(c.wood, "wood");
    goods(c.bricks, "bricks");
    goods(c.cannons, "cannons");
    return s;
}

}  // namespace o1602::app
