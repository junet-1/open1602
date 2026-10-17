// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/savegame_writer.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <span>
#include <unordered_map>
#include <unordered_set>

#include "o1602/assets/chunk_writer.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/sim/growth.hpp"
#include "o1602/sim/houses.hpp"
#include "o1602/sim/savegame_extension.hpp"

namespace o1602::sim {

using core::ErrorCode;
using core::make_error;

namespace {

// Limits of the original (docs/re/savegame-writer.md, section 7).
constexpr std::size_t kMaxIslands = 50;
constexpr std::size_t kMaxCities = 75;
constexpr std::size_t kMaxShips = 150;
constexpr std::size_t kCityNameMax = 23;
constexpr std::size_t kShipNameMax = 23;
constexpr std::uint8_t kNoOwner = 7;
constexpr std::uint32_t kIdOffset = 20000;

// Record sizes.
constexpr std::size_t kIslandHeaderSize = 0x74;
constexpr std::size_t kTileSize = 8;
constexpr std::size_t kDeerSize = 8;
constexpr std::size_t kProductionSize = 0x18;
constexpr std::size_t kShipyardSize = 0x18;
constexpr std::size_t kCastleSize = 0x70;
constexpr std::size_t kHouseSize = 0x10;
constexpr std::size_t kGrowthSize = 8;
constexpr std::size_t kCitySize = 0xA8;
constexpr std::size_t kWarehouseSize = 0x3EC;
constexpr std::size_t kConsumptionSize = 0x104;
constexpr std::size_t kPlayerSize = 0x430;
constexpr std::size_t kShipSize = 0x1B4;
constexpr std::size_t kSoldierSize = 0x44;
constexpr std::size_t kTimersSize = 0x298;

void put16(core::Bytes& d, std::size_t o, std::uint32_t v) {
    d[o] = static_cast<std::uint8_t>(v);
    d[o + 1] = static_cast<std::uint8_t>(v >> 8);
}
void put32(core::Bytes& d, std::size_t o, std::uint32_t v) {
    for (std::size_t k = 0; k < 4; ++k) {
        d[o + k] = static_cast<std::uint8_t>(v >> (8 * k));
    }
}
std::uint32_t get16(std::span<const std::uint8_t> d, std::size_t o) {
    return static_cast<std::uint32_t>(d[o] | (d[o + 1] << 8));
}
std::uint32_t get32(std::span<const std::uint8_t> d, std::size_t o) {
    return static_cast<std::uint32_t>(d[o]) | (static_cast<std::uint32_t>(d[o + 1]) << 8) |
           (static_cast<std::uint32_t>(d[o + 2]) << 16) | (static_cast<std::uint32_t>(d[o + 3]) << 24);
}
std::uint32_t clamp32(std::uint64_t v) {
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(v, 0xFFFFFFFFU));
}

constexpr std::uint64_t key3(std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept {
    return (std::uint64_t{a} << 32) | (std::uint64_t{b} << 16) | c;
}

/// Records of the source file: the starting point of every record the writer patches.
class Source {
public:
    explicit Source(const assets::ChunkFile* file) : m_file(file) {}

    /// All records of a chunk (all chunks of that name), in file order.
    [[nodiscard]] std::vector<std::span<const std::uint8_t>> records(std::string_view name, std::size_t size) const {
        std::vector<std::span<const std::uint8_t>> out;
        if (m_file == nullptr) {
            return out;
        }
        for (const assets::Chunk& c : m_file->chunks()) {
            if (c.name != name) {
                continue;
            }
            const auto data = m_file->payload(c);
            for (std::size_t o = 0; o + size <= data.size(); o += size) {
                out.push_back(data.subspan(o, size));
            }
        }
        return out;
    }
    /// Records by a key.
    template <class Key>
    [[nodiscard]] std::unordered_map<std::uint64_t, std::span<const std::uint8_t>> by_key(std::string_view name,
                                                                                          std::size_t size,
                                                                                          Key key) const {
        std::unordered_map<std::uint64_t, std::span<const std::uint8_t>> out;
        for (const auto r : records(name, size)) {
            out.emplace(key(r), r);
        }
        return out;
    }
    /// The first chunk of that name, whole.
    [[nodiscard]] std::optional<std::span<const std::uint8_t>> chunk(std::string_view name) const {
        if (m_file == nullptr) {
            return std::nullopt;
        }
        const assets::Chunk* c = m_file->find(name);
        if (c == nullptr) {
            return std::nullopt;
        }
        return m_file->payload(*c);
    }
    [[nodiscard]] const assets::ChunkFile* file() const noexcept { return m_file; }

private:
    const assets::ChunkFile* m_file;
};

/// A record started from the source one, or zeros.
core::Bytes start(const std::unordered_map<std::uint64_t, std::span<const std::uint8_t>>& raw, std::uint64_t key,
                  std::size_t size) {
    core::Bytes r(size, 0);
    if (const auto it = raw.find(key); it != raw.end()) {
        std::copy(it->second.begin(), it->second.end(), r.begin());
    }
    return r;
}

class Writer {
public:
    Writer(const World& world, const assets::BuildingTable& buildings)
        : m_world(world), m_buildings(buildings), m_source(world.source().get()) {}

    core::Result<std::vector<SavedChunk>> run() {
        if (auto status = check(); !status) {
            return status.error();
        }
        islands();
        passthrough("CUSTOM");
        passthrough("NAME");
        production();
        cost_buildings(CostBuilding::Kind::Shipyard, "WERFT", kShipyardSize);
        cost_buildings(CostBuilding::Kind::Castle, "MILITAR", kCastleSize);
        houses();
        growth();
        cities();
        players();
        passthrough("AUFTRAG4");
        passthrough("HANDLER");
        ships();
        soldiers();
        passthrough("TURM");
        timers();
        return std::move(m_out);
    }

private:
    const World& m_world;
    const assets::BuildingTable& m_buildings;
    Source m_source;
    std::vector<SavedChunk> m_out;

    void add(std::string name, core::Bytes data) { m_out.push_back(SavedChunk{std::move(name), std::move(data)}); }

    core::Status check() const {
        if (m_world.islands().size() > kMaxIslands) {
            return make_error(ErrorCode::OutOfRange, "more than 50 islands");
        }
        for (const WorldIsland& i : m_world.islands()) {
            if (i.header.number >= kMaxIslands) {
                return make_error(ErrorCode::OutOfRange, "island number " + std::to_string(i.header.number));
            }
        }
        if (m_world.width() != World::kOriginalWidth || m_world.height() != World::kOriginalHeight) {
            return make_error(ErrorCode::OutOfRange, "the world is not 500 x 350");
        }
        if (m_world.cities().size() > kMaxCities) {
            return make_error(ErrorCode::OutOfRange, "more than 75 cities");
        }
        if (m_world.players().size() > assets::kOriginalPlayerSlots) {
            return make_error(ErrorCode::OutOfRange, "more than 7 players");
        }
        if (m_world.ships().size() > kMaxShips) {
            return make_error(ErrorCode::OutOfRange, "more than 150 ships");
        }
        return core::Ok{};
    }

    [[nodiscard]] std::uint8_t number(std::uint16_t island) const { return m_world.islands()[island].header.number; }
    [[nodiscard]] std::uint8_t owner_byte(PlayerId p) const {
        return p < assets::kOriginalPlayerSlots ? static_cast<std::uint8_t>(p) : kNoOwner;
    }

    void passthrough(std::string_view name) {
        if (const auto c = m_source.chunk(name)) {
            add(std::string(name), core::Bytes(c->begin(), c->end()));
        }
    }

    // ---- Islands ---------------------------------------------------------------------------------

    /// Islands in the order of their numbers, as the original writes them.
    [[nodiscard]] std::vector<std::uint16_t> island_order() const {
        std::vector<std::uint16_t> order(m_world.islands().size());
        for (std::uint16_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        std::stable_sort(order.begin(), order.end(),
                         [&](std::uint16_t a, std::uint16_t b) { return number(a) < number(b); });
        return order;
    }

    void islands() {
        const std::vector<std::uint16_t> order = island_order();
        // Source tiles of each island and layer: the random bits the original keeps per tile.
        std::map<std::pair<std::uint8_t, std::size_t>, std::unordered_map<std::uint32_t, assets::IslandTile>> tiles;
        if (m_source.file() != nullptr) {
            if (auto loaded = assets::load_islands(*m_source.file())) {
                for (const assets::Island& island : *loaded) {
                    for (std::size_t l = 0; l < island.layers.size(); ++l) {
                        auto& m = tiles[{island.header.number, l}];
                        for (const assets::IslandTile& t : island.layers[l]) {
                            m.emplace(std::uint32_t{t.y} << 8 | t.x, t);
                        }
                    }
                }
            }
        }
        for (const std::uint16_t i : order) {
            const WorldIsland& island = m_world.islands()[i];
            add("INSEL5", island_header(i));
            add("INSELHAUS", island_layer(i, false, tiles[{island.header.number, 0}]));
            add("INSELHAUS", island_layer(i, true, tiles[{island.header.number, 1}]));
            add("HIRSCH2", deer(i));
        }
    }

    core::Bytes island_header(std::uint16_t i) const {
        const WorldIsland& island = m_world.islands()[i];
        const assets::IslandHeader& h = island.header;
        core::Bytes r(kIslandHeaderSize, 0);
        std::copy_n(h.raw.begin(), std::min(h.raw.size(), r.size()), r.begin());
        r[0] = h.number;
        r[1] = static_cast<std::uint8_t>(island.map.width());
        r[2] = static_cast<std::uint8_t>(island.map.height());
        r[3] = static_cast<std::uint8_t>((r[3] & ~5U) | (island.drought.curse_pending ? 1U : 0U) |
                                         (island.volcano_erupted ? 4U : 0U));
        put16(r, 4, island.x);
        put16(r, 6, island.y);
        r[8] = static_cast<std::uint8_t>((r[8] & ~3U) | (island.deer_phase & 3U));
        // Owners of the eight city slots.
        for (std::uint8_t c = 0; c < 8; ++c) {
            const City* city = m_world.city(i, c);
            r[0x0C + c] = city != nullptr ? owner_byte(city->owner) : kNoOwner;
        }
        // Volcano craters (8 bytes each from 0x3C).
        r[0x17] = static_cast<std::uint8_t>(std::min<std::size_t>(island.craters.size(), 2));
        for (std::size_t k = 0; k < 2; ++k) {
            const std::size_t o = 0x3C + 8 * k;
            if (k < island.craters.size()) {
                const Crater& c = island.craters[k];
                r[o] = 0;
                r[o + 1] = c.x;
                r[o + 2] = c.y;
                r[o + 3] = r[o + 4] = r[o + 5] = 0;
                put16(r, o + 6, c.steps_left);
            } else {
                std::fill_n(r.begin() + static_cast<std::ptrdiff_t>(o), 8, std::uint8_t{0});
            }
        }
        // Deposits (8 bytes each from 0x1C).
        r[0x1A] = static_cast<std::uint8_t>(std::min<std::size_t>(h.deposits.size(), 2));
        for (std::size_t k = 0; k < 2; ++k) {
            const std::size_t o = 0x1C + 8 * k;
            if (k < h.deposits.size()) {
                const auto& d = h.deposits[k];
                r[o] = d.ware;
                r[o + 1] = d.x;
                r[o + 2] = d.y;
                r[o + 3] = d.mask;
                r[o + 4] = d.kind;
                put16(r, o + 6, d.amount);
            } else {
                std::fill_n(r.begin() + static_cast<std::ptrdiff_t>(o), 8, std::uint8_t{0});
            }
        }
        // The loader ORs 0x1181 into the fertility: keep the stored bits if they give the same.
        constexpr std::uint32_t kAlwaysFertile = 0x1181;
        if ((get32(r, 0x5C) | kAlwaysFertile) != (h.fertility | kAlwaysFertile)) {
            put32(r, 0x5C, h.fertility);
        }
        r[0x66] = island.drought.strength;
        put32(r, 0x6C, clamp32(island.drought.end_ms / 100));
        return r;
    }

    /// Layer 1 (`ground`): every building of the ground layer, of no city and no owner. Layer 2:
    /// the buildings on top that differ from the ground (0x00468BF0), with their city and owner.
    core::Bytes island_layer(std::uint16_t i, bool top_layer,
                             const std::unordered_map<std::uint32_t, assets::IslandTile>& source) const {
        const WorldIsland& island = m_world.islands()[i];
        const IslandMap& map = island.map;
        core::Bytes out;
        for (std::uint32_t y = 0; y < map.height(); ++y) {
            for (std::uint32_t x = 0; x < map.width(); ++x) {
                const MapCell& cell = top_layer ? map.top(x, y) : map.ground(x, y);
                if (cell.empty()) {
                    continue;
                }
                const SubTile sub =
                    top_layer ? map.sub_tile(m_buildings, x, y) : map.ground_sub_tile(m_buildings, x, y);
                if (sub.building == nullptr || sub.origin_x != static_cast<std::int32_t>(x) ||
                    sub.origin_y != static_cast<std::int32_t>(y)) {
                    continue;
                }
                if (top_layer && same_as_ground(map, x, y)) {
                    continue;
                }
                const std::uint8_t city = top_layer && cell.city != MapCell::kNoCity ? cell.city : 7;
                const std::uint8_t owner =
                    top_layer && cell.city != MapCell::kNoCity ? owner_byte(cell.owner) : kNoOwner;
                std::uint32_t phase = cell.anim_phase;
                if (sub.building->anim_count > 0) {
                    phase %= sub.building->anim_count;
                }
                std::uint32_t bits = (cell.rotation & 3U) | (phase & 0xFU) << 2 |
                                     std::uint32_t{island.header.number} << 6 | std::uint32_t{city & 7U} << 14 |
                                     std::uint32_t{owner & 0xFU} << 22;
                const auto id = static_cast<std::uint16_t>(sub.building->id - kIdOffset);
                bits |= std::uint32_t{cell.random & 0x1FU} << 17;
                if (const auto it = source.find(y << 8 | x); it != source.end() && it->second.id == id) {
                    bits |= it->second.bits & (0x3FU << 26);  // bits of unknown meaning
                }
                const std::size_t o = out.size();
                out.resize(o + kTileSize);
                put16(out, o, id);
                out[o + 2] = static_cast<std::uint8_t>(x);
                out[o + 3] = static_cast<std::uint8_t>(y);
                put32(out, o + 4, bits);
            }
        }
        return out;
    }

    /// Whether the top tile counts as the ground's (0x00468BF0): no city, and the same picture
    /// (0x00464F30), or water or grass of the same ware, growing or not. The original then does not
    /// save it; on loading, the tile shows the ground.
    [[nodiscard]] bool same_as_ground(const IslandMap& map, std::uint32_t x, std::uint32_t y) const {
        const MapCell& t = map.top(x, y);
        const MapCell& g = map.ground(x, y);
        if (t.city != MapCell::kNoCity) {
            return false;
        }
        if (t.sprite == g.sprite && t.rotation == g.rotation && t.anim_phase == g.anim_phase) {
            return true;
        }
        const assets::BuildingType* a = t.empty() ? nullptr : m_buildings.by_sprite(t.sprite);
        const assets::BuildingType* b = g.empty() ? nullptr : m_buildings.by_sprite(g.sprite);
        const auto ripe = [&](const assets::BuildingType* r) {
            return r != nullptr && r->prod_kind == kProdKindGrowing ? m_buildings.by_record(r->record + 1) : r;
        };
        a = ripe(a);
        b = ripe(b);
        constexpr std::int32_t kSea = 19;
        constexpr std::uint8_t kGrass = 52;
        return a != nullptr && b != nullptr && (a->kind == kSea || a->ware == kGrass) && a->ware == b->ware;
    }

    core::Bytes deer(std::uint16_t i) const {
        const WorldIsland& island = m_world.islands()[i];
        const auto raw = m_source.by_key("HIRSCH2", kDeerSize, [](auto r) { return key3(r[0], r[1], r[2]); });
        core::Bytes out;
        for (const DeerSpot& s : island.deer_spots) {
            if (!s.used) {
                continue;
            }
            core::Bytes r = start(raw, key3(island.header.number, s.x, s.y), kDeerSize);
            r[0] = island.header.number;
            r[1] = s.x;
            r[2] = s.y;
            put32(r, 4, clamp32(s.respawn_ms / 100));
            out.insert(out.end(), r.begin(), r.end());
        }
        return out;
    }

    // ---- Building lists --------------------------------------------------------------------------

    void production() {
        const auto raw = m_source.by_key("PRODLIST2", kProductionSize, [](auto r) { return key3(r[0], r[1], r[2]); });
        core::Bytes out;
        for (const Production& p : m_world.productions()) {
            if (!p.used) {
                continue;
            }
            const std::uint8_t n = number(p.island);
            core::Bytes r = start(raw, key3(n, p.x, p.y), kProductionSize);
            r[0] = n;
            r[1] = static_cast<std::uint8_t>(p.x);
            r[2] = static_cast<std::uint8_t>(p.y);
            put16(r, 5, p.stock);
            put16(r, 8, p.countdown);
            put16(r, 0x0A, p.raw2);
            put16(r, 0x0C, p.raw);
            r[0x0F] = p.rate;
            put16(r, 0x10, p.produced);
            put16(r, 0x12, p.elapsed);
            r[0x14] = static_cast<std::uint8_t>((r[0x14] & 0x80U) | (p.enabled ? 1U : 0U) | ((p.flags >> 6) & 1U) << 1 |
                                                (p.anim_phase & 0xFU) << 2 | ((p.flags >> 4) & 1U) << 6);
            r[0x15] = static_cast<std::uint8_t>((r[0x15] & 0xF0U) | (p.idle_cycles & 0xFU));
            out.insert(out.end(), r.begin(), r.end());
        }
        add("PRODLIST2", std::move(out));
    }

    void cost_buildings(CostBuilding::Kind kind, const char* name, std::size_t size) {
        const auto raw = m_source.by_key(name, size, [](auto r) { return key3(r[0], r[1], r[2]); });
        core::Bytes out;
        for (const CostBuilding& b : m_world.cost_buildings()) {
            if (b.kind != kind) {
                continue;
            }
            const std::uint8_t n = number(b.island);
            core::Bytes r = start(raw, key3(n, b.x, b.y), size);
            if (raw.find(key3(n, b.x, b.y)) == raw.end() && kind == CostBuilding::Kind::Shipyard) {
                r[6] = 0xFF;  // no ship under construction
            }
            r[0] = n;
            r[1] = static_cast<std::uint8_t>(b.x);
            r[2] = static_cast<std::uint8_t>(b.y);
            if (kind == CostBuilding::Kind::Shipyard) {
                r[3] = static_cast<std::uint8_t>((r[3] & ~0x08U) | (b.enabled ? 0x08U : 0U));
            }
            out.insert(out.end(), r.begin(), r.end());
        }
        add(name, std::move(out));
    }

    void houses() {
        const auto raw = m_source.by_key("SIEDLER", kHouseSize, [](auto r) { return key3(r[0], r[1], r[2]); });
        // Runtime word bits 2..10 -> dword 2 bits (0x00485530).
        constexpr std::array<std::uint8_t, 9> kServiceBits = {30, 23, 24, 28, 25, 26, 29, 31, 27};
        core::Bytes out;
        for (const House& h : m_world.houses()) {
            const std::uint8_t n = number(h.island);
            core::Bytes r = start(raw, key3(n, h.x, h.y), kHouseSize);
            std::uint32_t d0 = get32(r, 0);
            std::uint32_t d1 = get32(r, 4);
            std::uint32_t d2 = get32(r, 8);
            d0 = (d0 & 0xF8000000U) | n | std::uint32_t{h.x & 0xFFU} << 8 | std::uint32_t{h.y & 0xFFU} << 16 |
                 std::uint32_t{h.epoch & 7U} << 24;
            d1 = (d1 & 0xFFF000F8U) | (h.queue_epoch & 7U) | std::uint32_t{h.resident_class} << 8 |
                 std::uint32_t{h.market_distance & 0xFU} << 16;
            d2 = (d2 & 0x00100000U) | std::min<std::uint32_t>(h.inhabitants64, 0xFFFF) |
                 std::uint32_t{h.timer_group & 0xFU} << 16 | (h.services_complete ? 1U << 21 : 0U) |
                 (h.market ? 1U << 22 : 0U);
            for (std::size_t k = 0; k < kServiceBits.size(); ++k) {
                if ((h.services >> (k + 2) & 1U) != 0) {
                    d2 |= 1U << kServiceBits[k];
                }
            }
            put32(r, 0, d0);
            put32(r, 4, d1);
            put32(r, 8, d2);
            out.insert(out.end(), r.begin(), r.end());
        }
        add("SIEDLER", std::move(out));
    }

    /// What a tile shows after loading: the ground where the building on top is not saved.
    [[nodiscard]] const MapCell& loaded_cell(const IslandMap& map, std::uint32_t x, std::uint32_t y) const {
        const SubTile sub = map.sub_tile(m_buildings, x, y);
        if (sub.building != nullptr && map.contains(sub.origin_x, sub.origin_y) &&
            same_as_ground(map, static_cast<std::uint32_t>(sub.origin_x), static_cast<std::uint32_t>(sub.origin_y))) {
            return map.ground(x, y);
        }
        return map.top(x, y);
    }

    void growth() {
        const auto raw = m_source.by_key("ROHWACHS2", kGrowthSize, [](auto r) { return key3(r[0], r[1], r[2]); });
        // Entries for tiles that grow after loading without one (the ground's growing grass where
        // the ripe grass on top is not saved): the entries the loader would add (S-020), so loading
        // the file again gives the same list.
        std::vector<GrowthEntry> entries = m_world.growth().entries;
        std::unordered_set<std::uint64_t> present;
        for (const GrowthEntry& e : entries) {
            present.insert(key3(e.island, e.x, e.y));
        }
        for (const std::uint16_t i : island_order()) {
            const IslandMap& map = m_world.islands()[i].map;
            for (std::uint16_t y = 0; y < map.height(); ++y) {
                for (std::uint16_t x = 0; x < map.width(); ++x) {
                    if (present.contains(key3(i, x, y))) {
                        continue;
                    }
                    if (const auto e =
                            missing_growth_entry(m_world.growth(), m_buildings, i, x, y, loaded_cell(map, x, y))) {
                        entries.push_back(*e);
                    }
                }
            }
        }
        core::Bytes out;
        for (const GrowthEntry& e : entries) {
            const std::uint8_t n = number(e.island);
            core::Bytes r = start(raw, key3(n, e.x, e.y), kGrowthSize);
            r[0] = n;
            r[1] = static_cast<std::uint8_t>(e.x);
            r[2] = static_cast<std::uint8_t>(e.y);
            r[3] = e.growth_class;
            r[4] = static_cast<std::uint8_t>((r[4] & ~7U) | (e.seen & 7U));
            r[5] = e.stage;
            out.insert(out.end(), r.begin(), r.end());
        }
        add("ROHWACHS2", std::move(out));
    }

    // ---- Cities ----------------------------------------------------------------------------------

    /// Id (- 20000) of the first producer of a ware (the original's ware table, 0x00609160).
    [[nodiscard]] std::uint16_t producer_id(std::uint8_t ware) const {
        for (std::uint32_t rec = 0;; ++rec) {
            const assets::BuildingType* b = m_buildings.by_record(rec);
            if (b == nullptr) {
                return 0;
            }
            if (b->kind != 0 && b->prod_kind != 0 && b->ware == ware) {
                return static_cast<std::uint16_t>(b->id - kIdOffset);
            }
        }
    }

    void cities() {
        const auto raw_city = m_source.by_key("STADT4", kCitySize, [](auto r) { return key3(r[0], r[1], 0); });
        const auto raw_store =
            m_source.by_key("KONTOR2", kWarehouseSize, [](auto r) { return key3(r[0], (r[3] & 0xFU), 0); });
        const auto raw_market =
            m_source.by_key("MARKT2", kConsumptionSize, [](auto r) { return key3(r[0], (r[1] & 0xFU), 0); });
        core::Bytes cities;
        core::Bytes stores;
        core::Bytes markets;
        // The original keeps its cities in a table of 75; Open1602 writes them in its own order.
        for (const City& c : m_world.cities()) {
            if (!c.warehouse_x || !c.warehouse_y) {
                continue;  // the original saves only cities with a Kontor
            }
            const std::uint8_t n = number(c.island);
            const std::uint64_t key = key3(n, c.index, 0);
            core::Bytes r = start(raw_city, key, kCitySize);
            r[0] = n;
            r[1] = c.index;
            r[2] = owner_byte(c.owner);
            r[4] = static_cast<std::uint8_t>((r[4] & ~1U) | (c.material_lock ? 1U : 0U));
            for (std::size_t k = 0; k < 5; ++k) {
                put32(r, 0x5C + 4 * k, k < c.inhabitants.size() ? c.inhabitants[k] : 0);
                // The loader turns 0 into 128.
                const std::uint8_t sat = k < c.satisfaction.size() ? c.satisfaction[k] : 128;
                if (!(r[0x78 + k] == 0 && sat == 128)) {
                    r[0x78 + k] = sat;
                }
                const std::uint8_t tax = k < c.tax_rate.size() ? c.tax_rate[k] : 128;
                if (!(r[0x7F + k] == 0 && tax == 128)) {
                    r[0x7F + k] = tax;
                }
            }
            if (!(r[0x86] == 0 && c.food_ratio == 128)) {
                r[0x86] = c.food_ratio;
            }
            const auto stored = std::span<const std::uint8_t>(r).subspan(0x87);
            const std::string old(stored.begin(), std::find(stored.begin(), stored.end(), std::uint8_t{0}));
            if (old != c.name) {
                std::fill(r.begin() + 0x87, r.end(), std::uint8_t{0});
                const std::string name = c.name.substr(0, kCityNameMax);
                std::copy(name.begin(), name.end(), r.begin() + 0x87);
            }
            cities.insert(cities.end(), r.begin(), r.end());

            core::Bytes s = start(raw_store, key, kWarehouseSize);
            put32(s, 0,
                  (get32(s, 0) & 0xF0000000U) | n | std::uint32_t{*c.warehouse_x & 0xFFU} << 8 |
                      std::uint32_t{*c.warehouse_y & 0xFFU} << 16 | std::uint32_t{c.index & 0xFU} << 24);
            for (std::uint8_t w = 2; w <= 24; ++w) {
                const std::size_t o = 4 + 20 * std::size_t{w};
                const std::uint32_t stock = std::min<std::uint32_t>(c.stock(w), 0xFFFF);
                put16(s, o + 0x0C, stock);
                if (get16(s, o + 0x10) == 0 && stock != 0) {
                    put16(s, o + 0x10, producer_id(w));
                }
            }
            stores.insert(stores.end(), s.begin(), s.end());

            core::Bytes m = start(raw_market, key, kConsumptionSize);
            put32(m, 0, (get32(m, 0) & 0xFFFFF000U) | n | std::uint32_t{c.index & 0xFU} << 8);
            for (std::uint8_t w = 14; w <= 24; ++w) {
                const std::size_t o = 4 + 16 * std::size_t{w - 14U};
                if (w >= c.supply.size()) {
                    continue;
                }
                const City::Supply& sup = c.supply[w];
                put32(m, o, clamp32(sup.demand));
                put32(m, o + 4, clamp32(sup.supplied));
                m[o + 0x0E] = sup.ratio;
                if (get16(m, o + 0x0C) == 0) {
                    put16(m, o + 0x0C, producer_id(w));
                }
            }
            markets.insert(markets.end(), m.begin(), m.end());
        }
        add("STADT4", std::move(cities));
        add("KONTOR2", std::move(stores));
        add("MARKT2", std::move(markets));
    }

    // ---- Players, ships, soldiers ------------------------------------------------------------------

    void players() {
        const auto raw = m_source.records("PLAYER4", kPlayerSize);
        core::Bytes out;
        for (std::size_t p = 0; p < assets::kOriginalPlayerSlots; ++p) {
            core::Bytes r(kPlayerSize, 0);
            if (p < raw.size()) {
                std::copy(raw[p].begin(), raw[p].end(), r.begin());
            }
            r[5] = static_cast<std::uint8_t>(p);
            if (const Player* player = m_world.player(static_cast<PlayerId>(p))) {
                put32(r, 0, static_cast<std::uint32_t>(player->money));
                put32(r, 0x34, player->infra_mask);
                r[0x0E] = player->castles_built;
                r[0x0F] = player->cathedrals_built;
                put16(r, 0x10, player->arches_allowed);
                put16(r, 0x12, player->arches_built);
                put16(r, 0x38, player->monuments_allowed);
                put16(r, 0x3A, player->monuments_built);
            }
            out.insert(out.end(), r.begin(), r.end());
        }
        add("PLAYER4", std::move(out));
    }

    void ships() {
        auto raw = m_source.records("SHIP4", kShipSize);
        // The loader skips ships outside the map; the others are the world's ships in order.
        std::erase_if(raw, [&](auto r) { return !m_world.contains(get16(r, 0x1C), get16(r, 0x1E)); });
        core::Bytes out;
        for (std::size_t k = 0; k < m_world.ships().size(); ++k) {
            const Ship& s = m_world.ships()[k];
            core::Bytes r(kShipSize, 0);
            if (raw.size() == m_world.ships().size()) {
                std::copy(raw[k].begin(), raw[k].end(), r.begin());
            } else {
                put16(r, 0x46, static_cast<std::uint32_t>(k));  // slot
            }
            const auto stored = std::span<const std::uint8_t>(r).subspan(0, 0x1C);
            const std::string old(stored.begin(), std::find(stored.begin(), stored.end(), std::uint8_t{0}));
            if (old != s.name) {
                std::fill_n(r.begin(), 0x1C, std::uint8_t{0});
                const std::string name = s.name.substr(0, kShipNameMax);
                std::copy(name.begin(), name.end(), r.begin());
            }
            put16(r, 0x1C, s.x);
            put16(r, 0x1E, s.y);
            put16(r, 0x48, s.figure);
            r[0x4B] = owner_byte(s.owner);
            r[0x50] = static_cast<std::uint8_t>((r[0x50] & ~7U) | (s.direction & 7U));
            // Cargo: eight slots of the producer id and the amount (whole tons for the original).
            for (std::size_t c = 0; c < 8; ++c) {
                const std::size_t o = 0x174 + 8 * c;
                if (c < s.cargo.size()) {
                    const std::uint16_t id = producer_id(s.cargo[c].ware);
                    if (get16(r, o) == 0 || m_buildings.by_id(get16(r, o) + kIdOffset) == nullptr ||
                        m_buildings.by_id(get16(r, o) + kIdOffset)->ware != s.cargo[c].ware) {
                        put16(r, o, id);
                    }
                    put16(r, o + 2, std::min<std::uint32_t>(s.cargo[c].amount, 0xFFFF));
                } else if (get16(r, o) != 0) {
                    put16(r, o, 0);
                    put16(r, o + 2, 0);
                }
            }
            out.insert(out.end(), r.begin(), r.end());
        }
        add("SHIP4", std::move(out));
    }

    void soldiers() {
        auto raw = m_source.records("SOLDAT3", kSoldierSize);
        std::erase_if(raw, [&](auto r) { return !m_world.contains(get16(r, 0) / 2, get16(r, 2) / 2); });
        core::Bytes out;
        for (std::size_t k = 0; k < m_world.soldiers().size(); ++k) {
            const Soldier& s = m_world.soldiers()[k];
            core::Bytes r(kSoldierSize, 0);
            if (raw.size() == m_world.soldiers().size()) {
                std::copy(raw[k].begin(), raw[k].end(), r.begin());
            } else {
                put16(r, 6, s.figure);
                put16(r, 8, static_cast<std::uint32_t>(k));  // slot
            }
            // Half tiles; a stored position that gives the same one stays.
            const auto x2 = static_cast<std::uint32_t>(s.x * 2.0 + 0.5);
            const auto y2 = static_cast<std::uint32_t>(s.y * 2.0 + 0.5);
            if (get16(r, 0) / 2.0 != s.x) {
                put16(r, 0, x2);
            }
            if (get16(r, 2) / 2.0 != s.y) {
                put16(r, 2, y2);
            }
            r[0x18] = owner_byte(s.owner);
            r[0x19] = s.anim;
            r[0x1B] = static_cast<std::uint8_t>((r[0x1B] & ~7U) | (s.direction & 7U));
            out.insert(out.end(), r.begin(), r.end());
        }
        add("SOLDAT3", std::move(out));
    }

    // ---- Timers ----------------------------------------------------------------------------------

    void timers() {
        core::Bytes r(kTimersSize, 0);
        if (const auto c = m_source.chunk("TIMERS")) {
            std::copy_n(c->begin(), std::min(c->size(), r.size()), r.begin());
        }
        const GrowthState& g = m_world.growth();
        for (std::size_t k = 0; k < GrowthState::kClasses; ++k) {
            r[0x64 + k] = static_cast<std::uint8_t>((r[0x64 + k] & ~7U) | (g.counter[k] & 7U));
            // The loader takes the clock modulo the class period.
            const std::uint32_t period = growth_period_ms(static_cast<std::uint32_t>(k));
            if (get32(r, 0x218 + 4 * k) % period != g.time_ms[k]) {
                put32(r, 0x218 + 4 * k, g.time_ms[k]);
            }
        }
        const HouseClocks& h = m_world.house_clocks();
        for (std::size_t k = 0; k < HouseClocks::kGroups; ++k) {
            r[0x44 + k] = static_cast<std::uint8_t>((r[0x44 + k] & ~7U) | (h.epoch[k] & 7U));
            const std::uint32_t period = house_group_period_ms(static_cast<std::uint32_t>(k));
            if (get32(r, 0x198 + 4 * k) % period != h.time_ms[k]) {
                put32(r, 0x198 + 4 * k, h.time_ms[k]);
            }
        }
        put32(r, 0xA0, clamp32(m_world.time_ms() / 100));
        put32(r, 0xB0, m_world.city_name_index());
        r[0xA6] = m_world.game_settings().difficulty;
        put32(r, 0xA8, m_world.game_settings().flags);
        // A stored 0 loads as 60 minutes.
        const std::uint32_t drought = clamp32(m_world.next_drought_ms() / 100);
        if (!(get32(r, 0xB4) == 0 && drought == 36'000)) {
            put32(r, 0xB4, drought);
        }
        const VolcanoState& v = m_world.volcanoes();
        const auto ref = [&](std::size_t o, const std::optional<CraterRef>& c) {
            if (c) {
                r[o] = 0x33;
                r[o + 1] = number(c->island);
                r[o + 2] = c->x;
                r[o + 3] = c->y;
            } else if (r[o] != 0) {
                // Only the low byte tells (0x0048C0F0); a ref that was set is cleared whole so the
                // file does not depend on what ran before.
                std::fill_n(r.begin() + static_cast<std::ptrdiff_t>(o), 4, std::uint8_t{0});
            }
        };
        ref(0xC4, v.eruptions.empty() ? std::nullopt : std::optional<CraterRef>(v.eruptions.front().crater));
        ref(0xC8, v.last);
        put32(r, 0xCC, clamp32(v.next_ms / 100));
        std::uint32_t craters = 0;
        for (const WorldIsland& island : m_world.islands()) {
            craters += static_cast<std::uint32_t>(island.craters.size());
        }
        put32(r, 0xD0, std::min<std::uint32_t>(craters, 20));
        add("TIMERS", std::move(r));
    }
};

}  // namespace

core::Result<std::vector<SavedChunk>> savegame_chunks(const World& world, const assets::BuildingTable& buildings) {
    return Writer(world, buildings).run();
}

core::Result<core::Bytes> write_savegame(const World& world, const assets::BuildingTable& buildings,
                                         const Simulation* simulation) {
    auto chunks = savegame_chunks(world, buildings);
    if (!chunks) {
        return chunks.error();
    }
    assets::ChunkWriter out;
    for (const SavedChunk& c : *chunks) {
        out.add(c.name, c.data);
    }
    out.add_checksum();
    const core::Bytes extension = encode_extension(world, buildings, simulation, out.bytes());
    out.add(kExtensionChunk, extension);
    return out.take();
}

}  // namespace o1602::sim
