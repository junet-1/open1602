// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/land_paths.hpp"

#include <algorithm>
#include <cstdlib>

#include "o1602/sim/pathfinding.hpp"
#include "o1602/sim/sea_paths.hpp"

namespace o1602::sim {

namespace {

constexpr std::uint8_t kBlocked = 1;
constexpr std::uint8_t kVisited = 4;
constexpr std::uint32_t kInfinite = 0xFFFFFFFFU;

// Neighbour order of the original's wavefront: the diagonals first, then the straight steps.
constexpr std::uint8_t kOrder[8] = {7, 1, 5, 3, 0, 4, 6, 2};

}  // namespace

bool soldiers_walk_on(const assets::BuildingType& b) noexcept {
    switch (b.kind) {
        case building_kind::kRoad:
        case building_kind::kGate:
        case building_kind::kGround:
        case building_kind::kRuin:
        case building_kind::kSquare:
        case building_kind::kBridge:
        case building_kind::kBeachRuin:
        case building_kind::kPier:
            return true;
        default:
            return false;
    }
}

void LandMap::rebuild(const World& world, const assets::BuildingTable& buildings) {
    m_world = &world;
    m_tiles.assign(world.islands().size(), {});
    for (std::size_t i = 0; i < world.islands().size(); ++i) {
        update_island(world, buildings, static_cast<std::uint16_t>(i));
    }
}

void LandMap::update_island(const World& world, const assets::BuildingTable& buildings, std::uint16_t index) {
    m_world = &world;
    if (index >= world.islands().size()) {
        return;
    }
    if (m_tiles.size() < world.islands().size()) {
        m_tiles.resize(world.islands().size());
    }
    const IslandMap& map = world.islands()[index].map;
    auto& tiles = m_tiles[index];
    tiles.assign(std::size_t{map.width()} * map.height(), nullptr);
    for (std::uint32_t y = 0; y < map.height(); ++y) {
        for (std::uint32_t x = 0; x < map.width(); ++x) {
            const MapCell& c = map.top(x, y);
            tiles[std::size_t{y} * map.width() + x] = c.empty() ? nullptr : buildings.by_sprite(c.sprite);
        }
    }
}

std::uint16_t LandMap::island_at(std::int32_t x, std::int32_t y) const noexcept {
    if (m_world == nullptr || x < 0 || y < 0 ||
        !m_world->contains(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y))) {
        return World::kSea;
    }
    return m_world->cell(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)).island;
}

