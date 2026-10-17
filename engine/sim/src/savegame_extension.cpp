// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/savegame_extension.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>

#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/sim/state_io.hpp"

namespace o1602::sim {

using core::ErrorCode;
using core::make_error;

namespace {

constexpr std::array<std::uint8_t, 8> kMagic = {'O', '1', '6', '0', '2', 'S', 'A', 'V'};
constexpr std::uint16_t kFormatVersion = 1;

constexpr std::uint32_t tag(const char (&s)[5]) noexcept {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(s[0])) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(s[1])) << 8 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(s[2])) << 16 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(s[3])) << 24;
}

// Sections (each with version 1 so far).
constexpr std::uint32_t kRules = tag("RULE");
constexpr std::uint32_t kWorld = tag("WRLD");
constexpr std::uint32_t kIslands = tag("ISLE");
constexpr std::uint32_t kTiles = tag("TILE");
constexpr std::uint32_t kPlayers = tag("PLAY");
constexpr std::uint32_t kCities = tag("CITY");
constexpr std::uint32_t kProductions = tag("PROD");
constexpr std::uint32_t kHouses = tag("HOUS");
constexpr std::uint32_t kGrowth = tag("GROW");
constexpr std::uint32_t kCosts = tag("COST");
constexpr std::uint32_t kShips = tag("SHIP");
constexpr std::uint32_t kSoldiers = tag("SOLD");
constexpr std::uint32_t kAnimals = tag("ANIM");
constexpr std::uint32_t kDisasters = tag("DISA");
constexpr std::uint32_t kVolcanoes = tag("VOLC");
constexpr std::uint32_t kSimulation = tag("SIMU");

// Keys of the rules (RULE): a key per setting, so settings can be added and old files keep theirs.
enum RuleKey : std::uint16_t {
    kRefund = 1,
    kRefundPercent = 2,
    kMaxShips = 3,
    kGuaranteeResources = 4,
    kMaxGameSpeed = 5,
    kDroughts = 6,
    kVolcanoRule = 7,
};

using Out = core::ByteWriter;

/// Writes a section: tag, version, size, payload.
template <class Fn>
void section(Out& out, std::uint32_t id, Fn&& body) {
    out.u32(id);
    out.u16(1);
    out.u16(0);
    const std::size_t size_at = out.size();
    out.u32(0);
    const std::size_t start = out.size();
    body(out);
    out.patch_u32(size_at, static_cast<std::uint32_t>(out.size() - start));
}

// ---- Codecs ------------------------------------------------------------------------------------

void put_cell(Out& out, const assets::BuildingTable& buildings, const MapCell& c) {
    const assets::BuildingType* b = c.empty() ? nullptr : buildings.by_sprite(c.sprite);
    out.u32(b != nullptr ? b->id : 0);
    out.u16(b != nullptr ? static_cast<std::uint16_t>(c.sprite - b->gfx) : 0);
    out.u8(c.rotation);
    out.u8(c.anim_phase);
    out.u8(c.owner);
    out.u8(c.city);
    out.u8(c.random);
}

MapCell get_cell(StateIn& in, const assets::BuildingTable& buildings) {
    MapCell c;
    const std::uint32_t id = in.u32();
    const std::uint16_t sub = in.u16();
    c.rotation = in.u8();
    c.anim_phase = in.u8();
    c.owner = in.u8();
    c.city = in.u8();
    c.random = in.u8();
    if (id != 0) {
        const assets::BuildingType* b = buildings.by_id(id);
        if (b == nullptr) {
            in.fail();  // the game's data changed
            return c;
        }
        c.sprite = static_cast<std::uint16_t>(b->gfx + sub);
    }
    return c;
}

bool same_cell(const MapCell& a, const MapCell& b) noexcept {
    return a.sprite == b.sprite && a.rotation == b.rotation && a.anim_phase == b.anim_phase && a.owner == b.owner &&
           a.city == b.city && a.random == b.random;
}

void put_rules(Out& out, const Rules& r) {
    const std::array<std::pair<RuleKey, std::uint32_t>, 7> values = {{
        {kRefund, static_cast<std::uint32_t>(r.demolish_refund)},
        {kRefundPercent, r.refund_percent},
        {kMaxShips, r.max_ships_per_player},
        {kGuaranteeResources, r.guarantee_resources ? 1U : 0U},
        {kMaxGameSpeed, r.max_game_speed},
        {kDroughts, static_cast<std::uint32_t>(r.droughts)},
        {kVolcanoRule, static_cast<std::uint32_t>(r.volcanoes)},
    }};
    out.u32(static_cast<std::uint32_t>(values.size()));
    for (const auto& [key, value] : values) {
        out.u16(key);
        out.u32(value);
    }
}

