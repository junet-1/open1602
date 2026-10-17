// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/ambient.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

namespace o1602::render {

namespace {

// HAUS Kind values.
constexpr std::int32_t kRoad = 1;
constexpr std::int32_t kGate = 3;
constexpr std::int32_t kSquare = 13;
constexpr std::int32_t kBridge = 18;
constexpr std::int32_t kPier = 30;
constexpr std::int32_t kBeachInnerCorner = 26;  // STRANDECKI
// HAUS_PRODTYP Kind values.
constexpr std::int32_t kProdMarket = 7;
constexpr std::int32_t kProdRipe = 9;
constexpr std::uint8_t kWareTree = 53;

// Townsfolk (figuren.cod).
constexpr std::uint16_t kAdelweibl = 93;
constexpr std::uint16_t kAdel = 94;
constexpr std::uint16_t kAlter = 95;
constexpr std::uint16_t kFrau = 96;
constexpr std::uint16_t kPassant = 97;
constexpr std::uint16_t kVeteran = 98;
constexpr std::uint16_t kKind = 99;
constexpr std::uint16_t kPilger = 100;

constexpr std::uint32_t kWalkRadius = 10;  // the 21 x 21 search window around the house
constexpr std::uint32_t kStraight = 64;    // one step of the original's wave search
constexpr std::uint32_t kDiagonal = 90;
constexpr std::uint32_t kWalkerTries = 10;      // per city round (0x00480370)
constexpr std::uint32_t kHouseBucket = 1087;    // records of the original's house list a try picks from
constexpr std::uint32_t kMinSettlers = 30;      // settlers and better for walkers
constexpr std::uint32_t kJugglerCitizens = 59;  // citizens and better for a juggler: more than this
constexpr std::uint32_t kSeaTries = 3;
constexpr std::int32_t kShipNear = 5;      // no creature this close to a ship
constexpr std::int32_t kShipSeen = 10;     // turns away from ships this close
constexpr std::int32_t kIslandSeen = 5;    // turns away from islands this close
constexpr std::uint32_t kBirdTrees = 25;   // a bird emitter on every 25th tree
constexpr std::uint32_t kSurfCorners = 4;  // a surf emitter on every 4th inner beach corner
constexpr std::uint32_t kBirdPeriodMs = 32'000;
constexpr std::uint32_t kSurfPeriodMs = 9'000;
constexpr std::uint32_t kBirdVariants = 6;

constexpr std::uint64_t key3(std::uint32_t island, std::uint32_t x, std::uint32_t y) noexcept {
    return (std::uint64_t{island} << 32) | (std::uint64_t{y} << 16) | x;
}

std::uint32_t tile_hash(std::uint32_t island, std::uint32_t x, std::uint32_t y) noexcept {
    std::uint32_t h = island * 0x9E3779B1U ^ x * 0x85EBCA77U ^ y * 0xC2B2AE3DU;
    h ^= h >> 15;
    h *= 0x2C1B3C6DU;
    h ^= h >> 12;
    return h;
}

const assets::BuildingType* type_at(const sim::IslandMap& map, const assets::BuildingTable& buildings, std::int32_t x,
                                    std::int32_t y) {
    if (!map.contains(x, y)) {
        return nullptr;
    }
    const sim::MapCell& c = map.top(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    return c.empty() ? nullptr : buildings.by_sprite(c.sprite);
}

/// Tiles townsfolk are seen on (0x00451590): roads, ground, ruins, squares, bridges, piers; on
/// building tiles (leaving the house, entering the target) they are hidden.
bool walker_visible_on(const assets::BuildingType* b) noexcept {
    if (b == nullptr) {
        return false;
    }
    switch (b->kind) {
        case 1:
        case 11:
        case 12:
        case 13:
        case 17:
        case 18:
        case 29:
        case 30:
            return true;
        default:
            return false;
    }
}

std::uint32_t inhabitants_from(const sim::City& c, std::size_t first) {
    std::uint32_t n = 0;
    for (std::size_t k = first; k < c.inhabitants.size(); ++k) {
        n += c.inhabitants[k];
    }
    return n;
}

/// Direction code 1..8 (N, NE, E, SE, S, SW, W, NW) away from the blocked sides
/// (bit 0 +y, bit 1 -x, bit 2 -y, bit 3 +x); 0 = free choice, 15 = nowhere (0x00498EFC, 0x00498F0C).
constexpr std::array<std::uint8_t, 16> kAwayDolphin = {0, 1, 3, 2, 5, 3, 4, 3, 7, 8, 1, 1, 6, 7, 5, 0xFF};
constexpr std::array<std::uint8_t, 16> kAwayWhale = {0, 1, 3, 3, 5, 3, 5, 3, 7, 1, 1, 1, 7, 7, 5, 0xFF};
/// Species by rand() & 7 (0x00498F1C): dolphins 5 of 8, whales 2, a kraken 1.
constexpr std::array<std::uint16_t, 8> kSpecies = {kFigureDolphin, kFigureKraken,  kFigureDolphin, kFigureDolphin,
                                                   kFigureWhale,   kFigureDolphin, kFigureWhale,   kFigureDolphin};

AnimState fresh_anim(std::uint8_t anim, std::uint32_t seed) noexcept {
    AnimState s;
    s.anim = anim;
    s.random = seed | 1U;
    return s;
}

}  // namespace

std::array<std::uint16_t, 4> townsfolk_for(std::int32_t kind) noexcept {
    switch (kind) {
        case 7:  // MARKT
            return {kAlter, kFrau, kKind, kAdelweibl};
        case 17:  // WIRT
            return {kAlter, kAdel, kPilger, kAdelweibl};
        case 18:  // KAPELLE
            return {kAdel, kAlter, kFrau, kPilger};
        case 19:  // KIRCHE
            return {kAdel, kPassant, kPilger, kAdelweibl};
        case 20:  // BADEHAUS
        case 21:  // THEATER
            return {kAdel, kPassant, kFrau, kAdelweibl};
        case 22:  // KLINIK
            return {kAlter, kFrau, kVeteran, kPassant};
        case 23:  // SCHULE
            return {kFrau, kKind, kPassant, kAdelweibl};
        case 24:  // HOCHSCHULE
            return {kPassant, kAdel, kPassant, kFrau};
        default:
            return {kFrau, kFrau, kFrau, kFrau};
    }
}

std::vector<std::int32_t> walker_targets(std::uint32_t roll) {
    switch (roll & 3U) {
        case 0:
            return {17, 22};  // tavern, doctor
        case 1:
            return {7, 24};  // market, university
        case 2:
            return {18, 19, 21};  // chapel, church, theatre
        default:
            return {20, 23, 24};  // bath house, school, university
    }
}

void Ambient::update(const sim::World& world, const assets::BuildingTable& buildings,
                     const assets::FigureTable& figures, const AmbientView& view, std::uint32_t dt_ms) {
    m_time_ms += dt_ms;
    // Townsfolk walk on.
    for (Walker& w : m_walking) {
        w.position += w.tiles_per_ms * dt_ms;
        if (const assets::FigureType* type = figures.get(w.figure)) {
            advance_animation(w.anim, *type, dt_ms);
        }
    }
    std::erase_if(m_walking, [&](const Walker& w) {
        const bool done = w.position >= static_cast<double>(w.path.size() - 1) || w.island >= world.islands().size();
        if (done) {
            m_busy_houses.erase(w.house);
        }
        return done;
    });
    // Sea creatures swim on and go.
    for (Creature& c : m_creatures) {
        c.x += c.vx * dt_ms;
        c.y += c.vy * dt_ms;
        c.life_ms = c.life_ms > dt_ms ? c.life_ms - dt_ms : 0;
        if (const assets::FigureType* type = figures.get(c.figure)) {
            advance_animation(c.anim, *type, dt_ms);
        }
    }
    std::erase_if(m_creatures, [](const Creature& c) { return c.life_ms == 0; });

    // City rounds every 10 s.
    for (const sim::City& c : world.cities()) {
        const std::uint32_t key = std::uint32_t{c.island} << 8 | c.index;
        auto [it, fresh] = m_city_ms.try_emplace(key, 0);
        if (fresh) {
            it->second = m_random.below(kCityPeriodMs);  // cities do not all go at once
        }
        if (it->second > dt_ms) {
            it->second -= dt_ms;
            continue;
        }
        it->second = kCityPeriodMs - (dt_ms - it->second) % kCityPeriodMs;
        const bool seen = c.island < view.islands.size() && view.islands[c.island];
        if (!seen || inhabitants_from(c, 1) < kMinSettlers) {
            continue;
        }
        // No townsfolk while soldiers of another player stand on the island (0x00480370).
        bool troops = false;
        for (const sim::Soldier& s : world.soldiers()) {
            if (s.owner == c.owner || s.owner == sim::kNoPlayer || s.x < 0 || s.y < 0) {
                continue;
            }
            const auto sx = static_cast<std::uint32_t>(s.x);
            const auto sy = static_cast<std::uint32_t>(s.y);
            const sim::Player* p = world.player(s.owner);
            troops = troops || (world.contains(sx, sy) && world.cell(sx, sy).island == c.island && p != nullptr &&
                                (p->role == sim::PlayerRole::Human || p->role == sim::PlayerRole::Computer));
        }
        if (troops) {
            continue;
        }
        std::vector<const sim::House*> houses;
        for (const sim::House& h : world.houses()) {
            if (h.island == c.island && h.city == c.index) {
                houses.push_back(&h);
            }
        }
        // The original picks 10 records of a house list shared by several islands, so a city gets
        // about 10 * houses / 1087 walkers per round; Open1602 keeps that rate but always finds a
        // house of the city.
        for (std::uint32_t t = 0; t < kWalkerTries && !houses.empty(); ++t) {
            if (m_random.below(kHouseBucket) >= houses.size()) {
                continue;
            }
            const sim::House& h = *houses[m_random.below(static_cast<std::uint32_t>(houses.size()))];
            send_walker(world, buildings, figures, h, m_random.below(4));
        }
    }

    // A chance for sea creatures every 5 s.
    m_sea_ms += dt_ms;
    while (m_sea_ms >= kSeaPeriodMs) {
        m_sea_ms -= kSeaPeriodMs;
        sea_creatures(world, figures, view);
    }

    jugglers(world, buildings, figures, view, dt_ms);
    sounds(world, buildings, view, dt_ms);

    // Pictures.
    m_walker_instances.clear();
    for (const Walker& w : m_walking) {
        const auto step = static_cast<std::size_t>(w.position);
        const double t = w.position - static_cast<double>(step);
        const sim::TilePos a = w.path[step];
        const sim::TilePos b = w.path[std::min(step + 1, w.path.size() - 1)];
        const sim::IslandMap& map = world.islands()[w.island].map;
        const sim::TilePos on = t < 0.5 ? a : b;
        if (!walker_visible_on(type_at(map, buildings, on.x, on.y))) {
            continue;
        }
        const sim::WorldIsland& island = world.islands()[w.island];
        FigureInstance f;
        f.x = static_cast<double>(island.x) + a.x + 0.5 + (b.x - a.x) * t;
        f.y = static_cast<double>(island.y) + a.y + 0.5 + (b.y - a.y) * t;
        f.figure = w.figure;
        f.anim = w.anim.anim;
        f.frame = w.anim.frame;
        f.direction = sim::direction_between(a, b);
        f.sheet = assets::FigureSheet::Traeger;
        m_walker_instances.push_back(f);
    }
    std::erase_if(m_effects, [](const EffectFigure& e) { return e.sheet == assets::FigureSheet::Fische; });
    for (const Creature& c : m_creatures) {
        EffectFigure e;
        e.x = c.x;
        e.y = c.y;
        e.figure = c.figure;
        e.anim = c.anim.anim;
        e.frame = c.anim.frame;
        e.direction = c.direction;
        e.sheet = assets::FigureSheet::Fische;
        m_effects.push_back(e);
    }
}

void Ambient::send_walker(const sim::World& world, const assets::BuildingTable& buildings,
                          const assets::FigureTable& figures, const sim::House& house, std::uint32_t roll) {
    const std::uint64_t key = key3(house.island, house.x, house.y);
    if (m_busy_houses.contains(key)) {
        return;  // one walker per house
    }
    const sim::IslandMap& map = world.islands()[house.island].map;
    const assets::BuildingType* home = type_at(map, buildings, house.x, house.y);
    if (home == nullptr) {
        return;
    }
    const sim::MapCell& home_cell = map.top(house.x, house.y);
    std::uint32_t hw = 0;
    std::uint32_t hh = 0;
    sim::rotated_size(*home, home_cell.rotation, hw, hh);
    const auto cx = static_cast<std::int32_t>(house.x + (hw - 1) / 2);
    const auto cy = static_cast<std::int32_t>(house.y + (hh - 1) / 2);
    const std::vector<std::int32_t> targets = walker_targets(roll);
    const std::uint32_t min_cost = (m_random.below(8) + 5) * kStraight;  // 5..12 steps on the road

    // Wave search on the roads of a 21 x 21 window (0x0046F000 grid, 0x0046C7D0 search): roads,
    // squares, bridges, piers and the middle of gates are open; buildings of this city of the
    // chosen kinds are goals; the first goal at least `min_cost` away is taken, nearer ones are
    // dead ends.
    constexpr std::int32_t kSide = 2 * kWalkRadius + 1;
    const std::int32_t x0 = cx - static_cast<std::int32_t>(kWalkRadius);
    const std::int32_t y0 = cy - static_cast<std::int32_t>(kWalkRadius);
    enum : std::uint8_t { kClosed, kOpen, kGoal };
    std::array<std::uint8_t, kSide * kSide> grid{};
    for (std::int32_t j = 0; j < kSide; ++j) {
        for (std::int32_t i = 0; i < kSide; ++i) {
            const std::int32_t x = x0 + i;
            const std::int32_t y = y0 + j;
            const assets::BuildingType* b = type_at(map, buildings, x, y);
            if (b == nullptr) {
                continue;
            }
            std::uint8_t& g = grid[static_cast<std::size_t>(j * kSide + i)];
            if (b->kind == kRoad || b->kind == kSquare || b->kind == kBridge || b->kind == kPier) {
                g = kOpen;
            } else if (b->kind == kGate) {
                const sim::SubTile sub =
                    map.sub_tile(buildings, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                std::uint32_t gw = 0;
                std::uint32_t gh = 0;
                sim::rotated_size(*b, map.top(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)).rotation,
                                  gw, gh);
                if (x == sub.origin_x + static_cast<std::int32_t>(gw / 2) &&
                    y == sub.origin_y + static_cast<std::int32_t>(gh / 2)) {
                    g = kOpen;
                }
            }
            if (std::find(targets.begin(), targets.end(), b->prod_kind) != targets.end() &&
                map.top(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)).city == house.city) {
                g = kGoal;
            }
        }
    }
    constexpr std::uint32_t kInf = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint32_t, kSide * kSide> cost{};
    std::array<std::int32_t, kSide * kSide> from{};
    cost.fill(kInf);
    from.fill(-1);
    using Item = std::pair<std::uint32_t, std::int32_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> queue;
    for (std::uint32_t j = 0; j < hh; ++j) {
        for (std::uint32_t i = 0; i < hw; ++i) {
            const std::int32_t gx = static_cast<std::int32_t>(house.x + i) - x0;
            const std::int32_t gy = static_cast<std::int32_t>(house.y + j) - y0;
            if (gx >= 0 && gy >= 0 && gx < kSide && gy < kSide) {
                const std::int32_t n = gy * kSide + gx;
                cost[static_cast<std::size_t>(n)] = 0;
                grid[static_cast<std::size_t>(n)] = kOpen;  // the house is the start
                queue.emplace(0, n);
            }
        }
    }
    std::int32_t goal = -1;
    while (!queue.empty()) {
        const auto [d, n] = queue.top();
        queue.pop();
        if (d != cost[static_cast<std::size_t>(n)]) {
            continue;
        }
        if (grid[static_cast<std::size_t>(n)] == kGoal) {
            if (d >= min_cost) {
                goal = n;
                break;
            }
            continue;  // too near: a dead end
        }
        const std::int32_t gx = n % kSide;
        const std::int32_t gy = n / kSide;
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const std::int32_t nx = gx + dx;
                const std::int32_t ny = gy + dy;
                if (nx < 0 || ny < 0 || nx >= kSide || ny >= kSide) {
                    continue;
                }
                const std::int32_t m = ny * kSide + nx;
                if (grid[static_cast<std::size_t>(m)] == kClosed) {
                    continue;
                }
                if (dx != 0 && dy != 0 &&
                    (grid[static_cast<std::size_t>(gy * kSide + nx)] == kClosed ||
                     grid[static_cast<std::size_t>(ny * kSide + gx)] == kClosed)) {
                    continue;  // no cutting corners
                }
                const std::uint32_t nd = d + (dx != 0 && dy != 0 ? kDiagonal : kStraight);
                if (nd < cost[static_cast<std::size_t>(m)]) {
                    cost[static_cast<std::size_t>(m)] = nd;
                    from[static_cast<std::size_t>(m)] = n;
                    queue.emplace(nd, m);
                }
            }
        }
    }
    if (goal < 0) {
        return;  // nowhere to go (the original makes the figure and frees it at once)
    }
    Walker w;
    w.island = house.island;
    w.house = key;
    for (std::int32_t n = goal; n >= 0; n = from[static_cast<std::size_t>(n)]) {
        w.path.push_back(sim::TilePos{x0 + n % kSide, y0 + n / kSide});
    }
    std::reverse(w.path.begin(), w.path.end());
    const assets::BuildingType* target = type_at(map, buildings, w.path.back().x, w.path.back().y);
    w.figure = townsfolk_for(target != nullptr ? target->prod_kind : 0)[m_random.below(4)];
    const assets::FigureType* type = figures.get(w.figure);
    if (type == nullptr || w.path.size() < 2) {
        return;
    }
    // Speed * 0.002 tiles per second on the road (Wegspeed 100 %).
    w.tiles_per_ms = type->speed * 0.000002;
    w.anim = start_animation(*type, 0, m_random.next());
    m_busy_houses.insert(key);
    m_walking.push_back(std::move(w));
    ++m_stats.walkers;
}

