// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/logistics.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_map>

#include "o1602/assets/savegame.hpp"
#include "o1602/sim/growth.hpp"

namespace o1602::sim {

namespace {

/// Time units of a tick: a step of `need` units at `speed` units per tick takes
/// need * kTimeUnits / speed of them.
constexpr std::uint64_t kTimeUnits = 65536;
/// A cart only sets out for at least a ton (0x0047D810 with min 32).
constexpr std::uint16_t kMinCartLoad = 32;
/// Carriers want at least half a load, at most 2 t (min(Maxtrag / 2, 64)).
constexpr std::uint16_t kMaxCarrierMinimum = 64;
/// Room of every further market or warehouse of a city (0x0047AB00: 0x140 = 10 t).
constexpr std::uint32_t kStorehouseRoom = 320;
/// Producers a market cart collects from: HAUS_PRODTYP kinds 1..6 (0x004706E0).
constexpr std::int32_t kLastProducerKind = 6;
/// Buildings a carrier fetches from: kinds 1..8, producers, markets and warehouses (0x004704D0).
constexpr std::int32_t kLastSourceKind = 8;
/// HAUPT (0x1E): handled like a warehouse; no building of the original uses it.
constexpr std::int32_t kHauptKind = 30;
/// Statistics value of a market is at most this (u16 in the original).
constexpr std::uint32_t kMaxU16 = std::numeric_limits<std::uint16_t>::max();

/// Deposits (0x0046BFF0): small veins serve every mine, big ones every mine down to 120 t.
constexpr std::uint8_t kSmallVein = 1;
constexpr std::uint32_t kBigVeinShared = 0xF00;
/// Below 40 t computer players stop exhausting small veins.
constexpr std::uint32_t kComputerKeeps = 0x500;
constexpr std::uint8_t kGold = 3;
using assets::kStockUnitsPerTon;

std::uint64_t tile_key(std::uint16_t island, TilePos t) noexcept {
    return (std::uint64_t{island} << 32) | (std::uint64_t{static_cast<std::uint16_t>(t.y)} << 16) |
           static_cast<std::uint16_t>(t.x);
}

bool is_storehouse(std::int32_t prod_kind) noexcept {
    return prod_kind == static_cast<std::int32_t>(ProductionKind::Markt) ||
           prod_kind == static_cast<std::int32_t>(ProductionKind::Kontor) || prod_kind == kHauptKind;
}

/// Finished goods (Schwerter 11 .. Ziegel 24) are always collected; raw material and intermediate
/// goods (Erz 2 .. Eisen 10) only from a full store (0x004717B0).
bool cart_collects_always(std::uint8_t ware) noexcept {
    return ware >= 11 && ware <= 24;
}

std::uint16_t clamp_u16(std::uint32_t v) noexcept {
    return static_cast<std::uint16_t>(std::min(v, kMaxU16));
}

std::uint16_t to_u16(std::int32_t v) noexcept {
    return static_cast<std::uint16_t>(v);
}

}  // namespace

std::optional<BuildingAt> building_at(const World& world, const assets::BuildingTable& buildings, std::uint16_t island,
                                      TilePos tile) {
    if (island >= world.islands().size()) {
        return std::nullopt;
    }
    const IslandMap& map = world.islands()[island].map;
    if (tile.x < 0 || tile.y < 0 || static_cast<std::uint32_t>(tile.x) >= map.width() ||
        static_cast<std::uint32_t>(tile.y) >= map.height()) {
        return std::nullopt;
    }
    const auto x = static_cast<std::uint32_t>(tile.x);
    const auto y = static_cast<std::uint32_t>(tile.y);
    const SubTile sub = map.sub_tile(buildings, x, y);
    if (sub.building == nullptr || sub.origin_x < 0 || sub.origin_y < 0 ||
        static_cast<std::uint32_t>(sub.origin_x) >= map.width() ||
        static_cast<std::uint32_t>(sub.origin_y) >= map.height()) {
        return std::nullopt;
    }
    BuildingAt b;
    b.type = sub.building;
    b.origin = {sub.origin_x, sub.origin_y};
    b.rotation = map.top(static_cast<std::uint32_t>(sub.origin_x), static_cast<std::uint32_t>(sub.origin_y)).rotation;
    rotated_size(*b.type, b.rotation, b.width, b.height);
    b.city = map.top(x, y).city;
    return b;
}

TilePos centre_tile(const BuildingAt& b) noexcept {
    return {b.origin.x + static_cast<std::int32_t>((b.width - 1) / 2),
            b.origin.y + static_cast<std::int32_t>((b.height - 1) / 2)};
}

TilePos door_tile(const BuildingAt& b) noexcept {
    if (!b.type->door_on_edge) {
        return centre_tile(b);
    }
    // Unrotated size: the door lies in the middle of the edge the rotation turns to the front.
    const auto w = static_cast<std::int32_t>(b.type->size_x);
    const auto h = static_cast<std::int32_t>(b.type->size_y);
    switch (b.rotation & 3U) {
        case 0:
            return {b.origin.x + w - 1, b.origin.y + (h - 1) / 2};
        case 1:
            return {b.origin.x + (h - 1) / 2, b.origin.y + w - 1};
        case 2:
            return {b.origin.x, b.origin.y + (h - 1) / 2};
        default:
            return {b.origin.x + (h - 1) / 2, b.origin.y};
    }
}

WalkerPosition walker_position(const Walker& w) noexcept {
    WalkerPosition p{w.tile.x + 0.5, w.tile.y + 0.5};
    if (w.moving() && w.step < w.path.size()) {
        const double t = static_cast<double>(w.progress) / static_cast<double>(w.length);
        p.x += (w.path[w.step].x - w.tile.x) * t;
        p.y += (w.path[w.step].y - w.tile.y) * t;
    }
    return p;
}

Logistics::Logistics(World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures)
    : m_world(&world), m_buildings(&buildings), m_figures(&figures) {
    m_jobs.assign(world.productions().size(), 0);
    update_storage();
}

std::uint32_t Logistics::step_speed(const assets::FigureType& figure, std::uint8_t walk_speed) noexcept {
    // 0x0044A690: Speed * 1e-4 * 32 / Wegspeed tiles per 20 ms, i.e. 0.008 * Speed / Wegspeed
    // tiles per 50 ms tick.
    const std::uint64_t w = std::max<std::uint8_t>(walk_speed, 1);
    const std::uint64_t units = std::uint64_t{figure.speed} * kTileUnits * 8U / (1000U * w);
    return static_cast<std::uint32_t>(std::max<std::uint64_t>(units, 1));
}

std::uint8_t& Logistics::jobs(std::uint32_t production) {
    if (production >= m_jobs.size()) {
        m_jobs.resize(std::size_t{production} + 1, 0);
    }
    return m_jobs[production];
}

std::optional<BuildingAt> Logistics::building_of(std::uint32_t production) const {
    if (production >= m_world->productions().size()) {
        return std::nullopt;
    }
    const Production& p = m_world->productions()[production];
    auto b = building_at(*m_world, *m_buildings, p.island, {p.x, p.y});
    if (b && (b->origin.x != p.x || b->origin.y != p.y)) {
        return std::nullopt;  // the entry no longer belongs to a building
    }
    return b;
}

void Logistics::update_storage() {
    struct Storage {
        std::uint32_t base = 0;
        std::uint32_t count = 0;
    };
    std::unordered_map<const City*, Storage> storage;
    for (std::uint32_t i = 0; i < m_world->productions().size(); ++i) {
        const auto b = building_of(i);
        if (!b || !is_storehouse(b->type->prod_kind)) {
            continue;
        }
        const City* city = m_world->city(m_world->productions()[i].island, b->city);
        if (city == nullptr) {
            continue;
        }
        Storage& s = storage[city];
        ++s.count;
        if (b->type->prod_kind != static_cast<std::int32_t>(ProductionKind::Markt)) {
            s.base = std::max(s.base, b->type->max_stock);
        }
    }
    for (const City& c : m_world->cities()) {
        City* city = m_world->city(c.island, c.index);
        const auto it = storage.find(city);
        city->capacity = it == storage.end() || it->second.count == 0
                             ? 0
                             : it->second.base + (it->second.count - 1) * kStorehouseRoom;
    }
}

void Logistics::handle(const ProductionEvent& event) {
    if (event.kind != ProductionEvent::Kind::NeedsRawGoods) {
        return;
    }
    const auto b = building_of(event.production);
    if (!b) {
        return;
    }
    const assets::BuildingType& type = *b->type;
    const bool second = event.ware != 0 && event.ware == type.raw2_ware && event.ware != type.raw_ware;
    if (type.prod_kind == static_cast<std::int32_t>(ProductionKind::Bergwerk) && !second) {
        mine_ore(event.production);
        return;
    }
    if (event.ware == 0) {
        return;
    }
    if (event.ware == kAllWares) {
        if (is_storehouse(type.prod_kind)) {
            send_cart(event.production);
        }
        return;
    }
    const bool workshop = type.prod_kind == static_cast<std::int32_t>(ProductionKind::Handwerk);
    if (second) {
        // The second raw material always comes by carrier (0x0047E22C).
        send_carrier(event.production, event.ware, workshop ? type.worker_figure : kCarrierFigure);
        return;
    }
    switch (static_cast<ProductionKind>(type.prod_kind)) {
        case ProductionKind::Handwerk:
            send_carrier(event.production, event.ware, type.worker_figure);
            break;
        case ProductionKind::Plantage:
            send_worker(event.production, event.ware, WalkerKind::Harvester);
            break;
        case ProductionKind::Weidetier:
            send_worker(event.production, event.ware, WalkerKind::Grazer);
            break;
        case ProductionKind::Fischerei:
            send_worker(event.production, event.ware, WalkerKind::Fisher);
            break;
        default:
            // Hunters are called by the deer (0x0047D0E0), not by the production cycle.
            break;
    }
}

std::uint16_t Logistics::reserve(const BuildingAt& at, std::uint16_t island, std::uint8_t ware, std::uint16_t min,
                                 std::uint16_t max, bool& from_warehouse, std::optional<std::uint32_t>& production) {
    auto amount_of = [&](std::uint32_t free) -> std::uint16_t {
        if (free > max) {
            return max;
        }
        return free < min ? std::uint16_t{0} : static_cast<std::uint16_t>(free);
    };
    if (is_storehouse(at.type->prod_kind)) {
        City* city = m_world->city(island, at.city);
        if (city == nullptr) {
            return 0;
        }
        const std::uint16_t amount = amount_of(city->available(ware));
        if (amount != 0) {
            city->ensure_ware(ware);
            city->reserved[ware] += amount;
            from_warehouse = true;
            production.reset();
        }
        return amount;
    }
    const auto index = m_world->production_index(island, to_u16(at.origin.x), to_u16(at.origin.y));
    if (!index) {
        return 0;
    }
    Production& q = m_world->productions()[*index];
    const std::uint16_t amount = amount_of(q.available());
    if (amount != 0) {
        q.reserved = static_cast<std::uint16_t>(q.reserved + amount);
        from_warehouse = false;
        production = index;
    }
    return amount;
}

void Logistics::mark_goals(const IslandMap& map, std::uint8_t city, std::int32_t last_kind, std::uint8_t speed_type) {
    const TilePos corner = m_finder.window_min();
    for (std::uint32_t j = 0; j < m_finder.window_height(); ++j) {
        for (std::uint32_t i = 0; i < m_finder.window_width(); ++i) {
            const TilePos t{corner.x + static_cast<std::int32_t>(i), corner.y + static_cast<std::int32_t>(j)};
            if (!m_finder.inside(t) || !map.contains(t.x, t.y)) {
                continue;
            }
            const MapCell& cell = map.top(static_cast<std::uint32_t>(t.x), static_cast<std::uint32_t>(t.y));
            if (cell.city != city || cell.empty()) {
                continue;
            }
            const assets::BuildingType* b = m_buildings->by_sprite(cell.sprite);
            if (b != nullptr && b->prod_kind >= 1 && b->prod_kind <= last_kind) {
                m_finder.set(t, PathFinder::Cell::Goal, b->walk_speed[std::min<std::uint8_t>(speed_type, 3)]);
            }
        }
    }
}

void Logistics::send_carrier(std::uint32_t home, std::uint8_t ware, std::uint16_t figure) {
    // One carrier per building at a time (0x00443080), also for the two raw materials.
    if (jobs(home) != 0) {
        return;
    }
    const assets::FigureType* fig = m_figures->get(figure);
    const auto hb = building_of(home);
    if (fig == nullptr || fig->speed == 0 || fig->max_carry == 0 || !hb) {
        return;
    }
    const Production& p = m_world->productions()[home];
    const IslandMap& map = m_world->islands()[p.island].map;
    m_finder.begin(map, *m_buildings, catchment_of(*hb->type, hb->rotation, hb->origin), WalkClass::Land,
                   fig->speed_type);
    // Goals: every producer, market and warehouse of the same city (never crossed).
    mark_goals(map, hb->city, kLastSourceKind, fig->speed_type);
    m_finder.open_footprint(hb->origin, hb->width, hb->height);

    Walker w;
    w.kind = WalkerKind::Carrier;
    w.island = p.island;
    w.figure = figure;
    w.city = hb->city;
    w.home = home;
    w.home_tile = door_tile(*hb);
    w.ware = ware;
    w.tile = w.home_tile;
    const auto max = clamp_u16(fig->max_carry);
    const auto min = static_cast<std::uint16_t>(std::min<std::uint32_t>(max / 2U, kMaxCarrierMinimum));
    const auto hit = m_finder.search(w.tile, [&](TilePos t) {
        const auto at = building_at(*m_world, *m_buildings, p.island, t);
        if (!at) {
            return PathFinder::Answer::Reject;
        }
        const bool has_ware =
            at->type->ware == ware || (at->type->ware == kAllWares && is_storehouse(at->type->prod_kind));
        if (!has_ware) {
            return PathFinder::Answer::Reject;
        }
        w.reserved = reserve(*at, p.island, ware, min, max, w.from_warehouse, w.source);
        if (w.reserved == 0) {
            return PathFinder::Answer::Reject;
        }
        w.source_origin = at->origin;
        return PathFinder::Answer::Accept;
    });
    if (!hit || !m_finder.path_to(*hit, w.path)) {
        if (hit) {
            Handle none;
            finish(none, w);
        }
        return;  // nothing to fetch; the building asks again at its next countdown
    }
    ++m_stats.carrier_trips;
    launch(std::move(w));
}

void Logistics::send_cart(std::uint32_t home) {
    const auto hb = building_of(home);
    if (!hb || jobs(home) >= hb->type->worker_count) {
        return;
    }
    const assets::FigureType* fig = m_figures->get(hb->type->worker_figure);
    const Production& p = m_world->productions()[home];
    City* city = m_world->city(p.island, hb->city);
    if (fig == nullptr || fig->speed == 0 || fig->max_carry == 0 || city == nullptr || city->capacity == 0) {
        return;
    }
    const IslandMap& map = m_world->islands()[p.island].map;
    m_finder.begin(map, *m_buildings, catchment_of(*hb->type, hb->rotation, hb->origin), WalkClass::Roads,
                   fig->speed_type);
    // Goals: the producers of the same city.
    mark_goals(map, hb->city, kLastProducerKind, fig->speed_type);
    m_finder.open_footprint(hb->origin, hb->width, hb->height);

    // Room a cart may still fill: free warehouse space minus what other carts are bringing.
    auto room = [&](std::uint8_t ware) -> std::uint32_t {
        const std::uint32_t free = city->free_space(ware);
        const std::uint32_t coming = city->incoming_of(ware);
        return free > coming ? free - coming : 0;
    };

    // The fullest producer wins (ties: the nearest). Open1602 ranks by the stock nobody has
    // reserved yet (S-023) and skips goods without room in the warehouse (S-024).
    struct Best {
        TilePos tile;
        TilePos origin;
        std::uint32_t production = 0;
        std::uint32_t fill = 0;
    };
    std::optional<Best> best;
    const TilePos start = centre_tile(*hb);
    m_finder.search(start, [&](TilePos t) {
        const auto at = building_at(*m_world, *m_buildings, p.island, t);
        if (!at) {
            return PathFinder::Answer::Reject;
        }
        const auto index = m_world->production_index(p.island, to_u16(at->origin.x), to_u16(at->origin.y));
        if (!index) {
            return PathFinder::Answer::Reject;
        }
        const Production& q = m_world->productions()[*index];
        const std::uint8_t ware = at->type->ware;
        if (q.no_pickup() || ware <= kAllWares || room(ware) < kMinCartLoad || q.available() < kMinCartLoad) {
            return PathFinder::Answer::Reject;
        }
        const bool full = q.stock >= at->type->max_stock;
        if (!cart_collects_always(ware) && !full) {
            return PathFinder::Answer::Reject;
        }
        const std::uint32_t fill =
            std::uint32_t{q.available()} * Production::kFullRate / std::max<std::uint32_t>(at->type->max_stock, 1);
        if (!best || fill > best->fill) {
            best = Best{t, at->origin, *index, fill};
        }
        return PathFinder::Answer::Remember;
    });
    if (!best) {
        return;
    }
    Production& q = m_world->productions()[best->production];
    const assets::BuildingType* source_type = building_of(best->production)->type;
    const std::uint8_t ware = source_type->ware;
    const auto amount =
        static_cast<std::uint16_t>(std::min<std::uint32_t>({fig->max_carry, q.available(), room(ware), kMaxU16}));

    Walker w;
    w.kind = WalkerKind::Cart;
    w.island = p.island;
    w.figure = hb->type->worker_figure;
    w.city = hb->city;
    w.home = home;
    w.home_tile = start;
    w.tile = start;
    w.ware = ware;
    w.source = best->production;
    w.source_origin = best->origin;
    if (!m_finder.path_to(best->tile, w.path)) {
        return;
    }
    q.reserved = static_cast<std::uint16_t>(q.reserved + amount);
    w.reserved = amount;
    city->ensure_ware(ware);
    city->incoming[ware] += amount;
    w.expected = amount;
    ++m_stats.cart_trips;
    launch(std::move(w));
}

bool Logistics::launch(Walker walker) {
    if (walker.path.empty()) {
        Handle none;
        finish(none, walker);
        return false;
    }
    ++jobs(walker.home);
    m_walkers.add(std::move(walker));
    return true;
}

void Logistics::begin_step(Walker& w) {
    const TilePos next = w.path[w.step];
    w.direction = direction_between(w.tile, next);
    const bool diagonal = next.x != w.tile.x && next.y != w.tile.y;
    w.length = diagonal ? kDiagonalUnits : kTileUnits;
    w.progress = 0;
    // The pace is set by the tile the step starts on (0x00458EC0 sets +0x0F at every run).
    const assets::FigureType* fig = m_figures->get(w.figure);
    std::uint8_t ws = 32;
    const IslandMap& map = m_world->islands()[w.island].map;
    if (w.tile.x >= 0 && w.tile.y >= 0 && static_cast<std::uint32_t>(w.tile.x) < map.width() &&
        static_cast<std::uint32_t>(w.tile.y) < map.height()) {
        const MapCell& cell = map.top(static_cast<std::uint32_t>(w.tile.x), static_cast<std::uint32_t>(w.tile.y));
        if (const assets::BuildingType* b = cell.empty() ? nullptr : m_buildings->by_sprite(cell.sprite)) {
            ws = b->walk_speed[fig != nullptr ? fig->speed_type : 0];
        }
    }
    if (harvests(w.kind)) {
        // Kept from the original (S-026): farm workers, cattle and boats never take the pace of
        // the ground; the search still weighs it.
        ws = 32;
    }
    w.walk_speed = ws;
    w.speed = fig != nullptr ? step_speed(*fig, ws) : 1;
}

void Logistics::tick() {
    m_walkers.for_each([&](Handle h, Walker& w) {
        std::uint64_t budget = kTimeUnits;
        if (w.working()) {
            const std::uint32_t spent = std::min(w.work_ms, kTickMs);
            w.work_ms -= spent;
            if (w.working()) {
                return;
            }
            finish_work(h, w);
            if (!m_walkers.contains(h)) {
                return;
            }
            budget = kTimeUnits * (kTickMs - spent) / kTickMs;
        }
        // A tick moves at most a few tiles; the bound only protects against a broken path.
        for (int guard = 0; budget > 0 && guard < 64; ++guard) {
            if (!w.moving()) {
                if (w.step >= w.path.size()) {
                    arrive(h, w);
                    if (!m_walkers.contains(h) || w.working() || w.step >= w.path.size()) {
                        return;
                    }
                }
                begin_step(w);
            }
            const std::uint64_t need = w.length - w.progress;
            const std::uint64_t time = (need * kTimeUnits + w.speed - 1) / w.speed;
            if (time > budget) {
                w.progress += static_cast<std::uint32_t>(budget * w.speed / kTimeUnits);
                budget = 0;
            } else {
                budget -= time;
                w.tile = w.path[w.step];
                ++w.step;
                w.progress = 0;
                w.length = 0;
            }
        }
    });
}

void Logistics::arrive(Handle h, Walker& w) {
    if (w.returning) {
        deliver(w);
        finish(h, w);
        return;
    }
    if (harvests(w.kind)) {
        if (w.field && w.tile == *w.field) {
            const assets::FigureType* fig = m_figures->get(w.figure);
            w.work_ms = std::max<std::uint32_t>(fig != nullptr ? fig->work_time_ms : 0, 1);
            w.animation = w.kind == WalkerKind::Harvester ? 2 : 1;
            return;
        }
    } else {
        pick_up(w);
    }
    if (!plan_home(w)) {
        finish(h, w);
    }
}

void Logistics::pick_up(Walker& w) {
    const assets::FigureType* fig = m_figures->get(w.figure);
    const std::uint32_t capacity = fig != nullptr ? fig->max_carry : 0;
    const std::uint32_t space = capacity > w.load ? capacity - w.load : 0;
    if (w.from_warehouse) {
        City* city = m_world->city(w.island, w.city);
        if (city != nullptr) {
            city->ensure_ware(w.ware);
            city->reserved[w.ware] -= std::min<std::uint32_t>(w.reserved, city->reserved[w.ware]);
            w.reserved = 0;
            // The reserved part plus what else is free (0x0047D640).
            const std::uint32_t take = std::min(space, city->available(w.ware));
            city->goods[w.ware] -= take;
            w.load = clamp_u16(w.load + take);
        }
        return;
    }
    if (!w.source) {
        return;
    }
    Production& q = m_world->productions()[*w.source];
    const auto at = building_of(*w.source);
    q.reserved = static_cast<std::uint16_t>(q.reserved - std::min(w.reserved, q.reserved));
    w.reserved = 0;
    if (!at || at->origin != w.source_origin || at->type->ware != w.ware) {
        return;  // the building is gone: come back empty (S-025: nothing is made up)
    }
    std::uint32_t take = std::min<std::uint32_t>(space, q.available());
    if (w.kind == WalkerKind::Cart) {
        City* city = m_world->city(w.island, w.city);
        if (q.no_pickup() || city == nullptr) {
            take = 0;  // the player forbade collecting meanwhile
        } else {
            city->ensure_ware(w.ware);
            const std::uint32_t others =
                city->incoming[w.ware] - std::min<std::uint32_t>(w.expected, city->incoming[w.ware]);
            const std::uint32_t free = city->free_space(w.ware);
            take = std::min(take, free > others ? free - others : 0);
            city->incoming[w.ware] = others + take;
            w.expected = static_cast<std::uint16_t>(take);
        }
    }
    q.stock = static_cast<std::uint16_t>(q.stock - take);
    w.load = clamp_u16(w.load + take);
}

bool Logistics::plan_home(Walker& w) {
    const auto hb = building_of(w.home);
    const assets::FigureType* fig = m_figures->get(w.figure);
    if (!hb || fig == nullptr) {
        return false;
    }
    w.returning = true;
    w.animation = 0;
    w.path.clear();
    w.step = 0;
    if (w.tile == w.home_tile) {
        return true;  // delivers at once
    }
    prepare_area(w, *hb, *fig, false);
    if (!harvests(w.kind)) {
        // Leave the building it fetched from.
        if (const auto here = building_at(*m_world, *m_buildings, w.island, w.tile)) {
            if (here->type->kind != building_kind::kRoad) {
                m_finder.open_footprint(here->origin, here->width, here->height);
            }
        }
    }
    m_finder.set(w.home_tile, PathFinder::Cell::Goal);
    const TilePos target = w.home_tile;
    const auto hit = m_finder.search(
        w.tile, [&](TilePos t) { return t == target ? PathFinder::Answer::Accept : PathFinder::Answer::Reject; });
    return hit && m_finder.path_to(*hit, w.path);
}

void Logistics::prepare_area(const Walker& w, const BuildingAt& hb, const assets::FigureType& fig, bool targets) {
    const IslandMap& map = m_world->islands()[w.island].map;
    const Catchment area = catchment_of(*hb.type, hb.rotation, hb.origin);
    const std::uint8_t type = std::min<std::uint8_t>(fig.speed_type, 3);
    WalkClass walk = WalkClass::Land;
    if (w.kind == WalkerKind::Cart) {
        walk = WalkClass::Roads;
    } else if (w.kind == WalkerKind::Fisher) {
        walk = WalkClass::Water;
    }
    m_finder.begin(map, *m_buildings, area, walk, fig.speed_type);
    if (harvests(w.kind)) {
        const std::uint8_t hut_rotation =
            map.contains(w.home_tile.x, w.home_tile.y)
                ? map.top(static_cast<std::uint32_t>(w.home_tile.x), static_cast<std::uint32_t>(w.home_tile.y)).rotation
                : 0;
        const TilePos corner = m_finder.window_min();
        for (std::uint32_t j = 0; j < m_finder.window_height(); ++j) {
            for (std::uint32_t i = 0; i < m_finder.window_width(); ++i) {
                const TilePos t{corner.x + static_cast<std::int32_t>(i), corner.y + static_cast<std::int32_t>(j)};
                if (!m_finder.inside(t) || !map.contains(t.x, t.y)) {
                    continue;
                }
                const MapCell& cell = map.top(static_cast<std::uint32_t>(t.x), static_cast<std::uint32_t>(t.y));
                const assets::BuildingType* b = cell.empty() ? nullptr : m_buildings->by_sprite(cell.sprite);
                if (b == nullptr) {
                    continue;
                }
                const bool target = targets && b->prod_kind == kProdKindRipe && b->ware == w.ware &&
                                    !m_reserved_tiles.contains(tile_key(w.island, t));
                if (w.kind == WalkerKind::Fisher) {
                    // 0x0046FB50: shoals of any owner; surf and river mouths facing the hut's
                    // side are closed.
                    if ((b->kind == building_kind::kSurf || b->kind == building_kind::kRiverMouth) &&
                        cell.rotation == ((hut_rotation + 1) & 3U)) {
                        m_finder.set(t, PathFinder::Cell::Blocked);
                    } else if (target && b->kind == building_kind::kSea) {
                        m_finder.set(t, PathFinder::Cell::Goal, 32);
                    }
                    continue;
                }
                // 0x0046F920: the worker's own crop is walkable in its city, ripe tiles are goals.
                if (cell.city == w.city && crop_ware(*m_buildings, *b) == w.ware) {
                    m_finder.set(t, target ? PathFinder::Cell::Goal : PathFinder::Cell::Walkable, b->walk_speed[type]);
                }
            }
        }
        if (w.kind == WalkerKind::Fisher) {
            // The tile in front of the hut leads to the water.
            static constexpr std::array<TilePos, 4> kFront = {{{0, 1}, {-1, 0}, {0, -1}, {1, 0}}};
            const TilePos front{w.home_tile.x + kFront[hut_rotation & 3U].x,
                                w.home_tile.y + kFront[hut_rotation & 3U].y};
            if (const auto at = building_at(*m_world, *m_buildings, w.island, front)) {
                m_finder.open_footprint(at->origin, at->width, at->height);
            }
        }
    }
    m_finder.open_footprint(hb.origin, hb.width, hb.height);
}

bool Logistics::find_crop(Walker& w, const BuildingAt& hb, const assets::FigureType& fig) {
    prepare_area(w, hb, fig, true);
    const auto hit = m_finder.search(w.tile, [](TilePos) { return PathFinder::Answer::Accept; });
    if (!hit || !m_finder.path_to(*hit, w.path)) {
        return false;
    }
    w.step = 0;
    w.field = *hit;
    w.returning = false;
    m_reserved_tiles.insert(tile_key(w.island, *hit));
    return true;
}

void Logistics::send_worker(std::uint32_t home, std::uint8_t ware, WalkerKind kind) {
    const auto hb = building_of(home);
    if (!hb) {
        return;
    }
    // One worker per farm or fishing hut (Figuranz ignored, 0x00443080); cattle farms keep up
    // to Figuranz animals out (0x0044AF10).
    const std::uint8_t limit = kind == WalkerKind::Grazer ? std::max<std::uint8_t>(hb->type->worker_count, 1) : 1;
    if (jobs(home) >= limit) {
        return;
    }
    const assets::FigureType* fig = m_figures->get(hb->type->worker_figure);
    if (fig == nullptr || fig->speed == 0) {
        return;
    }
    Walker w;
    w.kind = kind;
    w.island = m_world->productions()[home].island;
    w.figure = hb->type->worker_figure;
    w.city = hb->city;
    w.home = home;
    w.home_tile = centre_tile(*hb);
    w.tile = w.home_tile;
    w.ware = ware;
    w.animation = kind == WalkerKind::Harvester ? 1 : 0;
    if (!find_crop(w, *hb, *fig)) {
        return;  // nothing ripe: the building asks again
    }
    ++m_stats.worker_trips;
    launch(std::move(w));
}

void Logistics::finish_work(Handle h, Walker& w) {
    bool harvested = false;
    if (w.field) {
        m_reserved_tiles.erase(tile_key(w.island, *w.field));
        harvested = harvest_tile(*m_world, *m_buildings, w.island, to_u16(w.field->x), to_u16(w.field->y), w.ware);
        w.field.reset();
    }
    // The original books a ton even if the field was gone meanwhile; Open1602 only what was
    // harvested (S-025).
    if (harvested) {
        w.load = clamp_u16(std::uint32_t{w.load} + kStockUnitsPerTon);
        m_stats.harvested += kStockUnitsPerTon;
    }
    const auto hb = building_of(w.home);
    const assets::FigureType* fig = m_figures->get(w.figure);
    if (hb && fig != nullptr && w.kind != WalkerKind::Harvester && w.load < fig->max_carry &&
        m_world->productions()[w.home].enabled) {
        // Cattle and boats go on to the next ripe tile until they are full.
        w.animation = 0;
        if (find_crop(w, *hb, *fig)) {
            return;
        }
    }
    if (!plan_home(w)) {
        finish(h, w);
    }
}

void Logistics::mine_ore(std::uint32_t home) {
    const auto hb = building_of(home);
    if (!hb) {
        return;
    }
    Production& p = m_world->productions()[home];
    const assets::BuildingType& mine = *hb->type;
    // 0x0046C010: the mountain lies one tile from the mine's origin, on the side its rotation
    // faces.
    static constexpr std::array<TilePos, 4> kSide = {{{-1, 0}, {0, -1}, {1, 0}, {0, 1}}};
    const TilePos side{hb->origin.x + kSide[hb->rotation & 3U].x, hb->origin.y + kSide[hb->rotation & 3U].y};
    const auto mountain = building_at(*m_world, *m_buildings, p.island, side);
    if (!mountain) {
        return;
    }
    auto& deposits = m_world->island(p.island).header.deposits;
    auto it = std::find_if(deposits.begin(), deposits.end(), [&](const assets::IslandHeader::Deposit& d) {
        return d.x == mountain->origin.x && d.y == mountain->origin.y && d.ware == mine.ware;
    });
    const std::uint32_t amount = mine.raw_amount;
    if (it == deposits.end() || amount == 0 || it->amount < amount) {
        return;  // exhausted: the mine idles and reports it after Maxnorohst cycles
    }
    const bool any_mine = it->kind == kSmallVein || it->amount >= kBigVeinShared;
    if (!any_mine && mine.ore_mountain != 0) {
        return;  // only the deep mine digs on in a big vein below 120 t
    }
    p.raw = clamp_u16(std::uint32_t{p.raw} + amount);
    m_stats.ore_mined += amount;
    if (any_mine) {
        if (mine.ware == kGold) {
            return;  // gold never runs out
        }
        // Computer players do not exhaust small veins (below 40 t nothing is taken).
        const City* city = m_world->city(p.island, hb->city);
        const Player* owner = city != nullptr ? m_world->player(city->owner) : nullptr;
        if (it->amount < kComputerKeeps && owner != nullptr && owner->role != PlayerRole::Human) {
            return;
        }
    }
    it->amount = static_cast<std::uint16_t>(it->amount - amount);
    if (it->amount < kStockUnitsPerTon) {
        deposits.erase(it);  // 0x0046BDD0
    }
}

void Logistics::deliver(Walker& w) {
    Production& home = m_world->productions()[w.home];
    const auto hb = building_of(w.home);
    if (!hb) {
        return;
    }
    if (w.kind == WalkerKind::Cart) {
        City* city = m_world->city(w.island, w.city);
        if (city == nullptr) {
            return;
        }
        city->ensure_ware(w.ware);
        city->incoming[w.ware] -= std::min<std::uint32_t>(w.expected, city->incoming[w.ware]);
        w.expected = 0;
        const std::uint32_t room = city->capacity > city->goods[w.ware] ? city->capacity - city->goods[w.ware] : 0;
        const std::uint32_t added = std::min<std::uint32_t>(w.load, room);
        city->goods[w.ware] += added;
        if (hb->type->prod_kind == static_cast<std::int32_t>(ProductionKind::Markt)) {
            // The market counts deliveries for its stalls (0x0047D940 -> +0x10).
            home.produced = clamp_u16(home.produced + added);
        }
        m_stats.goods_collected += added;
    } else {
        const bool second = w.ware == hb->type->raw2_ware && w.ware != hb->type->raw_ware;
        std::uint16_t& slot = second ? home.raw2 : home.raw;
        slot = clamp_u16(std::uint32_t{slot} + w.load);
        if (home.rate == 0) {
            home.countdown = 0;  // an idle building starts in the next second (0x0047D940)
        }
        m_stats.raw_delivered += w.load;
    }
    w.load = 0;
}

void Logistics::finish(Handle h, Walker& w) {
    if (w.field) {
        m_reserved_tiles.erase(tile_key(w.island, *w.field));
        w.field.reset();
    }
    // Fix S-022: whatever the figure still holds reserved goes back, however its trip ends.
    if (w.reserved != 0) {
        if (w.from_warehouse) {
            if (City* city = m_world->city(w.island, w.city); city != nullptr && w.ware < city->reserved.size()) {
                city->reserved[w.ware] -= std::min<std::uint32_t>(w.reserved, city->reserved[w.ware]);
            }
        } else if (w.source && *w.source < m_world->productions().size()) {
            Production& q = m_world->productions()[*w.source];
            q.reserved = static_cast<std::uint16_t>(q.reserved - std::min(w.reserved, q.reserved));
        }
        w.reserved = 0;
    }
    if (w.expected != 0) {
        if (City* city = m_world->city(w.island, w.city); city != nullptr && w.ware < city->incoming.size()) {
            city->incoming[w.ware] -= std::min<std::uint32_t>(w.expected, city->incoming[w.ware]);
        }
        w.expected = 0;
    }
    if (m_walkers.contains(h)) {
        std::uint8_t& n = jobs(w.home);
        n = n > 0 ? static_cast<std::uint8_t>(n - 1) : 0;
        m_walkers.remove(h);
    }
}

}  // namespace o1602::sim