Rules get_rules(StateIn& in) {
    Rules r;
    const std::uint32_t n = in.count(6);
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint16_t key = in.u16();
        const std::uint32_t v = in.u32();
        switch (key) {
            case kRefund:
                r.demolish_refund = static_cast<Refund>(std::min<std::uint32_t>(v, 2));
                break;
            case kRefundPercent:
                r.refund_percent = static_cast<std::uint8_t>(std::min<std::uint32_t>(v, 100));
                break;
            case kMaxShips:
                r.max_ships_per_player = v;
                break;
            case kGuaranteeResources:
                r.guarantee_resources = v != 0;
                break;
            case kMaxGameSpeed:
                r.max_game_speed = v;
                break;
            case kDroughts:
                r.droughts = static_cast<Droughts>(std::min<std::uint32_t>(v, 4));
                break;
            case kVolcanoRule:
                r.volcanoes = static_cast<Volcanoes>(std::min<std::uint32_t>(v, 2));
                break;
            default:
                break;  // a setting of a later version
        }
    }
    return r;
}

void put_random(Out& out, const Random& r) {
    out.u64(r.state());
    out.u64(r.increment());
}

/// Everything decoded, applied only when all of it was read without error.
struct Decoded {
    std::optional<Rules> rules;
    struct WorldPart {
        std::uint64_t time_ms = 0;
        std::uint32_t city_name_index = 0;
        World::GameSettings settings;
        std::uint64_t next_drought_ms = 0;
        std::optional<std::uint8_t> drought_seed;
        std::vector<Random> randoms;
        HouseClocks house_clocks;
        std::array<std::uint32_t, GrowthState::kClasses> growth_ms{};
        std::array<std::uint8_t, GrowthState::kClasses> growth_counter{};
        std::uint32_t next_disaster_id = 0;
    };
    std::optional<WorldPart> world;
    struct IslandPart {
        std::uint8_t number = 0;
        std::uint8_t deer_phase = 0;
        std::array<DeerSpot, 8> deer_spots{};
        WorldIsland::Drought drought{};
        std::vector<Crater> craters;
        bool volcano_erupted = false;
        std::vector<assets::IslandHeader::Deposit> deposits;
        std::uint32_t fertility = 0;
    };
    std::optional<std::vector<IslandPart>> islands;
    struct TileOverride {
        std::uint8_t island = 0;  ///< island number
        std::uint16_t x = 0;
        std::uint16_t y = 0;
        bool ground = false;
        MapCell cell;
    };
    std::optional<std::vector<TileOverride>> tiles;
    std::optional<std::vector<Player>> players;
    std::optional<std::vector<City>> cities;
    std::optional<std::pair<std::vector<Production>, std::vector<std::uint32_t>>> productions;
    std::optional<std::vector<House>> houses;
    std::optional<std::vector<GrowthEntry>> growth;
    std::optional<std::vector<CostBuilding>> costs;
    std::optional<std::vector<Ship>> ships;
    std::optional<std::vector<Soldier>> soldiers;
    std::optional<std::vector<Animal>> animals;
    std::optional<std::vector<Disaster>> disasters;
    std::optional<VolcanoState> volcanoes;
    std::optional<std::vector<std::uint8_t>> simulation;
};

// ---- Sections: write -----------------------------------------------------------------------------

void write_world(Out& out, const World& w) {
    out.u64(w.time_ms());
    out.u32(w.city_name_index());
    out.u8(w.game_settings().difficulty);
    out.u32(w.game_settings().flags);
    out.u64(w.next_drought_ms());
    out.boolean(w.drought_seed().has_value());
    out.u8(w.drought_seed().value_or(0));
    out.u32(static_cast<std::uint32_t>(RandomStream::Count));
    for (std::size_t s = 0; s < static_cast<std::size_t>(RandomStream::Count); ++s) {
        put_random(out, w.random(static_cast<RandomStream>(s)));
    }
    for (std::size_t g = 0; g < HouseClocks::kGroups; ++g) {
        out.u32(w.house_clocks().time_ms[g]);
        out.u8(w.house_clocks().epoch[g]);
    }
    for (std::size_t c = 0; c < GrowthState::kClasses; ++c) {
        out.u32(w.growth().time_ms[c]);
        out.u8(w.growth().counter[c]);
    }
    out.u32(w.peek_disaster_id());
}

void write_islands(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.islands().size()));
    for (const WorldIsland& i : w.islands()) {
        out.u8(i.header.number);
        out.u8(i.deer_phase);
        for (const DeerSpot& s : i.deer_spots) {
            out.boolean(s.used);
            out.u8(s.x);
            out.u8(s.y);
            out.u64(s.respawn_ms);
        }
        out.u8(i.drought.strength);
        out.u64(i.drought.end_ms);
        out.boolean(i.drought.curse_pending);
        out.u32(static_cast<std::uint32_t>(i.craters.size()));
        for (const Crater& c : i.craters) {
            out.u8(c.x);
            out.u8(c.y);
            out.u16(c.steps_left);
        }
        out.boolean(i.volcano_erupted);
        out.u32(static_cast<std::uint32_t>(i.header.deposits.size()));
        for (const auto& d : i.header.deposits) {
            out.u8(d.ware);
            out.u8(d.x);
            out.u8(d.y);
            out.u8(d.mask);
            out.u8(d.kind);
            out.u16(d.amount);
        }
        out.u32(i.header.fertility);
    }
}