void Ambient::sea_creatures(const sim::World& world, const assets::FigureTable& figures, const AmbientView& view) {
    if (!view.tile_at) {
        return;
    }
    for (std::uint32_t attempt = 0; attempt < kSeaTries; ++attempt) {
        // A random tile of the screen (0x00487F10), outside every island's rectangle.
        const double u = m_random.below(1024) / 1024.0;
        const double v = m_random.below(1024) / 1024.0;
        const auto tile = view.tile_at(u, v);
        if (!tile || !world.contains(static_cast<std::uint32_t>(tile->x), static_cast<std::uint32_t>(tile->y)) ||
            world.cell(static_cast<std::uint32_t>(tile->x), static_cast<std::uint32_t>(tile->y)).island !=
                sim::World::kSea) {
            continue;
        }
        const std::uint16_t species = kSpecies[m_random.below(8)];
        // Sides to keep away from: ships and islands near the spot (0x0044A200).
        std::uint32_t blocked = 0;
        bool near_ship = false;
        for (const sim::Ship& s : world.ships()) {
            const std::int32_t dx = static_cast<std::int32_t>(s.x) - tile->x;
            const std::int32_t dy = static_cast<std::int32_t>(s.y) - tile->y;
            if (std::abs(dx) >= kShipSeen || std::abs(dy) >= kShipSeen) {
                continue;
            }
            if (std::abs(dx) <= kShipNear && std::abs(dy) <= kShipNear) {
                near_ship = true;
                break;
            }
            if (std::abs(dx) > std::abs(dy)) {
                blocked |= dx > 0 ? 8U : 2U;
            } else {
                blocked |= dy > 0 ? 1U : 4U;
            }
        }
        if (near_ship) {
            continue;
        }
        for (const sim::WorldIsland& island : world.islands()) {
            const auto left = static_cast<std::int32_t>(island.x);
            const auto top = static_cast<std::int32_t>(island.y);
            const std::int32_t right = left + static_cast<std::int32_t>(island.map.width()) - 1;
            const std::int32_t bottom = top + static_cast<std::int32_t>(island.map.height()) - 1;
            if (tile->x < left - kIslandSeen || tile->x > right + kIslandSeen || tile->y < top - kIslandSeen ||
                tile->y > bottom + kIslandSeen) {
                continue;
            }
            blocked |= (tile->x <= left ? 8U : 0U) | (tile->x >= right ? 2U : 0U) | (tile->y <= top ? 1U : 0U) |
                       (tile->y >= bottom ? 4U : 0U);
        }
        const bool whale = species == kFigureWhale;
        std::uint8_t code = (whale ? kAwayWhale : kAwayDolphin)[blocked & 15U];
        if (code == 0xFF) {
            continue;
        }
        if (code == 0) {
            code = static_cast<std::uint8_t>(whale ? m_random.below(4) * 2 + 1 : m_random.below(8) + 1);
        }
        const auto direction = static_cast<std::uint8_t>(code - 1);
        const assets::FigureType* type = figures.get(species);
        if (type == nullptr || type->speed == 0) {
            return;
        }
        // Worktime runs down at Speed per 50 ms (0x00451890).
        const auto life = static_cast<std::uint32_t>(std::uint64_t{type->work_time_ms} * 500 / type->speed);
        const double x = tile->x + 0.5;
        const double y = tile->y + 0.5;
        const auto add = [&](double cx, double cy, std::uint8_t anim) {
            Creature c;
            c.figure = species;
            c.x = cx;
            c.y = cy;
            c.direction = direction;
            c.life_ms = life;
            c.anim = fresh_anim(anim, m_random.next());
            m_creatures.push_back(c);
        };
        if (species == kFigureDolphin) {
            // A pod: one or two companions with later jumps, then the leader.
            add(x, y, 2);
            if (m_random.below(2) != 0) {
                add(x + 1.0, y + 0.2, 3);
            }
            add(x - 1.0, y + 0.7, 0);
        } else {
            add(x, y, 0);
            if (whale) {
                // Swims along a world axis: Speed * 0.002 tiles per second.
                static constexpr std::array<std::pair<int, int>, 8> kStep = {
                    {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}};
                const double pace = type->speed * 0.000002;
                m_creatures.back().vx = kStep[direction].first * pace;
                m_creatures.back().vy = kStep[direction].second * pace;
            }
        }
        ++m_stats.creatures;
        return;
    }
}