const assets::BuildingType* LandMap::building(std::int32_t x, std::int32_t y) const noexcept {
    const std::uint16_t island = island_at(x, y);
    if (island == World::kSea || island >= m_tiles.size()) {
        return nullptr;
    }
    const World::Cell& c = m_world->cell(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    const IslandMap& map = m_world->islands()[island].map;
    const std::size_t i = std::size_t{c.local_y} * map.width() + c.local_x;
    return i < m_tiles[island].size() ? m_tiles[island][i] : nullptr;
}

LandMap::Cell LandMap::cell(std::int32_t x2, std::int32_t y2, std::uint8_t speed_type,
                            std::uint8_t* cost) const noexcept {
    if (x2 < 0 || y2 < 0) {
        return Cell::Off;
    }
    const std::int32_t x = x2 / 2;
    const std::int32_t y = y2 / 2;
    if (island_at(x, y) == World::kSea) {
        return Cell::Off;
    }
    const assets::BuildingType* b = building(x, y);
    if (b == nullptr) {
        return Cell::Open;
    }
    if (soldiers_walk_on(*b)) {
        if (cost != nullptr) {
            *cost = b->walk_speed[speed_type & 3U];
        }
        return Cell::Walk;
    }
    return b->blocks_shots ? Cell::Wall : Cell::Open;
}

std::int32_t CellRect::distance(std::int32_t x, std::int32_t y) const noexcept {
    const auto [nx, ny] = nearest(x, y);
    return sea_distance(nx - x, ny - y);
}

std::pair<std::int32_t, std::int32_t> CellRect::nearest(std::int32_t x, std::int32_t y) const noexcept {
    return {std::clamp(x, x0, x1), std::clamp(y, y0, y1)};
}

bool line_of_sight(const LandMap& map, std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1,
                   const CellRect& ignore) noexcept {
    // Bresenham over the cells between the ends.
    const std::int32_t dx = std::abs(x1 - x0);
    const std::int32_t dy = -std::abs(y1 - y0);
    const std::int32_t sx = x0 < x1 ? 1 : -1;
    const std::int32_t sy = y0 < y1 ? 1 : -1;
    std::int32_t err = dx + dy;
    std::int32_t x = x0;
    std::int32_t y = y0;
    while (x != x1 || y != y1) {
        const std::int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
        if ((x == x1 && y == y1) || ignore.contains(x, y)) {
            continue;
        }
        if (map.cell(x, y, 0) == LandMap::Cell::Wall) {
            return false;
        }
    }
    return true;
}

std::uint8_t direction_to(std::int32_t dx, std::int32_t dy) noexcept {
    const std::int32_t ax = std::abs(dx);
    const std::int32_t ay = std::abs(dy);
    if (ax == 0 && ay == 0) {
        return 0;
    }
    // tan(22.5 degrees) ~ 5/12.
    if (ay * 12 <= ax * 5) {
        return dx > 0 ? 2 : 6;
    }
    if (ax * 12 <= ay * 5) {
        return dy > 0 ? 4 : 0;
    }
    if (dx > 0) {
        return dy > 0 ? 3 : 1;
    }
    return dy > 0 ? 5 : 7;
}

LandCourse LandPlanner::plan(const LandMap& map, const LandPlan& r) {
    LandCourse course;
    if (r.radius <= 0) {
        return course;
    }
    const std::int32_t size = 2 * r.radius + 1;
    const std::int32_t x0 = r.start_x - r.radius + r.shift_x;
    const std::int32_t y0 = r.start_y - r.radius + r.shift_y;
    const auto cells = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
    m_cost.assign(cells, kInfinite);
    m_state.assign(cells, 0);
    m_speed.assign(cells, 32);
    const auto inside = [&](std::int32_t x, std::int32_t y) {
        return x >= x0 && y >= y0 && x < x0 + size && y < y0 + size;
    };
    const auto at = [&](std::int32_t x, std::int32_t y) {
        return static_cast<std::size_t>(y - y0) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x - x0);
    };
    if (!inside(r.start_x, r.start_y)) {
        return course;
    }
    // Each tile once for its (up to) four cells.
    const std::int32_t tx0 = (x0 >= 0 ? x0 : x0 - 1) / 2;
    const std::int32_t ty0 = (y0 >= 0 ? y0 : y0 - 1) / 2;
    for (std::int32_t ty = ty0; 2 * ty < y0 + size; ++ty) {
        for (std::int32_t tx = tx0; 2 * tx < x0 + size; ++tx) {
            std::uint8_t speed = 32;
            const bool walk = map.cell(2 * tx, 2 * ty, r.speed_type, &speed) == LandMap::Cell::Walk;
            for (std::int32_t dy = 0; dy < 2; ++dy) {
                for (std::int32_t dx = 0; dx < 2; ++dx) {
                    const std::int32_t x = 2 * tx + dx;
                    const std::int32_t y = 2 * ty + dy;
                    if (!inside(x, y)) {
                        continue;
                    }
                    m_state[at(x, y)] = walk ? 0 : kBlocked;
                    m_speed[at(x, y)] = speed;
                }
            }
        }
    }
    for (const auto& [bx, by] : r.blocked) {
        if (inside(bx, by)) {
            m_state[at(bx, by)] = kBlocked;
        }
    }
    const std::size_t start = at(r.start_x, r.start_y);
    m_state[start] = 0;  // the soldier's own cell is always open
    if (r.goal && r.goal(r.start_x, r.start_y)) {
        course.reached_goal = true;
        course.end_x = r.start_x;
        course.end_y = r.start_y;
        return course;
    }

    m_heap.clear();
    std::uint32_t seq = 0;
    const auto later = [](const Node& a, const Node& b) { return a.cost != b.cost ? a.cost > b.cost : a.seq > b.seq; };
    const auto push = [&](std::uint32_t cost, std::size_t cell) {
        m_heap.push_back(Node{cost, seq++, static_cast<std::uint32_t>(cell)});
        std::push_heap(m_heap.begin(), m_heap.end(), later);
    };
    m_cost[start] = 0;
    push(0, start);
    std::optional<std::size_t> goal;
    std::int32_t goal_distance = 0x7FFFFFFF;
    std::optional<std::size_t> edge;
    std::int32_t edge_distance = sea_distance(r.aim_x - r.start_x, r.aim_y - r.start_y);
    while (!m_heap.empty()) {
        std::pop_heap(m_heap.begin(), m_heap.end(), later);
        const Node n = m_heap.back();
        m_heap.pop_back();
        if ((m_state[n.cell] & kVisited) != 0) {
            continue;
        }
        m_state[n.cell] |= kVisited;
        const auto cx = static_cast<std::int32_t>(n.cell % static_cast<std::size_t>(size)) + x0;
        const auto cy = static_cast<std::int32_t>(n.cell / static_cast<std::size_t>(size)) + y0;
        if (n.cell != start && r.goal && r.goal(cx, cy)) {
            const std::int32_t d = sea_distance(r.aim_x - cx, r.aim_y - cy);
            if (r.first_goal) {
                goal = n.cell;
                break;
            }
            if (d < goal_distance) {
                goal_distance = d;
                goal = n.cell;
            }
        }
        if (cx == x0 || cy == y0 || cx == x0 + size - 1 || cy == y0 + size - 1) {
            const std::int32_t d = sea_distance(r.aim_x - cx, r.aim_y - cy);
            if (d < edge_distance) {
                edge_distance = d;
                edge = n.cell;
            }
        }
        for (const std::uint8_t dir : kOrder) {
            const std::int32_t nx = cx + step_x(dir);
            const std::int32_t ny = cy + step_y(dir);
            if (!inside(nx, ny)) {
                continue;
            }
            const std::size_t nc = at(nx, ny);
            if ((m_state[nc] & (kBlocked | kVisited)) != 0) {
                continue;
            }
            const bool diagonal = (dir & 1U) != 0;
            if (diagonal && ((m_state[at(cx + step_x(dir), cy)] & kBlocked) != 0 ||
                             (m_state[at(cx, cy + step_y(dir))] & kBlocked) != 0)) {
                continue;  // no cut corners
            }
            const std::uint8_t speed = m_speed[n.cell];
            const std::uint32_t cost = n.cost + (diagonal ? diagonal_cost(speed) : straight_cost(speed));
            if (cost < m_cost[nc]) {
                m_cost[nc] = cost;
                m_state[nc] = static_cast<std::uint8_t>((m_state[nc] & 0x0FU) | ((dir + 1U) << 4U));
                push(cost, nc);
            }
        }
    }
    const std::optional<std::size_t> end = goal ? goal : edge;
    if (!end) {
        return course;
    }
    course.reached_goal = goal.has_value();
    std::vector<std::uint8_t> dirs;
    std::size_t cell = *end;
    while (cell != start) {
        const auto from = static_cast<std::uint8_t>(m_state[cell] >> 4U);
        if (from == 0) {
            return LandCourse{};
        }
        const auto dir = static_cast<std::uint8_t>(from - 1U);
        dirs.push_back(dir);
        const auto cx = static_cast<std::int32_t>(cell % static_cast<std::size_t>(size)) + x0;
        const auto cy = static_cast<std::int32_t>(cell / static_cast<std::size_t>(size)) + y0;
        cell = at(cx - step_x(dir), cy - step_y(dir));
    }
    std::reverse(dirs.begin(), dirs.end());
    std::int32_t x = r.start_x;
    std::int32_t y = r.start_y;
    std::size_t i = 0;
    const std::uint8_t max_run = std::clamp<std::uint8_t>(r.max_run, 1, 15);
    while (i < dirs.size() && course.path.size() < kMaxRuns) {
        const std::uint8_t dir = dirs[i];
        const std::uint8_t speed = m_speed[at(x, y)];
        std::uint8_t n = 0;
        // A run: same direction and walking cost, at most Maxstepcnt cells (0x0046CF70).
        while (i < dirs.size() && dirs[i] == dir && n < max_run && m_speed[at(x, y)] == speed) {
            x += step_x(dir);
            y += step_y(dir);
            ++n;
            ++i;
        }
        course.path.push_back(path_code(dir, n));
    }
    if (i < dirs.size()) {
        course.reached_goal = false;
    }
    course.end_x = x;
    course.end_y = y;
    return course;
}

}  // namespace o1602::sim