/// Tiles that loading the original's chunks would not give back.
void write_tiles(Out& out, const World& w, const assets::BuildingTable& buildings,
                 std::span<const std::uint8_t> before) {
    std::map<std::uint8_t, IslandMap> reloaded;
    if (auto file = assets::ChunkFile::from_bytes(core::Bytes(before.begin(), before.end()))) {
        if (auto islands = assets::load_islands(*file)) {
            for (const assets::Island& island : *islands) {
                reloaded.emplace(island.header.number, IslandMap::from_island(island, buildings));
            }
        }
    }
    std::vector<Decoded::TileOverride> list;
    for (const WorldIsland& i : w.islands()) {
        const auto it = reloaded.find(i.header.number);
        const IslandMap* old =
            it != reloaded.end() && it->second.width() == i.map.width() && it->second.height() == i.map.height()
                ? &it->second
                : nullptr;
        for (std::uint16_t y = 0; y < i.map.height(); ++y) {
            for (std::uint16_t x = 0; x < i.map.width(); ++x) {
                if (old == nullptr || !same_cell(old->top(x, y), i.map.top(x, y))) {
                    list.push_back({i.header.number, x, y, false, i.map.top(x, y)});
                }
                if (old == nullptr || !same_cell(old->ground(x, y), i.map.ground(x, y))) {
                    list.push_back({i.header.number, x, y, true, i.map.ground(x, y)});
                }
            }
        }
    }
    out.u32(static_cast<std::uint32_t>(list.size()));
    for (const auto& t : list) {
        out.u8(t.island);
        out.u16(t.x);
        out.u16(t.y);
        out.boolean(t.ground);
        put_cell(out, buildings, t.cell);
    }
}

void write_players(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.players().size()));
    for (const Player& p : w.players()) {
        out.u8(static_cast<std::uint8_t>(p.role));
        out.u16(p.banner);
        out.i32(p.money);
        out.string(p.name);
        out.u32(p.infra_mask);
        out.u8(p.castles_built);
        out.u8(p.cathedrals_built);
        out.u16(p.arches_allowed);
        out.u16(p.arches_built);
        out.u16(p.monuments_allowed);
        out.u16(p.monuments_built);
    }
}

template <class T>
void put_u32s(Out& out, const std::vector<T>& v) {
    out.u32(static_cast<std::uint32_t>(v.size()));
    for (const T x : v) {
        out.u32(static_cast<std::uint32_t>(x));
    }
}
template <class T>
void put_u8s(Out& out, const std::vector<T>& v) {
    out.u32(static_cast<std::uint32_t>(v.size()));
    for (const T x : v) {
        out.u8(static_cast<std::uint8_t>(x));
    }
}

void write_cities(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.cities().size()));
    for (const City& c : w.cities()) {
        out.u16(c.island);
        out.u8(c.index);
        out.u8(c.owner);
        out.string(c.name);
        put_u32s(out, c.goods);
        out.boolean(c.warehouse_x.has_value() && c.warehouse_y.has_value());
        out.u16(c.warehouse_x.value_or(0));
        out.u16(c.warehouse_y.value_or(0));
        put_u32s(out, c.reserved);
        put_u32s(out, c.incoming);
        out.u32(c.capacity);
        put_u32s(out, c.inhabitants);
        put_u8s(out, c.satisfaction);
        put_u8s(out, c.tax_rate);
        out.u32(static_cast<std::uint32_t>(c.supply.size()));
        for (const City::Supply& s : c.supply) {
            out.u64(s.demand);
            out.u64(s.supplied);
            out.u8(s.ratio);
            for (const std::uint8_t h : s.history) {
                out.u8(h);
            }
        }
        out.u8(c.food_ratio);
        out.u16(c.satisfaction_bonus);
        out.u32(static_cast<std::uint32_t>(c.upgrade_queue.size()));
        for (const City::UpgradeSlot& u : c.upgrade_queue) {
            out.u32(u.inhabitants);
            out.u16(u.x);
            out.u16(u.y);
        }
        out.u8(c.epoch);
        out.boolean(c.material_lock);
        out.u8(c.worst_good);
        out.u64(c.next_event_ms);
    }
}

void write_productions(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.productions().size()));
    for (const Production& p : w.productions()) {
        out.u16(p.island);
        out.u16(p.x);
        out.u16(p.y);
        out.boolean(p.used);
        out.boolean(p.enabled);
        out.u8(p.rate);
        out.u8(p.anim_phase);
        out.u8(p.idle_cycles);
        out.u8(p.flags);
        out.u16(p.countdown);
        out.u16(p.raw);
        out.u16(p.raw2);
        out.u16(p.stock);
        out.u16(p.produced);
        out.u16(p.elapsed);
        out.u16(p.reserved);
    }
    put_u32s(out, w.free_productions());
}