void Ambient::jugglers(const sim::World& world, const assets::BuildingTable& buildings,
                       const assets::FigureTable& figures, const AmbientView& view, std::uint32_t dt_ms) {
    std::erase_if(m_effects, [](const EffectFigure& e) { return e.sheet == assets::FigureSheet::Gaukler; });
    const assets::FigureType* fig = figures.get(kFigureJuggler);
    if (fig == nullptr) {
        return;
    }
    std::unordered_set<std::uint64_t> seen;
    for (const sim::Production& p : world.productions()) {
        if (!p.used || p.island >= view.islands.size() || !view.islands[p.island]) {
            continue;
        }
        const sim::WorldIsland& island = world.islands()[p.island];
        const sim::MapCell& cell = island.map.top(p.x, p.y);
        const assets::BuildingType* b = cell.empty() ? nullptr : buildings.by_sprite(cell.sprite);
        if (b == nullptr || b->prod_kind != kProdMarket || b->attached_figures[1] != kFigureJuggler) {
            continue;
        }
        const sim::City* city = world.city(p.island, cell.city);
        if (city == nullptr || inhabitants_from(*city, 2) <= kJugglerCitizens) {
            continue;
        }
        // One per market; its act (ANIM) is (x + y) % 5 of the market (0x004466A0).
        const std::uint64_t key = key3(p.island, p.x, p.y);
        seen.insert(key);
        const auto act = static_cast<std::uint8_t>((p.x + p.y) % 5);
        auto [it, fresh] = m_juggler_anims.try_emplace(key);
        if (fresh) {
            it->second = start_animation(*fig, act, tile_hash(p.island, p.x, p.y));
        } else {
            advance_animation(it->second, *fig, dt_ms);
        }
        EffectFigure e;
        building_attach_point(island, *b, p.x, p.y, cell.rotation, *fig, e.x, e.y);
        e.z = fig->fahnoffs[2];
        e.figure = kFigureJuggler;
        e.anim = it->second.anim;
        e.frame = it->second.frame;
        e.direction = static_cast<std::uint8_t>(((cell.rotation + 1U) & 3U) * 2U);
        e.sheet = assets::FigureSheet::Gaukler;
        m_effects.push_back(e);
    }
    std::erase_if(m_juggler_anims, [&](const auto& kv) { return !seen.contains(kv.first); });
}

