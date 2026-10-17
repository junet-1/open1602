// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/combat.hpp"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <unordered_map>

#include "o1602/sim/logistics.hpp"
#include "o1602/sim/sea_paths.hpp"

namespace o1602::sim {

namespace {

constexpr std::int32_t kEighth = kPosUnit / 8;  // 1/8 tile in position units

struct Knot {
    std::int32_t x;
    std::int32_t v;
};

/// Byte tables of the scorer (Table_InterpByte 0x00403370, built in 0x00442A90): linear between the
/// knots, the last value beyond them, the first below.
std::int32_t table(std::initializer_list<Knot> knots, std::int32_t x) noexcept {
    const Knot* prev = nullptr;
    for (const Knot& k : knots) {
        if (x <= k.x) {
            return prev == nullptr ? k.v : prev->v + (k.v - prev->v) * (x - prev->x) / (k.x - prev->x);
        }
        prev = &k;
    }
    return prev != nullptr ? prev->v : 0;
}

std::int32_t t1(std::int32_t x) noexcept {  // remaining health after my shot: prefer kills
    return x < 0 ? 30 : table({{0, 30}, {25, 17}, {64, 9}, {96, 5}, {128, 0}}, x);
}
std::int32_t t2(std::int32_t x) noexcept {  // my damage relative to its size
    return table({{0, 0}, {25, 8}, {64, 20}, {96, 50}, {128, 60}, {192, 40}, {256, 20}}, std::min(x, 256));
}
std::int32_t t3(std::int32_t x) noexcept {  // its threat to me
    return table({{0, 0}, {6, 12}, {12, 28}, {38, 45}, {64, 60}, {96, 50}, {128, 40}, {256, 30}}, std::min(x, 256));
}
std::int32_t t5(std::int32_t x) noexcept {  // distance beyond my range, 1/8 tile
    return table({{0, 40}, {24, 20}, {48, 10}, {96, 0}}, x);
}

std::uint64_t key_of(const Target& t) noexcept {
    const auto kind = static_cast<std::uint64_t>(t.kind) << 56U;
    if (t.kind == Target::Kind::Building) {
        return kind | std::uint64_t{t.island} << 32U | std::uint64_t{t.x & 0xFFFFU} << 16U | (t.y & 0xFFFFU);
    }
    return kind | t.ship;
}

}  // namespace

std::uint32_t Combat::scaled_damage(std::uint32_t hit, std::uint32_t health, std::uint32_t max_health) noexcept {
    const std::uint32_t h = std::min(health, max_health);
    return ((h * 85 / std::max<std::uint32_t>(1, max_health) + 43) * hit) >> 7U;
}

std::uint32_t Combat::cannon_factor(std::uint8_t cannons) noexcept {
    if (cannons == 0) {
        return 0;
    }
    return cannons <= 2 ? 1U : cannons <= 10 ? 2U : cannons <= 16 ? 4U : 6U;
}

Combat::Combat(World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures)
    : m_world(&world), m_buildings(&buildings), m_figures(&figures) {
    m_land.rebuild(world, buildings);
}

void Combat::island_changed(std::uint16_t island) {
    m_land.update_island(*m_world, *m_buildings, island);
}

std::optional<Fighter> Combat::fighter(const Target& unit) const {
    Fighter f;
    f.unit = unit;
    switch (unit.kind) {
        case Target::Kind::Soldier: {
            const Soldier* s = m_world->soldier(unit.ship);
            if (s == nullptr || s->away()) {
                return std::nullopt;
            }
            const assets::FigureType* type = m_figures->get(s->figure);
            if (type == nullptr) {
                return std::nullopt;
            }
            f.owner = s->owner;
            f.island = s->island;
            f.x8 = s->pos_x / kEighth;
            f.y8 = s->pos_y / kEighth;
            f.cells = {s->cell_x(), s->cell_y(), s->cell_x(), s->cell_y()};
            f.max_health = std::max<std::uint32_t>(1, type->combat.max_health);
            f.health = s->health != 0 ? s->health : f.max_health;
            f.range8 = static_cast<std::int32_t>(type->combat.range8);
            f.damage = scaled_damage(type->combat.hit, f.health, f.max_health);
            f.threat = f.damage * 1000 / std::max<std::uint32_t>(1, type->work_time_ms);
            f.cannon = type->kind == 4;  // FIGTYP_KANONIER
            return f;
        }
        case Target::Kind::Ship: {
            const Ship* s = m_world->ship(unit.ship);
            if (s == nullptr || s->sinking) {
                return std::nullopt;
            }
            const assets::FigureType* type = m_figures->get(s->figure);
            if (type == nullptr) {
                return std::nullopt;
            }
            f.owner = s->owner;
            f.island = s->near_island;
            f.x8 = s->pos_x / kEighth;
            f.y8 = s->pos_y / kEighth;
            f.cells = {2 * s->x, 2 * s->y, 2 * s->x + 1, 2 * s->y + 1};
            f.max_health = std::max<std::uint32_t>(1, type->combat.max_health);
            f.health = s->health != 0 ? std::min(s->health, f.max_health) : f.max_health;
            f.range8 = static_cast<std::int32_t>(type->combat.range8);
            f.damage = type->combat.hit * cannon_factor(s->cannons);
            f.threat = f.damage;  // per shot; soldiers and towers count per second (original)
            f.cannon = true;
            f.ship = true;
            return f;
        }
        case Target::Kind::Building: {
            // A tower by its building.
            for (const Tower& t : m_world->towers()) {
                if (t.island == unit.island && t.x == unit.x && t.y == unit.y) {
                    return tower_fighter(t);
                }
            }
            // Any other building: a target, not a shooter.
            if (unit.island >= m_world->islands().size()) {
                return std::nullopt;
            }
            const auto b = building_at(*m_world, *m_buildings, unit.island,
                                       {static_cast<std::int32_t>(unit.x), static_cast<std::int32_t>(unit.y)});
            if (!b) {
                return std::nullopt;
            }
            const WorldIsland& island = m_world->islands()[unit.island];
            const auto wx = static_cast<std::int32_t>(island.x) + b->origin.x;
            const auto wy = static_cast<std::int32_t>(island.y) + b->origin.y;
            const auto w = static_cast<std::int32_t>(b->width);
            const auto h = static_cast<std::int32_t>(b->height);
            f.unit = Target::building(unit.island, static_cast<std::uint32_t>(b->origin.x),
                                      static_cast<std::uint32_t>(b->origin.y));
            f.owner =
                island.map.top(static_cast<std::uint32_t>(b->origin.x), static_cast<std::uint32_t>(b->origin.y)).owner;
            f.island = unit.island;
            f.x8 = wx * 8 + w * 4;
            f.y8 = wy * 8 + h * 4;
            f.cells = {2 * wx, 2 * wy, 2 * (wx + w) - 1, 2 * (wy + h) - 1};
            f.max_health = std::max<std::uint32_t>(1, b->type->max_health);
            std::uint32_t damage = 0;
            for (const BuildingDamage& d : m_world->building_damage()) {
                if (d.island == unit.island && d.x == b->origin.x && d.y == b->origin.y) {
                    damage = d.damage;
                }
            }
            f.health = f.max_health > damage ? f.max_health - damage : 0;
            return f;
        }
        default:
            break;
    }
    return std::nullopt;
}

std::optional<Fighter> Combat::tower_fighter(const Tower& t) const {
    const assets::FigureType* type = m_figures->get(t.figure);
    if (type == nullptr || t.island >= m_world->islands().size()) {
        return std::nullopt;
    }
    Fighter f;
    f.unit = Target::building(t.island, t.x, t.y);
    const WorldIsland& island = m_world->islands()[t.island];
    const auto wx = static_cast<std::int32_t>(island.x + t.x);
    const auto wy = static_cast<std::int32_t>(island.y + t.y);
    const assets::BuildingType* b = m_land.building(wx, wy);
    const auto w = static_cast<std::int32_t>(b != nullptr ? b->size_x : 1);
    const auto h = static_cast<std::int32_t>(b != nullptr ? b->size_y : 1);
    f.owner = t.owner;
    f.island = t.island;
    f.x8 = wx * 8 + w * 4;
    f.y8 = wy * 8 + h * 4;
    f.cells = {2 * wx, 2 * wy, 2 * (wx + w) - 1, 2 * (wy + h) - 1};
    f.max_health = std::max<std::uint32_t>(1, type->combat.max_health);
    f.health = f.max_health;  // the tower figure never weakens (original)
    f.range8 = static_cast<std::int32_t>(type->combat.range8);
    f.damage = type->combat.hit;
    f.threat = f.damage * 1000 / std::max<std::uint32_t>(1, type->work_time_ms);
    f.cannon = true;
    return f;
}

std::size_t Combat::list_index(const Target& unit) const {
    const auto it = m_index.find(key_of(unit));
    return it != m_index.end() ? it->second : m_entries.size();
}

const std::vector<Neighbour>& Combat::hostiles(const Target& unit) const {
    static const std::vector<Neighbour> kNone;
    const std::size_t i = list_index(unit);
    return i < m_lists.size() ? m_lists[i] : kNone;
}

void Combat::add_pair(std::size_t a, std::size_t b) {
    const Entry& ea = m_entries[a];
    const Entry& eb = m_entries[b];
    const std::int32_t d = sea_distance(ea.x8 - eb.x8, ea.y8 - eb.y8);
    if (m_lists[a].size() < kMaxNeighbours) {
        m_lists[a].push_back({eb.unit, d});
    }
    if (m_lists[b].size() < kMaxNeighbours) {
        m_lists[b].push_back({ea.unit, d});
    }
}

void Combat::update() {
    m_entries.clear();
    for (const Ship& s : m_world->ships()) {
        if (!s.sinking && s.owner != kNoPlayer) {
            m_entries.push_back(
                {Target::other_ship(s.id), s.owner, s.near_island, s.pos_x / kEighth, s.pos_y / kEighth, true});
        }
    }
    for (const Soldier& s : m_world->soldiers()) {
        if (!s.away() && s.owner != kNoPlayer) {
            m_entries.push_back(
                {Target::soldier(s.id), s.owner, s.island, s.pos_x / kEighth, s.pos_y / kEighth, false});
        }
    }
    for (const Tower& t : m_world->towers()) {
        if (t.owner == kNoPlayer || t.island >= m_world->islands().size()) {
            continue;
        }
        if (const auto f = tower_fighter(t)) {
            m_entries.push_back({f->unit, t.owner, t.island, f->x8, f->y8, false});
        }
    }
    m_lists.assign(m_entries.size(), {});
    m_index.clear();
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        m_index.emplace(key_of(m_entries[i].unit), i);
    }
    // Sorted by y like the original, so the lists fill in its order (x, then the order above, on ties).
    std::vector<std::size_t> order(m_entries.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return m_entries[a].y8 < m_entries[b].y8; });
    const Diplomacy& d = m_world->diplomacy();
    constexpr std::int32_t kSight8 = kSight * 8;
    for (std::size_t i = 0; i < order.size(); ++i) {
        const Entry& a = m_entries[order[i]];
        for (std::size_t j = i + 1; j < order.size(); ++j) {
            const Entry& b = m_entries[order[j]];
            if (b.y8 - a.y8 > kSight8) {
                break;
            }
            if (std::abs(b.x8 - a.x8) > kSight8 || a.owner == b.owner || !d.hostile(a.owner, b.owner)) {
                continue;
            }
            // A ship sees land figures only near their island (rectangle + 5 tiles).
            if (a.ship != b.ship && a.island != b.island) {
                continue;
            }
            add_pair(order[i], order[j]);
        }
    }
}