void write_houses(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.houses().size()));
    for (const House& h : w.houses()) {
        out.u16(h.island);
        out.u16(h.x);
        out.u16(h.y);
        out.u8(h.city);
        out.u8(h.resident_class);
        out.u32(h.inhabitants64);
        out.u8(h.timer_group);
        out.u8(h.epoch);
        out.u8(h.queue_epoch);
        out.u8(h.market_distance);
        out.boolean(h.market);
        out.boolean(h.services_complete);
        out.u16(h.services);
    }
}

void write_growth(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.growth().entries.size()));
    for (const GrowthEntry& e : w.growth().entries) {
        out.u16(e.island);
        out.u16(e.x);
        out.u16(e.y);
        out.u8(e.growth_class);
        out.u8(e.seen);
        out.u8(e.stage);
    }
}

void write_costs(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.cost_buildings().size()));
    for (const CostBuilding& b : w.cost_buildings()) {
        out.u8(static_cast<std::uint8_t>(b.kind));
        out.u16(b.island);
        out.u16(b.x);
        out.u16(b.y);
        out.boolean(b.enabled);
    }
}

void write_ships(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.ships().size()));
    for (const Ship& s : w.ships()) {
        out.string(s.name);
        out.u16(s.x);
        out.u16(s.y);
        out.u16(s.figure);
        out.u8(s.owner);
        out.u8(s.direction);
        out.u32(static_cast<std::uint32_t>(s.cargo.size()));
        for (const Ship::Cargo& c : s.cargo) {
            out.u8(c.ware);
            out.u32(c.amount);
        }
    }
}

void write_soldiers(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.soldiers().size()));
    for (const Soldier& s : w.soldiers()) {
        put_real(out, s.x);
        put_real(out, s.y);
        out.u16(s.figure);
        out.u8(s.owner);
        out.u8(s.direction);
        out.u8(s.anim);
    }
}

void write_animals(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.animals().size()));
    for (const Animal& a : w.animals()) {
        out.u16(a.island);
        out.u16(a.x);
        out.u16(a.y);
        out.u16(a.figure);
    }
}

void write_disasters(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.disasters().size()));
    for (const Disaster& d : w.disasters()) {
        out.u8(static_cast<std::uint8_t>(d.kind));
        out.u16(d.island);
        out.u16(d.x);
        out.u16(d.y);
        out.u8(d.elapsed);
        out.u8(d.duration);
        out.u32(d.id);
    }
}

void put_crater(Out& out, const CraterRef& c) {
    out.u16(c.island);
    out.u8(c.x);
    out.u8(c.y);
}

void write_volcanoes(Out& out, const World& w) {
    const VolcanoState& v = w.volcanoes();
    out.u64(v.next_ms);
    out.boolean(v.last.has_value());
    put_crater(out, v.last.value_or(CraterRef{}));
    out.u32(static_cast<std::uint32_t>(v.eruptions.size()));
    for (const Eruption& e : v.eruptions) {
        put_crater(out, e.crater);
        out.u16(e.seed);
        out.u8(e.step);
        out.u64(e.next_us);
    }
    out.u32(static_cast<std::uint32_t>(v.lava.size()));
    for (const LavaLump& p : v.lava) {
        out.u16(p.island);
        for (const std::int64_t q : {p.x, p.y, p.z, p.dx, p.dy, p.dz, p.speed}) {
            out.i64(q);
        }
        out.u64(p.launch_us);
        out.u64(p.leg_start_us);
        out.u64(p.leg_end_us);
        out.u8(p.heading);
        out.u8(p.tilt);
        out.boolean(p.grounded);
        out.u8(p.anim);
    }
}

// ---- Sections: read ------------------------------------------------------------------------------

template <class T>
std::vector<T> get_u32s(StateIn& in) {
    std::vector<T> v(in.count(4));
    for (T& x : v) {
        x = static_cast<T>(in.u32());
    }
    return v;
}
template <class T>
std::vector<T> get_u8s(StateIn& in) {
    std::vector<T> v(in.count(1));
    for (T& x : v) {
        x = static_cast<T>(in.u8());
    }
    return v;
}

Decoded::WorldPart read_world(StateIn& in) {
    Decoded::WorldPart w;
    w.time_ms = in.u64();
    w.city_name_index = in.u32();
    w.settings.difficulty = in.u8();
    w.settings.flags = in.u32();
    w.next_drought_ms = in.u64();
    const bool seeded = in.boolean();
    const std::uint8_t seed = in.u8();
    if (seeded) {
        w.drought_seed = seed;
    }
    const std::uint32_t streams = in.count(16);
    for (std::uint32_t s = 0; s < streams; ++s) {
        const std::uint64_t state = in.u64();
        w.randoms.push_back(Random::from_state(state, in.u64()));
    }
    for (std::size_t g = 0; g < HouseClocks::kGroups; ++g) {
        w.house_clocks.time_ms[g] = in.u32();
        w.house_clocks.epoch[g] = in.u8();
    }
    for (std::size_t c = 0; c < GrowthState::kClasses; ++c) {
        w.growth_ms[c] = in.u32();
        w.growth_counter[c] = in.u8();
    }
    w.next_disaster_id = in.u32();
    return w;
}