void Ambient::sounds(const sim::World& world, const assets::BuildingTable& buildings, const AmbientView& view,
                     std::uint32_t dt_ms) {
    // Birds in every 25th tree and surf at every 4th inner beach corner of the islands on screen;
    // the original counts them in the order it looks at the screen, so they move when it scrolls;
    // here each tile decides by itself. Each one sounds in its own cycle; only at normal speed.
    if (view.game_speed != 1 || dt_ms == 0) {
        return;
    }
    const std::uint64_t before = m_time_ms - dt_ms;
    m_emitters.resize(world.islands().size());
    for (std::uint16_t i = 0; i < world.islands().size() && i < view.islands.size(); ++i) {
        if (!view.islands[i]) {
            continue;
        }
        const sim::WorldIsland& island = world.islands()[i];
        IslandEmitters& em = m_emitters[i];
        if (!em.built || m_time_ms - em.built_ms >= kCityPeriodMs) {
            // Trees are felled and planted: look again from time to time.
            em.built = true;
            em.built_ms = m_time_ms;
            em.list.clear();
            for (std::uint32_t y = 0; y < island.map.height(); ++y) {
                for (std::uint32_t x = 0; x < island.map.width(); ++x) {
                    const sim::MapCell& cell = island.map.top(x, y);
                    const assets::BuildingType* b = cell.empty() ? nullptr : buildings.by_sprite(cell.sprite);
                    if (b == nullptr) {
                        continue;
                    }
                    const bool tree = b->prod_kind == kProdRipe && b->ware == kWareTree;
                    const bool corner = b->kind == kBeachInnerCorner;
                    const std::uint32_t h = tile_hash(i, x, y);
                    if ((tree && h % kBirdTrees == 0) || (corner && h % kSurfCorners == 0)) {
                        em.list.push_back(
                            Emitter{static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), tree, h});
                    }
                }
            }
        }
        for (const Emitter& e : em.list) {
            const std::uint32_t period = e.birds ? kBirdPeriodMs : kSurfPeriodMs;
            const std::uint64_t phase = (e.hash >> 8) % period;
            if ((before + phase) / period == (m_time_ms + phase) / period) {
                continue;
            }
            AmbientSound s;
            s.x = island.x + e.x + 0.5;
            s.y = island.y + e.y + 0.5;
            if (e.birds) {
                s.kind = AmbientSound::Kind::Birds;
                s.variant = static_cast<std::uint8_t>(m_random.below(kBirdVariants));
            } else if (m_random.below(2) == 0) {
                s.kind = AmbientSound::Kind::Surf;
            } else {
                continue;
            }
            m_sounds.push_back(s);
        }
    }
}

}  // namespace o1602::render
