// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the computer players read from the world (docs/re/ai.md): tiles, cities, land, money.
#include <algorithm>

#include "ai_engine.hpp"

#include "o1602/sim/growth.hpp"

namespace o1602::sim::ai {

namespace {

constexpr std::uint8_t kInfra2D = 19;  // INFRA_STUFE_2D
constexpr std::uint8_t kInfra3C = 25;
constexpr std::uint8_t kInfra4B = 30;
constexpr std::uint8_t kInfra5A = 31;

}  // namespace

bool Engine::inside(std::uint16_t island, std::int32_t x, std::int32_t y) const {
    return island < m_world.islands().size() && m_world.islands()[island].map.contains(x, y);
}

Tile Engine::tile(std::uint16_t island, std::int32_t x, std::int32_t y) const {
    Tile t;
    if (!inside(island, x, y)) {
        return t;
    }
    const IslandMap& map = m_world.islands()[island].map;
    const MapCell& c = map.top(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    t.city = c.city;
    t.owner = c.owner;
    if (c.empty()) {
        return t;
    }
    const assets::BuildingType* b = m_buildings.by_sprite(c.sprite);
    if (b == nullptr) {
        return t;
    }
    const SubTile st = map.sub_tile(m_buildings, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    t.ox = st.origin_x;
    t.oy = st.origin_y;
    t.origin = st.origin_x == x && st.origin_y == y;
    // Growing and withered crops count as the ripe crop (the original adds or takes one record).
    const assets::BuildingType* n = b;
    if (b->prod_kind == prod::kGrowing) {
        const assets::BuildingType* r = b->withered ? ripe_of_withered(m_buildings, *b) : ripe_record(m_buildings, *b);
        if (r != nullptr) {
            n = r;
        }
    }
    t.type = n;
    t.kind = n->kind;
    t.prod = n->prod_kind;
    t.ware = n->ware;
    t.raw = n->raw_ware;
    t.growth = static_cast<std::uint8_t>(std::uint32_t{n->edge_growth} * 32U / 128U);
    return t;
}

City* Engine::current_city(AiPlayer& p) {
    return p.has_city ? city(p.island, p.city) : nullptr;
}

const City* Engine::current_city(const AiPlayer& p) const {
    return p.has_city ? city(p.island, p.city) : nullptr;
}

City* Engine::home_city(AiPlayer& p) {
    if (!p.home_valid) {
        return nullptr;
    }
    City* c = city(p.home_island, p.home_city);
    return c != nullptr && c->owner == p.player ? c : nullptr;
}

CityPlan& Engine::plan_of(AiPlayer& p, std::uint16_t island, std::uint8_t c) {
    for (CityPlan& r : p.plans) {
        if (r.island == island && r.city == c) {
            return r;
        }
    }
    CityPlan r;
    r.island = island;
    r.city = c;
    r.stage_ms = now_ms();
    p.plans.push_back(r);
    return p.plans.back();
}

CityPlan* Engine::current_plan(AiPlayer& p) {
    if (!p.has_city) {
        return nullptr;
    }
    return &plan_of(p, p.island, p.city);
}

std::uint32_t Engine::own_city_count(PlayerId owner) const {
    std::uint32_t n = 0;
    for (const City& c : m_world.cities()) {
        n += c.owner == owner ? 1U : 0U;
    }
    return n;
}

std::uint32_t Engine::population(const City& c, std::size_t from) const {
    std::uint32_t n = 0;
    for (std::size_t k = from; k < c.inhabitants.size(); ++k) {
        n += c.inhabitants[k];
    }
    return n;
}

std::uint32_t Engine::demand(const City& c, std::uint8_t w) const {
    // 0x0047F7B0: food by all inhabitants, the other consumer goods by the classes that want them.
    if (w == m_buildings.food_ware()) {
        return static_cast<std::uint32_t>((std::uint64_t{population(c)} * m_buildings.food_need() * 6) >> 8);
    }
    std::uint64_t sum = 0;
    const auto& classes = m_buildings.resident_classes();
    for (std::size_t k = 0; k < classes.size() && k < c.inhabitants.size(); ++k) {
        for (const assets::ResidentClass::Need& n : classes[k].needs) {
            if (n.ware == w) {
                sum += std::uint64_t{c.inhabitants[k]} * n.amount;
            }
        }
    }
    return static_cast<std::uint32_t>((sum * 6) >> 8);
}

std::uint32_t Engine::kontor_level(const City& c) const {
    if (!c.warehouse_x || !c.warehouse_y) {
        return 0;
    }
    const Tile t = tile(c.island, *c.warehouse_x, *c.warehouse_y);
    if (!t.valid() || t.prod != prod::kKontor || t.type->destroyed) {
        return 0;
    }
    if (t.type->id >= id::kKontor1 && t.type->id <= id::kKontor1 + 6) {
        return (t.type->id - id::kKontor1) / 2 + 1;
    }
    return 1;
}

std::uint32_t Engine::market_count(const City& c) const {
    std::uint32_t n = 0;
    const IslandMap& map = m_world.islands()[c.island].map;
    for (const Production& pr : m_world.productions()) {
        if (!pr.used || pr.island != c.island || !map.contains(pr.x, pr.y)) {
            continue;
        }
        const MapCell& cell = map.top(pr.x, pr.y);
        if (cell.city != c.index || cell.empty()) {
            continue;
        }
        const assets::BuildingType* b = m_buildings.by_sprite(cell.sprite);
        if (b != nullptr && (b->prod_kind == prod::kMarket || b->prod_kind == prod::kKontor) && !b->destroyed) {
            ++n;
        }
    }
    return n;
}

std::uint32_t Engine::building_count(const City& c) const {
    std::uint32_t n = 0;
    const IslandMap& map = m_world.islands()[c.island].map;
    for (const Production& pr : m_world.productions()) {
        if (pr.used && pr.island == c.island && map.contains(pr.x, pr.y) && map.top(pr.x, pr.y).city == c.index) {
            ++n;
        }
    }
    for (const House& h : m_world.houses()) {
        if (h.island == c.island && h.city == c.index) {
            ++n;
        }
    }
    return n;
}

std::uint32_t Engine::free_land(std::uint16_t island, std::uint8_t own, std::uint32_t* percent) const {
    const IslandMap& map = m_world.islands()[island].map;
    const std::uint32_t total = map.width() * map.height();
    std::int64_t n = total;
    for (std::uint32_t y = 0; y < map.height(); ++y) {
        for (std::uint32_t x = 0; x < map.width(); ++x) {
            const MapCell& c = map.top(x, y);
            if (c.city != MapCell::kNoCity && c.city != own) {
                --n;
                continue;
            }
            const assets::BuildingType* b = c.empty() ? nullptr : m_buildings.by_sprite(c.sprite);
            if (b == nullptr) {
                continue;
            }
            switch (b->kind) {
                case kind::kWallBeach:
                case kind::kTowerBeach:
                case kind::kRock:
                case kind::kSea:
                case kind::kSurf:
                case kind::kSurfCorner:
                case kind::kMouth:
                case kind::kBeach:
                case kind::kBeachVariant:
                case kind::kBeachRuin:
                case kind::kPier:
                case kind::kSlope:
                case kind::kSlopeCorner:
                    n -= 1;
                    break;
                case kind::kRiver:
                case kind::kRiverCorner:
                case kind::kBridge:
                    n -= 4;
                    break;
                case kind::kBeachOuter:
                case kind::kBeachInner:
                    n -= 2;
                    break;
                default:
                    break;
            }
        }
    }
    const auto result = static_cast<std::uint32_t>(std::max<std::int64_t>(n, 0));
    if (percent != nullptr) {
        *percent = total >= 100 ? result / (total / 100) : 0;
    }
    return result;
}

std::uint32_t Engine::civ_level(const AiPlayer& p) const {
    const Player& pl = player(p);
    if (pl.unlocked(kInfra5A)) {
        return 4;
    }
    if (pl.unlocked(kInfra4B)) {
        return 3;
    }
    if (pl.unlocked(kInfra3C)) {
        return 2;
    }
    return pl.unlocked(kInfra2D) ? 1 : 0;
}

bool Engine::unlocked(const AiPlayer& p, const assets::BuildingType& b) const {
    return player(p).unlocked(b.infra);
}

bool Engine::affordable(const AiPlayer& p, const City& c, const assets::BuildingType& b) const {
    // 0x0041D120. The original compares the gold unsigned, so a debt passes (S-096); Open1602 does not.
    const auto& m = m_buildings.materials();
    const auto& cost = b.build_cost;
    return std::int64_t{player(p).money} >= cost.money && c.stock(m.wood) >= cost.wood &&
           c.stock(m.bricks) >= cost.bricks && c.stock(m.tools) >= cost.tools && c.stock(m.cannons) >= cost.cannons;
}

const assets::BuildingType* Engine::producer(std::uint8_t w, std::uint8_t raw) const {
    if (w >= m_producers.size()) {
        return nullptr;
    }
    for (const assets::BuildingType* b : m_producers[w]) {
        if (b->raw_ware == raw) {
            return b;
        }
    }
    return nullptr;
}

std::uint32_t Engine::fertility(std::uint16_t island, std::uint8_t w) const {
    return island < m_world.islands().size() ? island_fertility(m_world.islands()[island], w) : 0;
}

std::uint32_t Engine::full_crops(std::uint16_t island) const {
    std::uint32_t n = 0;
    for (const std::uint8_t w : {ware::kSpiceField, ware::kTobaccoField, ware::kWineField, ware::kCottonField,
                                 ware::kCocoaField, ware::kSugarField}) {
        n += fertility(island, w) == 128 ? 1U : 0U;
    }
    return n;
}

bool Engine::is_settler(PlayerId owner) const {
    const Player* pl = m_world.player(owner);
    return pl != nullptr && (pl->role == PlayerRole::Human || pl->role == PlayerRole::Computer);
}

std::uint32_t Engine::island_size(std::uint16_t island) const {
    const assets::IslandHeader& h = m_world.islands()[island].header;
    if (h.template_number != assets::IslandHeader::kNoTemplate) {
        return h.template_size;
    }
    const std::uint32_t area = std::uint32_t{h.width} * h.height;
    return area <= 900 ? 0U : area <= 1600 ? 1U : area <= 2600 ? 2U : area <= 4200 ? 3U : 4U;
}

void Engine::add_goods(City& c, std::uint8_t w, std::uint32_t amount) {
    c.ensure_ware(w);
    const std::uint32_t room = c.capacity > c.goods[w] ? c.capacity - c.goods[w] : 0;
    c.goods[w] += std::min(amount, room);
}

std::uint32_t Engine::remove_goods(City& c, std::uint8_t w, std::uint32_t amount) {
    c.ensure_ware(w);
    const std::uint32_t taken = std::min(amount, c.goods[w]);
    c.goods[w] -= taken;
    return taken;
}

std::uint32_t Engine::army_strength(PlayerId owner) const {
    return m_sim.military().army_size(owner);
}

std::uint32_t Engine::total_population(PlayerId owner) const {
    std::uint32_t n = 0;
    for (const City& c : m_world.cities()) {
        if (c.owner == owner) {
            n += population(c);
        }
    }
    return n;
}

bool Engine::behind(const AiPlayer& p) const {
    // 0x0040E9B0: behind the leading human in inhabitants (scaled by the difficulty) or in units.
    std::uint32_t best_pop = 0;
    std::uint32_t best_army = 0;
    for (std::size_t h = 0; h < m_world.players().size(); ++h) {
        if (m_world.players()[h].role != PlayerRole::Human) {
            continue;
        }
        best_pop = std::max(best_pop, total_population(static_cast<PlayerId>(h)));
        best_army = std::max(best_army, army_strength(static_cast<PlayerId>(h)));
    }
    std::uint32_t threshold = best_pop;
    switch (difficulty()) {
        case 0:
            threshold = best_pop - best_pop / 4;
            break;
        case 1:
            break;
        case 2:
            threshold = best_pop + best_pop / 4;
            break;
        default:
            threshold = best_pop + best_pop / 2;
            break;
    }
    return total_population(p.player) < threshold || army_strength(p.player) < best_army;
}

bool Engine::threatened(const AiPlayer& p) const {
    // 0x0040F1D0: the current city was attacked within 30 s, or a human not at peace has soldiers
    // on its island (the original looks only at the first such human, S-102).
    const City* c = current_city(p);
    if (c == nullptr) {
        return false;
    }
    if (c->last_attacked_ms != 0 && now_ms() - c->last_attacked_ms < 30'000) {
        return true;
    }
    for (const Soldier& s : m_world.soldiers()) {
        if (s.island != c->island || s.owner == p.player || s.dying()) {
            continue;
        }
        const Player* o = m_world.player(s.owner);
        if (o != nullptr && o->role == PlayerRole::Human && m_world.diplomacy().hostile(p.player, s.owner)) {
            return true;
        }
    }
    return false;
}

}  // namespace o1602::sim::ai