std::vector<Decoded::IslandPart> read_islands(StateIn& in) {
    std::vector<Decoded::IslandPart> list(in.count(2));
    for (Decoded::IslandPart& i : list) {
        i.number = in.u8();
        i.deer_phase = in.u8();
        for (DeerSpot& s : i.deer_spots) {
            s.used = in.boolean();
            s.x = in.u8();
            s.y = in.u8();
            s.respawn_ms = in.u64();
        }
        i.drought.strength = in.u8();
        i.drought.end_ms = in.u64();
        i.drought.curse_pending = in.boolean();
        i.craters.resize(in.count(4));
        for (Crater& c : i.craters) {
            c.x = in.u8();
            c.y = in.u8();
            c.steps_left = in.u16();
        }
        i.volcano_erupted = in.boolean();
        i.deposits.resize(in.count(7));
        for (auto& d : i.deposits) {
            d.ware = in.u8();
            d.x = in.u8();
            d.y = in.u8();
            d.mask = in.u8();
            d.kind = in.u8();
            d.amount = in.u16();
        }
        i.fertility = in.u32();
    }
    return list;
}

std::vector<Decoded::TileOverride> read_tiles(StateIn& in, const assets::BuildingTable& buildings) {
    std::vector<Decoded::TileOverride> list(in.count(17));
    for (Decoded::TileOverride& t : list) {
        t.island = in.u8();
        t.x = in.u16();
        t.y = in.u16();
        t.ground = in.boolean();
        t.cell = get_cell(in, buildings);
    }
    return list;
}

std::vector<Player> read_players(StateIn& in) {
    std::vector<Player> list(in.count(8));
    for (Player& p : list) {
        p.role = static_cast<PlayerRole>(in.u8());
        p.banner = in.u16();
        p.money = in.i32();
        p.name = in.string();
        p.infra_mask = in.u32();
        p.castles_built = in.u8();
        p.cathedrals_built = in.u8();
        p.arches_allowed = in.u16();
        p.arches_built = in.u16();
        p.monuments_allowed = in.u16();
        p.monuments_built = in.u16();
    }
    return list;
}

std::vector<City> read_cities(StateIn& in) {
    std::vector<City> list(in.count(16));
    for (City& c : list) {
        c.island = in.u16();
        c.index = in.u8();
        c.owner = in.u8();
        c.name = in.string();
        c.goods = get_u32s<std::uint32_t>(in);
        const bool store = in.boolean();
        const std::uint16_t x = in.u16();
        const std::uint16_t y = in.u16();
        if (store) {
            c.warehouse_x = x;
            c.warehouse_y = y;
        }
        c.reserved = get_u32s<std::uint32_t>(in);
        c.incoming = get_u32s<std::uint32_t>(in);
        c.capacity = in.u32();
        c.inhabitants = get_u32s<std::uint32_t>(in);
        c.satisfaction = get_u8s<std::uint8_t>(in);
        c.tax_rate = get_u8s<std::uint8_t>(in);
        c.supply.resize(in.count(20));
        for (City::Supply& s : c.supply) {
            s.demand = in.u64();
            s.supplied = in.u64();
            s.ratio = in.u8();
            for (std::uint8_t& h : s.history) {
                h = in.u8();
            }
        }
        c.food_ratio = in.u8();
        c.satisfaction_bonus = in.u16();
        c.upgrade_queue.resize(in.count(8));
        for (City::UpgradeSlot& u : c.upgrade_queue) {
            u.inhabitants = in.u32();
            u.x = in.u16();
            u.y = in.u16();
        }
        c.epoch = in.u8();
        c.material_lock = in.boolean();
        c.worst_good = in.u8();
        c.next_event_ms = in.u64();
    }
    return list;
}

std::pair<std::vector<Production>, std::vector<std::uint32_t>> read_productions(StateIn& in) {
    std::vector<Production> list(in.count(26));
    for (Production& p : list) {
        p.island = in.u16();
        p.x = in.u16();
        p.y = in.u16();
        p.used = in.boolean();
        p.enabled = in.boolean();
        p.rate = in.u8();
        p.anim_phase = in.u8();
        p.idle_cycles = in.u8();
        p.flags = in.u8();
        p.countdown = in.u16();
        p.raw = in.u16();
        p.raw2 = in.u16();
        p.stock = in.u16();
        p.produced = in.u16();
        p.elapsed = in.u16();
        p.reserved = in.u16();
    }
    std::vector<std::uint32_t> free = get_u32s<std::uint32_t>(in);
    for (const std::uint32_t f : free) {
        if (f >= list.size() || list[f].used) {
            in.fail();
        }
    }
    return {std::move(list), std::move(free)};
}