std::vector<Candidate> Combat::rank(const Fighter& me, const std::vector<Neighbour>& list,
                                    std::int32_t max_distance8) const {
    std::vector<Candidate> out;
    for (const Neighbour& n : list) {
        if (n.distance8 > max_distance8) {
            continue;
        }
        const auto t = fighter(n.unit);
        if (!t) {
            continue;
        }
        const auto my_damage = static_cast<std::int32_t>(me.damage);
        const auto t_max = static_cast<std::int32_t>(t->max_health);
        const std::int32_t over = std::max(0, n.distance8 - me.range8);
        std::int32_t score = t1((static_cast<std::int32_t>(t->health) - my_damage) * 128 / t_max);
        score += t2(my_damage * 128 / t_max);
        if (n.distance8 <= t->range8) {
            score += t3(static_cast<std::int32_t>(t->threat * 128 / std::max<std::uint32_t>(1, me.max_health)));
        }
        if (t->range8 < me.range8) {
            score += 35;
        }
        score += t5(over);
        out.push_back({n.unit, n.distance8, score, over < 4});
    }
    std::stable_sort(out.begin(), out.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
    return out;
}

void Combat::shoot(const Fighter& shooter, const Fighter& target, std::uint32_t delay_ms) {
    PendingHit h;
    h.due_ms = m_world->time_ms() + delay_ms;
    h.shooter = shooter.unit;
    h.attacker = shooter.owner;
    h.cannon = shooter.cannon;
    h.damage = shooter.damage;
    h.target = target.unit;
    h.from_x = shooter.x8 * kEighth;
    h.from_y = shooter.y8 * kEighth;
    m_world->pending_hits().push_back(h);
}

}  // namespace o1602::sim