std::vector<House> read_houses(StateIn& in) {
    std::vector<House> list(in.count(20));
    for (House& h : list) {
        h.island = in.u16();
        h.x = in.u16();
        h.y = in.u16();
        h.city = in.u8();
        h.resident_class = in.u8();
        h.inhabitants64 = in.u32();
        h.timer_group = in.u8();
        h.epoch = in.u8();
        h.queue_epoch = in.u8();
        h.market_distance = in.u8();
        h.market = in.boolean();
        h.services_complete = in.boolean();
        h.services = in.u16();
    }
    return list;
}

std::vector<GrowthEntry> read_growth(StateIn& in) {
    std::vector<GrowthEntry> list(in.count(9));
    for (GrowthEntry& e : list) {
        e.island = in.u16();
        e.x = in.u16();
        e.y = in.u16();
        e.growth_class = in.u8();
        e.seen = in.u8();
        e.stage = in.u8();
    }
    return list;
}

std::vector<CostBuilding> read_costs(StateIn& in) {
    std::vector<CostBuilding> list(in.count(8));
    for (CostBuilding& b : list) {
        b.kind = static_cast<CostBuilding::Kind>(in.u8());
        b.island = in.u16();
        b.x = in.u16();
        b.y = in.u16();
        b.enabled = in.boolean();
    }
    return list;
}

std::vector<Ship> read_ships(StateIn& in) {
    std::vector<Ship> list(in.count(14));
    for (Ship& s : list) {
        s.name = in.string();
        s.x = in.u16();
        s.y = in.u16();
        s.figure = in.u16();
        s.owner = in.u8();
        s.direction = in.u8();
        s.cargo.resize(in.count(5));
        for (Ship::Cargo& c : s.cargo) {
            c.ware = in.u8();
            c.amount = in.u32();
        }
    }
    return list;
}

std::vector<Soldier> read_soldiers(StateIn& in) {
    std::vector<Soldier> list(in.count(21));
    for (Soldier& s : list) {
        s.x = in.real();
        s.y = in.real();
        s.figure = in.u16();
        s.owner = in.u8();
        s.direction = in.u8();
        s.anim = in.u8();
    }
    return list;
}

std::vector<Animal> read_animals(StateIn& in) {
    std::vector<Animal> list(in.count(8));
    for (Animal& a : list) {
        a.island = in.u16();
        a.x = in.u16();
        a.y = in.u16();
        a.figure = in.u16();
    }
    return list;
}

std::vector<Disaster> read_disasters(StateIn& in) {
    std::vector<Disaster> list(in.count(13));
    for (Disaster& d : list) {
        d.kind = static_cast<Disaster::Kind>(in.u8());
        d.island = in.u16();
        d.x = in.u16();
        d.y = in.u16();
        d.elapsed = in.u8();
        d.duration = in.u8();
        d.id = in.u32();
    }
    return list;
}

CraterRef get_crater(StateIn& in) {
    CraterRef c;
    c.island = in.u16();
    c.x = in.u8();
    c.y = in.u8();
    return c;
}

VolcanoState read_volcanoes(StateIn& in) {
    VolcanoState v;
    v.next_ms = in.u64();
    const bool has_last = in.boolean();
    const CraterRef last = get_crater(in);
    if (has_last) {
        v.last = last;
    }
    v.eruptions.resize(in.count(15));
    for (Eruption& e : v.eruptions) {
        e.crater = get_crater(in);
        e.seed = in.u16();
        e.step = in.u8();
        e.next_us = in.u64();
    }
    v.lava.resize(in.count(86));
    for (LavaLump& p : v.lava) {
        p.island = in.u16();
        for (std::int64_t* q : {&p.x, &p.y, &p.z, &p.dx, &p.dy, &p.dz, &p.speed}) {
            *q = in.i64();
        }
        p.launch_us = in.u64();
        p.leg_start_us = in.u64();
        p.leg_end_us = in.u64();
        p.heading = in.u8();
        p.tilt = in.u8();
        p.grounded = in.boolean();
        p.anim = in.u8();
    }
    return v;
}

// ---- Checks against the world ----------------------------------------------------------------------

bool island_ok(const World& w, std::uint16_t island) {
    return island < w.islands().size();
}

core::Status check(const World& w, const Decoded& d) {
    const auto bad = [](const char* what) {
        return make_error(ErrorCode::InvalidFormat, std::string("O1602: ") + what);
    };
    if (d.islands && d.islands->size() != w.islands().size()) {
        return bad("islands do not match");
    }
    if (d.islands) {
        for (std::size_t i = 0; i < w.islands().size(); ++i) {
            if ((*d.islands)[i].number != w.islands()[i].header.number) {
                return bad("islands do not match");
            }
        }
    }
    if (d.tiles) {
        for (const auto& t : *d.tiles) {
            const auto i = w.island_index(t.island);
            if (!i || !w.islands()[*i].map.contains(t.x, t.y)) {
                return bad("tile outside its island");
            }
        }
    }
    if (d.cities) {
        for (const City& c : *d.cities) {
            if (!island_ok(w, c.island)) {
                return bad("city on an unknown island");
            }
        }
    }
    if (d.productions) {
        for (const Production& p : d.productions->first) {
            if (p.used && (!island_ok(w, p.island) || !w.islands()[p.island].map.contains(p.x, p.y))) {
                return bad("production outside its island");
            }
        }
    }
    const auto on_island = [&](const auto& list) {
        return std::all_of(list.begin(), list.end(), [&](const auto& e) {
            return island_ok(w, e.island) && w.islands()[e.island].map.contains(e.x, e.y);
        });
    };
    if ((d.houses && !on_island(*d.houses)) || (d.growth && !on_island(*d.growth)) ||
        (d.costs && !on_island(*d.costs)) || (d.animals && !on_island(*d.animals)) ||
        (d.disasters && !on_island(*d.disasters))) {
        return bad("a record outside its island");
    }
    if (d.volcanoes) {
        for (const Eruption& e : d.volcanoes->eruptions) {
            if (!island_ok(w, e.crater.island)) {
                return bad("eruption on an unknown island");
            }
        }
        for (const LavaLump& p : d.volcanoes->lava) {
            if (!island_ok(w, p.island)) {
                return bad("lava of an unknown island");
            }
        }
    }
    if (d.world && d.world->randoms.size() != static_cast<std::size_t>(RandomStream::Count)) {
        return bad("random generators do not match");
    }
    return core::Ok{};
}

}  // namespace

std::uint64_t savegame_fingerprint(std::span<const std::uint8_t> bytes) noexcept {
    std::uint64_t h = 0xCBF29CE484222325ULL;
    for (const std::uint8_t b : bytes) {
        h ^= b;
        h *= 0x100000001B3ULL;
    }
    return h;
}

core::Bytes encode_extension(const World& world, const assets::BuildingTable& buildings, const Simulation* simulation,
                             std::span<const std::uint8_t> before) {
    Out out;
    out.bytes(kMagic);
    out.u16(kFormatVersion);
    out.u16(0);
    out.u64(savegame_fingerprint(before));
    // The settings: those of the running game, or those the world was loaded with.
    if (simulation != nullptr || world.saved_rules()) {
        const Rules rules = simulation != nullptr ? simulation->rules() : *world.saved_rules();
        section(out, kRules, [&](Out& o) { put_rules(o, rules); });
    }
    section(out, kWorld, [&](Out& o) { write_world(o, world); });
    section(out, kIslands, [&](Out& o) { write_islands(o, world); });
    section(out, kTiles, [&](Out& o) { write_tiles(o, world, buildings, before); });
    section(out, kPlayers, [&](Out& o) { write_players(o, world); });
    section(out, kCities, [&](Out& o) { write_cities(o, world); });
    section(out, kProductions, [&](Out& o) { write_productions(o, world); });
    section(out, kHouses, [&](Out& o) { write_houses(o, world); });
    section(out, kGrowth, [&](Out& o) { write_growth(o, world); });
    section(out, kCosts, [&](Out& o) { write_costs(o, world); });
    section(out, kShips, [&](Out& o) { write_ships(o, world); });
    section(out, kSoldiers, [&](Out& o) { write_soldiers(o, world); });
    section(out, kAnimals, [&](Out& o) { write_animals(o, world); });
    section(out, kDisasters, [&](Out& o) { write_disasters(o, world); });
    section(out, kVolcanoes, [&](Out& o) { write_volcanoes(o, world); });
    if (simulation != nullptr) {
        section(out, kSimulation, [&](Out& o) { simulation->save_state(o); });
    } else if (!world.simulation_state().empty()) {
        // A world loaded but not yet run keeps the figures it was saved with.
        section(out, kSimulation, [&](Out& o) { o.bytes(world.simulation_state()); });
    }
    return out.take();
}

core::Status apply_extension(World& world, const assets::BuildingTable& buildings,
                             std::span<const std::uint8_t> payload, std::span<const std::uint8_t> before) {
    core::ByteReader reader(payload);
    const auto magic = reader.bytes(kMagic.size());
    if (!magic || !std::equal(magic->begin(), magic->end(), kMagic.begin())) {
        return make_error(ErrorCode::InvalidFormat, "O1602: not Open1602's chunk");
    }
    const auto version = reader.u16();
    const auto flags = reader.u16();
    const auto fingerprint = reader.u64();
    if (!version || !flags || !fingerprint) {
        return make_error(ErrorCode::InvalidFormat, "O1602: chunk too short");
    }
    if (*version > kFormatVersion) {
        return make_error(ErrorCode::Unsupported, "O1602: chunk of a newer Open1602");
    }
    if (*fingerprint != savegame_fingerprint(before)) {
        return make_error(ErrorCode::InvalidFormat, "O1602: the original's chunks were changed since");
    }
    Decoded d;
    while (!reader.at_end()) {
        const auto id = reader.u32();
        const auto section_version = reader.u16();
        const auto reserved = reader.u16();
        const auto size = reader.u32();
        if (!id || !section_version || !reserved || !size) {
            return make_error(ErrorCode::InvalidFormat, "O1602: damaged section header");
        }
        const auto body = reader.bytes(*size);
        if (!body) {
            return make_error(ErrorCode::InvalidFormat, "O1602: section beyond the chunk");
        }
        if (*section_version != 1) {
            continue;  // a later version of this part: skipped, the original's data stands
        }
        core::ByteReader sr(*body);
        StateIn in(sr);
        switch (*id) {
            case kRules:
                d.rules = get_rules(in);
                break;
            case kWorld:
                d.world = read_world(in);
                break;
            case kIslands:
                d.islands = read_islands(in);
                break;
            case kTiles:
                d.tiles = read_tiles(in, buildings);
                break;
            case kPlayers:
                d.players = read_players(in);
                break;
            case kCities:
                d.cities = read_cities(in);
                break;
            case kProductions:
                d.productions = read_productions(in);
                break;
            case kHouses:
                d.houses = read_houses(in);
                break;
            case kGrowth:
                d.growth = read_growth(in);
                break;
            case kCosts:
                d.costs = read_costs(in);
                break;
            case kShips:
                d.ships = read_ships(in);
                break;
            case kSoldiers:
                d.soldiers = read_soldiers(in);
                break;
            case kAnimals:
                d.animals = read_animals(in);
                break;
            case kDisasters:
                d.disasters = read_disasters(in);
                break;
            case kVolcanoes:
                d.volcanoes = read_volcanoes(in);
                break;
            case kSimulation:
                d.simulation = std::vector<std::uint8_t>(body->begin(), body->end());
                break;
            default:
                continue;  // a part of a later version
        }
        if (!in.ok()) {
            return make_error(ErrorCode::InvalidFormat, "O1602: damaged section");
        }
    }
    if (auto status = check(world, d); !status) {
        return status;
    }

    // Everything read: apply.
    if (d.rules) {
        world.set_saved_rules(d.rules);
    }
    if (d.world) {
        const auto& w = *d.world;
        world.set_time_ms(w.time_ms);
        world.set_city_name_index(w.city_name_index);
        world.set_game_settings(w.settings);
        world.set_next_drought_ms(w.next_drought_ms);
        if (w.drought_seed) {
            world.set_drought_seed(*w.drought_seed);
        }
        for (std::size_t s = 0; s < w.randoms.size(); ++s) {
            world.random(static_cast<RandomStream>(s)) = w.randoms[s];
        }
        world.house_clocks() = w.house_clocks;
        world.growth().time_ms = w.growth_ms;
        world.growth().counter = w.growth_counter;
        world.set_next_disaster_id(w.next_disaster_id);
    }
    if (d.islands) {
        for (std::size_t i = 0; i < d.islands->size(); ++i) {
            const auto& part = (*d.islands)[i];
            WorldIsland& island = world.island(i);
            island.deer_phase = part.deer_phase;
            island.deer_spots = part.deer_spots;
            island.drought = part.drought;
            island.craters = part.craters;
            island.volcano_erupted = part.volcano_erupted;
            island.header.deposits = part.deposits;
            island.header.fertility = part.fertility;
        }
    }
    if (d.tiles) {
        for (const auto& t : *d.tiles) {
            IslandMap& map = world.island(*world.island_index(t.island)).map;
            if (t.ground) {
                map.set_ground_cell(t.x, t.y, t.cell);
            } else {
                map.set_top_cell(t.x, t.y, t.cell);
            }
        }
    }
    if (d.players) {
        world.set_players(std::move(*d.players));
    }
    if (d.cities) {
        world.set_cities(std::move(*d.cities));
    }
    if (d.productions) {
        world.set_productions(std::move(d.productions->first), std::move(d.productions->second));
    }
    if (d.houses) {
        world.set_houses(std::move(*d.houses));
    }
    if (d.growth) {
        world.growth().entries = std::move(*d.growth);
    }
    if (d.costs) {
        world.cost_buildings() = std::move(*d.costs);
    }
    if (d.ships) {
        world.ships() = std::move(*d.ships);
    }
    if (d.soldiers) {
        world.set_soldiers(std::move(*d.soldiers));
    }
    if (d.animals) {
        world.set_animals(std::move(*d.animals));
    }
    if (d.disasters) {
        world.disasters() = std::move(*d.disasters);
    }
    if (d.volcanoes) {
        world.volcanoes() = std::move(*d.volcanoes);
    }
    if (d.simulation) {
        world.set_simulation_state(std::move(*d.simulation));
    }
    return core::Ok{};
}

}  // namespace o1602::sim
