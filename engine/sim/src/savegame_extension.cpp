// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/savegame_extension.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>

#include "save_sections.hpp"

#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/chunk_writer.hpp"
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
constexpr std::uint32_t kFleet = tag("FLEE");
constexpr std::uint32_t kEpochs = tag("EPOC");
constexpr std::uint32_t kArmy = tag("ARMY");
constexpr std::uint32_t kDiplomacy = tag("DIPL");
constexpr std::uint32_t kExplored = tag("XPLR");
// Only in Open1602's own savegame (save_game.hpp): the whole map, further world settings, what the
// file holds.
constexpr std::uint32_t kMap = tag("MAPW");
constexpr std::uint32_t kWorld2 = tag("WRL2");
constexpr std::uint32_t kMeta = tag("META");
constexpr std::uint32_t kMissions = tag("MISN");
constexpr std::uint32_t kPlayerState = tag("PSTA");
constexpr std::uint32_t kCityState = tag("CSTA");
/// The original's file the game came from, without the island tiles (Open1602's own savegame only):
/// the export to a .GAM takes the bytes Open1602 does not model from it (savegame_writer.cpp).
constexpr std::uint32_t kOrigin = tag("ORIG");
/// Orders waiting behind the current one of ships and units (Shift + right click).
constexpr std::uint32_t kQueues = tag("QUEU");

// Keys of the rules (RULE): a key per setting, so settings can be added and old files keep theirs.
enum RuleKey : std::uint16_t {
    kRefund = 1,
    kRefundPercent = 2,
    kMaxShips = 3,
    kGuaranteeResources = 4,
    kMaxGameSpeed = 5,
    kDroughts = 6,
    kVolcanoRule = 7,
    kTraderPrices = 8,
    kMaxArmy = 9,
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

/// FNV-1a over what makes the game's buildings (id, picture, size) for the META section.
std::uint64_t buildings_fingerprint(const assets::BuildingTable& buildings) {
    std::uint64_t h = 0xCBF29CE484222325ULL;
    const auto mix = [&](std::uint32_t v) {
        for (int k = 0; k < 4; ++k) {
            h ^= (v >> (8 * k)) & 0xFFU;
            h *= 0x100000001B3ULL;
        }
    };
    for (const assets::BuildingType& b : buildings.types()) {
        mix(b.id);
        mix(b.gfx);
        mix(b.size_x | b.size_y << 16);
    }
    return h;
}

bool same_cell(const MapCell& a, const MapCell& b) noexcept {
    return a.sprite == b.sprite && a.rotation == b.rotation && a.anim_phase == b.anim_phase && a.owner == b.owner &&
           a.city == b.city && a.random == b.random;
}

void put_rules(Out& out, const Rules& r) {
    const std::array<std::pair<RuleKey, std::uint32_t>, 9> values = {{
        {kRefund, static_cast<std::uint32_t>(r.demolish_refund)},
        {kRefundPercent, r.refund_percent},
        {kMaxShips, r.max_ships_per_player},
        {kGuaranteeResources, r.guarantee_resources ? 1U : 0U},
        {kMaxGameSpeed, r.max_game_speed},
        {kDroughts, static_cast<std::uint32_t>(r.droughts)},
        {kVolcanoRule, static_cast<std::uint32_t>(r.volcanoes)},
        {kTraderPrices, static_cast<std::uint32_t>(r.trader_prices)},
        {kMaxArmy, r.max_army},
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
            case kTraderPrices:
                r.trader_prices = static_cast<TraderPrices>(std::min<std::uint32_t>(v, 1));
                break;
            case kMaxArmy:
                r.max_army = v;
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
    std::optional<std::uint32_t> pirate_interval_s;  ///< WRL2
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
    /// Explored crops and known deposits for every player (INSEL5 keeps seven slots and 8 bits).
    struct ExplorePart {
        std::vector<bool> explored;
        std::vector<std::uint32_t> deposit_masks;
    };
    std::optional<std::vector<ExplorePart>> explored;
    struct TileOverride {
        std::uint8_t island = 0;  ///< island number
        std::uint16_t x = 0;
        std::uint16_t y = 0;
        bool ground = false;
        MapCell cell;
    };
    std::optional<std::vector<TileOverride>> tiles;
    std::optional<std::vector<Player>> players;
    /// Missions and the end of the game (MISN).
    struct MissionPart {
        std::vector<Mission> missions;
        std::optional<World::GameEnd> end;
    };
    std::optional<MissionPart> missions;
    /// What players have of the missions and the defeat rules (PSTA), in the order of PLAY.
    std::optional<std::vector<Player>> player_state;
    std::optional<std::vector<City>> cities;
    /// Trade statistics of the cities (CSTA): island, index, purchases, sales.
    struct CityStats {
        std::uint16_t island = 0;
        std::uint8_t index = 0;
        std::int64_t purchases = 0;
        std::int64_t sales = 0;
    };
    std::optional<std::vector<CityStats>> city_state;
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
    /// Ships with their courses and routes, shipyards, Kontor trade settings, discovered islands.
    struct FleetPart {
        std::uint32_t next_ship_id = 1;
        std::vector<std::pair<std::uint8_t, std::vector<bool>>> discovered;  ///< island number, players
        struct CityTrade {
            std::uint16_t island = 0;
            std::uint8_t index = 0;
            std::vector<City::Trade> trade;
            std::vector<std::uint32_t> goodwill;
            bool trader_inbound = false;
        };
        std::vector<CityTrade> cities;
        std::vector<CostBuilding> yards;  ///< the shipyard fields, in the order of the costs list
        std::vector<Ship> ships;
    };
    std::optional<FleetPart> fleet;
    /// Soldiers with their paths, towers, building damage, shots in flight, castle slots.
    struct ArmyPart {
        std::uint32_t next_soldier_id = 1;
        std::vector<Soldier> soldiers;
        std::vector<Tower> towers;
        std::vector<BuildingDamage> damage;
        std::vector<PendingHit> hits;
        std::vector<std::pair<std::array<std::uint32_t, 3>, std::vector<CastleSlot>>> castles;
        struct ShipPart {
            std::uint32_t id = 0;
            bool sinking = false;
            std::uint64_t next_shot_ms = 0;
        };
        std::vector<ShipPart> ships;
        struct CityPart {
            std::uint16_t island = 0;
            std::uint8_t index = 0;
            std::uint64_t last_attacked_ms = 0;
            bool cursed = false;
        };
        std::vector<CityPart> cities;
    };
    std::optional<ArmyPart> army;
    std::optional<Diplomacy> diplomacy;
    /// Phase of the island epochs and the phase each island was last ticked in (island number).
    struct EpochPart {
        std::uint8_t phase = 0;
        std::vector<std::pair<std::uint8_t, std::uint8_t>> islands;
    };
    std::optional<EpochPart> epochs;
};

// ---- Map (own savegame only) ----------------------------------------------------------------------

/// What of an island header the game uses after loading. The rest of the header mirrors state the
/// island itself keeps (drought, craters, discovered by, ...), or fields of the original's file the
/// game does not use (`raw`); neither goes into Open1602's own savegame.
void put_header(Out& out, const assets::IslandHeader& h) {
    out.string(h.chunk_name);
    out.u8(h.number);
    out.u8(h.width);
    out.u8(h.height);
    out.u8(h.flags);
    out.u16(h.pos_x);
    out.u16(h.pos_y);
    out.u16(h.template_number);
    out.u16(h.template_size);
    out.u8(h.climate);
    out.boolean(h.stored_complete);
}

assets::IslandHeader get_header(StateIn& in) {
    assets::IslandHeader h;
    h.chunk_name = in.string();
    h.number = in.u8();
    h.width = in.u8();
    h.height = in.u8();
    h.flags = in.u8();
    h.pos_x = in.u16();
    h.pos_y = in.u16();
    h.template_number = in.u16();
    h.template_size = in.u16();
    h.climate = in.u8();
    h.stored_complete = in.boolean();
    return h;
}

/// The whole map: the world's size and every island with both layers of all its tiles. The cells
/// go field by field over the island (all ids, then all sub-tiles, ...), which zlib packs well.
void write_map(Out& out, const World& w, const assets::BuildingTable& buildings) {
    out.u32(w.width());
    out.u32(w.height());
    out.u32(static_cast<std::uint32_t>(w.islands().size()));
    for (const WorldIsland& i : w.islands()) {
        put_header(out, i.header);
        out.u32(i.x);
        out.u32(i.y);
        out.u32(i.map.width());
        out.u32(i.map.height());
        const std::uint32_t n = i.map.width() * i.map.height();
        for (const bool ground : {false, true}) {
            std::vector<const MapCell*> cells;
            cells.reserve(n);
            for (std::uint32_t y = 0; y < i.map.height(); ++y) {
                for (std::uint32_t x = 0; x < i.map.width(); ++x) {
                    cells.push_back(ground ? &i.map.ground(x, y) : &i.map.top(x, y));
                }
            }
            std::vector<const assets::BuildingType*> types(n);
            for (std::uint32_t k = 0; k < n; ++k) {
                types[k] = cells[k]->empty() ? nullptr : buildings.by_sprite(cells[k]->sprite);
                out.u32(types[k] != nullptr ? types[k]->id : 0);
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                out.u16(types[k] != nullptr ? static_cast<std::uint16_t>(cells[k]->sprite - types[k]->gfx) : 0);
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                out.u8(static_cast<std::uint8_t>(cells[k]->rotation | cells[k]->anim_phase << 2));
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                out.u8(cells[k]->owner);
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                out.u8(cells[k]->city);
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                out.u8(cells[k]->random);
            }
        }
    }
}

/// A world with the size and islands of the MAPW section, nothing else yet.
core::Result<World> read_map(StateIn& in, const assets::BuildingTable& buildings) {
    const std::uint32_t width = in.u32();
    const std::uint32_t height = in.u32();
    if (!in.ok() || width == 0 || height == 0 || width > 0xFFFF || height > 0xFFFF) {
        return make_error(ErrorCode::InvalidFormat, "save: impossible world size");
    }
    World world(width, height);
    const std::uint32_t islands = in.count(64);
    for (std::uint32_t i = 0; i < islands && in.ok(); ++i) {
        assets::IslandHeader header = get_header(in);
        const std::uint32_t x = in.u32();
        const std::uint32_t y = in.u32();
        const std::uint32_t w = in.u32();
        const std::uint32_t h = in.u32();
        if (!in.ok() || w == 0 || h == 0 || w > width || h > height || std::uint64_t{w} * h * 2 * 10 > in.remaining()) {
            return make_error(ErrorCode::InvalidFormat, "save: damaged island");
        }
        IslandMap map(w, h);
        const std::uint32_t n = w * h;
        for (const bool ground : {false, true}) {
            std::vector<MapCell> cells(n);
            std::vector<std::uint32_t> ids(n);
            for (std::uint32_t k = 0; k < n; ++k) {
                ids[k] = in.u32();
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                const std::uint16_t sub = in.u16();
                if (ids[k] != 0) {
                    const assets::BuildingType* b = buildings.by_id(ids[k]);
                    if (b == nullptr) {
                        return make_error(ErrorCode::InvalidFormat,
                                          "save: building " + std::to_string(ids[k]) + " is not in the game's data");
                    }
                    cells[k].sprite = static_cast<std::uint16_t>(b->gfx + sub);
                }
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                const std::uint8_t v = in.u8();
                cells[k].rotation = static_cast<std::uint8_t>(v & 3U);
                cells[k].anim_phase = static_cast<std::uint8_t>(v >> 2 & 0x0FU);
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                cells[k].owner = in.u8();
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                cells[k].city = in.u8();
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                cells[k].random = in.u8();
            }
            for (std::uint32_t k = 0; k < n; ++k) {
                if (ground) {
                    map.set_ground_cell(k % w, k / w, cells[k]);
                } else {
                    map.set_top_cell(k % w, k / w, cells[k]);
                }
            }
        }
        if (!in.ok()) {
            break;
        }
        WorldIsland island{std::move(header), std::move(map), x, y};
        if (auto status = world.add_island(std::move(island)); !status) {
            return status.error();
        }
    }
    if (!in.ok()) {
        return make_error(ErrorCode::InvalidFormat, "save: damaged map");
    }
    return world;
}

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

void write_explored(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.islands().size()));
    for (const WorldIsland& i : w.islands()) {
        out.u32(static_cast<std::uint32_t>(i.explored.size()));
        for (const bool e : i.explored) {
            out.boolean(e);
        }
        out.u32(static_cast<std::uint32_t>(i.header.deposits.size()));
        for (const auto& d : i.header.deposits) {
            out.u32(d.mask);
        }
    }
}

std::vector<Decoded::ExplorePart> read_explored(StateIn& in) {
    std::vector<Decoded::ExplorePart> list(in.count(8));
    for (auto& part : list) {
        part.explored.resize(in.count(1));
        for (std::size_t p = 0; p < part.explored.size(); ++p) {
            part.explored[p] = in.boolean();
        }
        part.deposit_masks.resize(in.count(4));
        for (auto& m : part.deposit_masks) {
            m = in.u32();
        }
    }
    return list;
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
            out.u8(static_cast<std::uint8_t>(d.mask));  // all bits in XPLR
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

void put_city_goal(Out& out, const Mission::CityGoal& g) {
    out.i32(g.inhabitants);
    out.u8(g.resident_class);
    out.u32(g.count);
}

void write_missions(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.missions().size()));
    for (const Mission& m : w.missions()) {
        out.u32(m.goals);
        out.u32(m.losses);
        out.u8(m.target);
        out.u16(m.min_defeated);
        out.u32(static_cast<std::uint32_t>(m.defeat.size()));
        for (const PlayerId p : m.defeat) {
            out.u8(p);
        }
        out.i32(m.min_cities);
        out.i32(m.min_inhabitants);
        out.i32(m.min_balance);
        out.i32(m.min_trade_balance);
        out.i32(m.protected_min_cities);
        out.u16(m.success_message);
        out.string(m.text);
        for (const Mission::GoodsGoal& g : m.goods) {
            out.u8(g.ware);
            out.u32(g.amount);
        }
        for (const Mission::IslandGoal& g : m.islands) {
            out.u8(g.resource);
            out.u32(static_cast<std::uint32_t>(g.islands.size()));
            for (const std::uint16_t i : g.islands) {
                out.u16(i);
            }
        }
        for (const Mission::CityGoal& g : m.cities) {
            put_city_goal(out, g);
        }
        put_city_goal(out, m.every_city);
        put_city_goal(out, m.target_city);
        put_city_goal(out, m.protected_city);
        out.i32(m.unused_48);
        out.i32(m.unused_58);
    }
    out.boolean(w.game_end().has_value());
    if (w.game_end()) {
        out.u64(w.game_end()->time_ms);
        out.u8(w.game_end()->winner);
    }
}

void write_player_state(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.players().size()));
    for (const Player& p : w.players()) {
        out.u8(p.mission);
        out.u8(static_cast<std::uint8_t>(p.status));
        out.boolean(p.won);
        out.u16(p.opponents_defeated);
        out.u8(p.bankrupt_checks);
        out.u8(p.empty_checks);
        out.i64(p.purchases);
        out.i64(p.sales);
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

void write_city_state(Out& out, const World& w) {
    out.u32(static_cast<std::uint32_t>(w.cities().size()));
    for (const City& c : w.cities()) {
        out.u16(c.island);
        out.u8(c.index);
        out.i64(c.purchases);
        out.i64(c.sales);
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
        // The cargo as a list of the filled holds (the format of the first version).
        std::uint32_t filled = 0;
        for (const CargoSlot& c : s.cargo) {
            filled += c.ware != 0 ? 1U : 0U;
        }
        out.u32(filled);
        for (const CargoSlot& c : s.cargo) {
            if (c.ware != 0) {
                out.u8(c.ware);
                out.u32(c.amount);
            }
        }
    }
}

void put_target(Out& out, const Target& t) {
    out.u8(static_cast<std::uint8_t>(t.kind));
    out.u16(t.island);
    out.u8(t.city);
    out.u32(t.x);
    out.u32(t.y);
    out.u32(t.ship);
    out.u32(t.raw);
}

Target get_target(StateIn& in) {
    Target t;
    const std::uint8_t kind = in.u8();
    if (kind > static_cast<std::uint8_t>(Target::Kind::Spot)) {
        in.fail();
    }
    t.kind = static_cast<Target::Kind>(kind);
    t.island = in.u16();
    t.city = in.u8();
    t.x = in.u32();
    t.y = in.u32();
    t.ship = in.u32();
    t.raw = in.u32();
    return t;
}

void put_ship(Out& out, const Ship& s) {
    out.u32(s.id);
    out.string(s.name);
    out.i32(s.pos_x);
    out.i32(s.pos_y);
    out.u16(s.figure);
    out.u8(s.owner);
    out.u8(s.direction);
    out.u32(s.health);
    out.u8(s.cannons);
    out.u32(static_cast<std::uint32_t>(s.cargo.size()));
    for (const CargoSlot& c : s.cargo) {
        out.u8(c.ware);
        out.u32(c.amount);
        out.u16(c.price);
    }
    for (const RouteStop& stop : s.route.stops) {
        put_target(out, stop.target);
        for (const RouteEntry& e : stop.entries) {
            out.u8(e.ware);
            out.boolean(e.unload);
            out.u32(e.amount);
        }
    }
    out.u8(s.route.current);
    out.boolean(s.route.active);
    out.boolean(s.route.one_way);
    out.boolean(s.route.problem);
    out.u8(s.route.problem_rounds);
    put_target(out, s.target);
    put_target(out, s.order);
    put_target(out, s.previous);
    out.boolean(s.held);
    out.boolean(s.repairing);
    out.boolean(s.for_sale);
    out.u16(s.sale_factor);
    out.boolean(s.trader);
    out.u32(static_cast<std::uint32_t>(s.trader_goods.size()));
    for (const TraderGood& g : s.trader_goods) {
        out.u16(g.price);
        out.u8(g.age);
        out.u32(g.amount);
    }
    out.u32(s.trader_tools_ms);
    out.u32(s.trader_ore_ms);
    out.u16(s.near_island);
    out.u16(s.source_slot);
    const ShipMotion& m = s.motion;
    out.u8(static_cast<std::uint8_t>(m.phase));
    out.u8(m.count);
    out.u32(m.elapsed_us);
    out.u32(m.duration_us);
    out.i32(m.from_x);
    out.i32(m.from_y);
    put_u8s(out, m.path);
    out.u8(m.path_index);
    out.u8(m.radius);
    out.u8(m.failures);
    out.u8(m.turn_to);
    out.u64(m.next_follow_us);
}

Ship get_ship(StateIn& in) {
    Ship s;
    s.id = in.u32();
    s.name = in.string();
    s.pos_x = in.i32();
    s.pos_y = in.i32();
    s.x = static_cast<std::uint16_t>(std::max(0, s.pos_x / kShipUnit));
    s.y = static_cast<std::uint16_t>(std::max(0, s.pos_y / kShipUnit));
    s.figure = in.u16();
    s.owner = in.u8();
    s.direction = static_cast<std::uint8_t>(in.u8() & 7U);
    s.health = in.u32();
    s.cannons = in.u8();
    s.cargo.resize(in.count(7));
    for (CargoSlot& c : s.cargo) {
        c.ware = in.u8();
        c.amount = in.u32();
        c.price = in.u16();
    }
    for (RouteStop& stop : s.route.stops) {
        stop.target = get_target(in);
        for (RouteEntry& e : stop.entries) {
            e.ware = in.u8();
            e.unload = in.boolean();
            e.amount = in.u32();
        }
    }
    s.route.current = static_cast<std::uint8_t>(in.u8() % TradeRoute::kStops);
    s.route.active = in.boolean();
    s.route.one_way = in.boolean();
    s.route.problem = in.boolean();
    s.route.problem_rounds = in.u8();
    s.target = get_target(in);
    s.order = get_target(in);
    s.previous = get_target(in);
    s.held = in.boolean();
    s.repairing = in.boolean();
    s.for_sale = in.boolean();
    s.sale_factor = in.u16();
    s.trader = in.boolean();
    s.trader_goods.resize(in.count(7));
    for (TraderGood& g : s.trader_goods) {
        g.price = in.u16();
        g.age = in.u8();
        g.amount = in.u32();
    }
    s.trader_tools_ms = in.u32();
    s.trader_ore_ms = in.u32();
    s.near_island = in.u16();
    s.source_slot = in.u16();
    ShipMotion& m = s.motion;
    const std::uint8_t phase = in.u8();
    if (phase > static_cast<std::uint8_t>(ShipMotion::Phase::Sinking)) {
        in.fail();
    }
    m.phase = static_cast<ShipMotion::Phase>(phase);
    m.count = in.u8();
    m.elapsed_us = in.u32();
    m.duration_us = in.u32();
    m.from_x = in.i32();
    m.from_y = in.i32();
    m.path.resize(in.count(1));
    for (std::uint8_t& c : m.path) {
        c = in.u8();
    }
    m.path_index = in.u8();
    m.radius = in.u8();
    m.failures = in.u8();
    m.turn_to = in.u8();
    m.next_follow_us = in.u64();
    return s;
}

void write_fleet(Out& out, const World& w) {
    out.u32(w.next_ship_id());
    out.u32(static_cast<std::uint32_t>(w.islands().size()));
    for (const WorldIsland& i : w.islands()) {
        out.u8(i.header.number);
        out.u32(static_cast<std::uint32_t>(i.discovered.size()));
        for (const bool d : i.discovered) {
            out.boolean(d);
        }
    }
    out.u32(static_cast<std::uint32_t>(w.cities().size()));
    for (const City& c : w.cities()) {
        out.u16(c.island);
        out.u8(c.index);
        out.u32(static_cast<std::uint32_t>(c.trade.size()));
        for (const City::Trade& t : c.trade) {
            out.boolean(t.sell);
            out.boolean(t.buy);
            out.u16(t.sell_price);
            out.u16(t.buy_price);
            out.u32(t.keep);
            out.u32(t.limit);
        }
        put_u32s(out, c.goodwill);
        out.boolean(c.trader_inbound);
    }
    out.u32(static_cast<std::uint32_t>(w.cost_buildings().size()));
    for (const CostBuilding& b : w.cost_buildings()) {
        out.u8(b.order);
        out.u32(b.progress);
        out.u32(b.wood);
        out.u32(b.cloth);
        out.u8(b.rate);
        out.u16(b.countdown);
        out.u32(b.repair_ship);
        out.u8(b.failed_launches);
        out.u8(b.idle_cycles);
        out.boolean(b.fetching);
    }
    out.u32(static_cast<std::uint32_t>(w.ships().size()));
    for (const Ship& s : w.ships()) {
        put_ship(out, s);
    }
}

Decoded::FleetPart read_fleet(StateIn& in) {
    Decoded::FleetPart f;
    f.next_ship_id = in.u32();
    f.discovered.resize(in.count(5));
    for (auto& [number, players] : f.discovered) {
        number = in.u8();
        players.resize(in.count(1));
        for (std::size_t p = 0; p < players.size(); ++p) {
            players[p] = in.boolean();
        }
    }
    f.cities.resize(in.count(12));
    for (auto& c : f.cities) {
        c.island = in.u16();
        c.index = in.u8();
        c.trade.resize(in.count(14));
        for (City::Trade& t : c.trade) {
            t.sell = in.boolean();
            t.buy = in.boolean();
            t.sell_price = in.u16();
            t.buy_price = in.u16();
            t.keep = in.u32();
            t.limit = in.u32();
        }
        c.goodwill.resize(in.count(4));
        for (std::uint32_t& g : c.goodwill) {
            g = in.u32();
        }
        c.trader_inbound = in.boolean();
    }
    f.yards.resize(in.count(23));
    for (CostBuilding& b : f.yards) {
        b.order = in.u8();
        b.progress = in.u32();
        b.wood = in.u32();
        b.cloth = in.u32();
        b.rate = in.u8();
        b.countdown = in.u16();
        b.repair_ship = in.u32();
        b.failed_launches = in.u8();
        b.idle_cycles = in.u8();
        b.fetching = in.boolean();
    }
    f.ships.resize(in.count(40));
    for (Ship& s : f.ships) {
        s = get_ship(in);
    }
    return f;
}

// ---- Military and diplomacy -----------------------------------------------------------------

void put_soldier(Out& out, const Soldier& s) {
    out.u32(s.id);
    out.i32(s.pos_x);
    out.i32(s.pos_y);
    out.u16(s.figure);
    out.u8(s.owner);
    out.u8(s.direction);
    out.u8(s.anim);
    out.u16(s.island);
    out.u32(s.health);
    put_target(out, s.home);
    put_target(out, s.target);
    out.u8(s.radius);
    out.u8(s.failures);
    out.u64(s.next_attack_ms);
    out.u64(s.next_follow_ms);
    out.u64(s.last_attack_ms);
    out.boolean(s.patrol.active);
    out.boolean(s.patrol.one_way);
    out.u8(s.patrol.index);
    put_target(out, s.patrol.points[0]);
    put_target(out, s.patrol.points[1]);
    out.boolean(s.disbanded);
    out.u16(s.source_slot);
    const SoldierMotion& m = s.motion;
    out.u8(static_cast<std::uint8_t>(m.phase));
    out.u8(m.direction);
    out.u8(m.count);
    out.u32(m.elapsed_us);
    out.u32(m.duration_us);
    out.i32(m.from_x);
    out.i32(m.from_y);
    put_u8s(out, m.path);
    out.u8(m.path_index);
}

Soldier get_soldier(StateIn& in) {
    Soldier s;
    s.id = in.u32();
    s.pos_x = in.i32();
    s.pos_y = in.i32();
    s.figure = in.u16();
    s.owner = in.u8();
    s.direction = static_cast<std::uint8_t>(in.u8() & 7U);
    s.anim = in.u8();
    s.island = in.u16();
    s.health = in.u32();
    s.home = get_target(in);
    s.target = get_target(in);
    s.radius = in.u8();
    s.failures = in.u8();
    s.next_attack_ms = in.u64();
    s.next_follow_ms = in.u64();
    s.last_attack_ms = in.u64();
    s.patrol.active = in.boolean();
    s.patrol.one_way = in.boolean();
    s.patrol.index = static_cast<std::uint8_t>(in.u8() & 1U);
    s.patrol.points[0] = get_target(in);
    s.patrol.points[1] = get_target(in);
    s.disbanded = in.boolean();
    s.source_slot = in.u16();
    SoldierMotion& m = s.motion;
    const std::uint8_t phase = in.u8();
    if (phase > static_cast<std::uint8_t>(SoldierMotion::Phase::Healing)) {
        in.fail();
    }
    m.phase = static_cast<SoldierMotion::Phase>(phase);
    m.direction = static_cast<std::uint8_t>(in.u8() & 7U);
    m.count = in.u8();
    m.elapsed_us = in.u32();
    m.duration_us = in.u32();
    m.from_x = in.i32();
    m.from_y = in.i32();
    const std::size_t runs = in.count(1);
    for (std::size_t k = 0; k < runs; ++k) {
        m.path.push_back(in.u8());
    }
    m.path_index = in.u8();
    return s;
}

void write_army(Out& out, const World& w) {
    out.u32(w.next_soldier_id());
    out.u32(static_cast<std::uint32_t>(w.soldiers().size()));
    for (const Soldier& s : w.soldiers()) {
        put_soldier(out, s);
    }
    out.u32(static_cast<std::uint32_t>(w.towers().size()));
    for (const Tower& t : w.towers()) {
        out.u16(t.island);
        out.u16(t.x);
        out.u16(t.y);
        out.u8(t.owner);
        out.u16(t.figure);
        out.u8(t.direction);
        out.u64(t.next_shot_ms);
        out.u32(t.next_think_us);
        out.u16(t.source_slot);
    }
    out.u32(static_cast<std::uint32_t>(w.building_damage().size()));
    for (const BuildingDamage& d : w.building_damage()) {
        out.u16(d.island);
        out.u16(d.x);
        out.u16(d.y);
        out.u32(d.damage);
    }
    out.u32(static_cast<std::uint32_t>(w.pending_hits().size()));
    for (const PendingHit& h : w.pending_hits()) {
        out.u64(h.due_ms);
        put_target(out, h.shooter);
        out.u8(h.attacker);
        out.boolean(h.cannon);
        out.u32(h.damage);
        put_target(out, h.target);
        out.i32(h.from_x);
        out.i32(h.from_y);
    }
    // Combat state of ships and cities.
    out.u32(static_cast<std::uint32_t>(w.ships().size()));
    for (const Ship& s : w.ships()) {
        out.u32(s.id);
        out.boolean(s.sinking);
        out.u64(s.next_shot_ms);
    }
    out.u32(static_cast<std::uint32_t>(w.cities().size()));
    for (const City& c : w.cities()) {
        out.u16(c.island);
        out.u8(c.index);
        out.u64(c.last_attacked_ms);
        out.boolean(c.cursed);
    }
    // Castles, in the order of the costs list.
    out.u32(static_cast<std::uint32_t>(w.cost_buildings().size()));
    for (const CostBuilding& b : w.cost_buildings()) {
        for (const std::uint32_t weapon : b.weapons) {
            out.u32(weapon);
        }
        out.u32(static_cast<std::uint32_t>(b.slots.size()));
        for (const CastleSlot& slot : b.slots) {
            out.u16(slot.figure);
            out.boolean(slot.training);
            out.boolean(slot.armed);
            out.u32(slot.health);
            out.u16(slot.progress);
        }
    }
}

Decoded::ArmyPart read_army(StateIn& in) {
    Decoded::ArmyPart a;
    a.next_soldier_id = in.u32();
    a.soldiers.resize(in.count(60));
    for (Soldier& s : a.soldiers) {
        s = get_soldier(in);
    }
    a.towers.resize(in.count(24));
    for (Tower& t : a.towers) {
        t.island = in.u16();
        t.x = in.u16();
        t.y = in.u16();
        t.owner = in.u8();
        t.figure = in.u16();
        t.direction = static_cast<std::uint8_t>(in.u8() & 7U);
        t.next_shot_ms = in.u64();
        t.next_think_us = in.u32();
        t.source_slot = in.u16();
    }
    a.damage.resize(in.count(10));
    for (BuildingDamage& d : a.damage) {
        d.island = in.u16();
        d.x = in.u16();
        d.y = in.u16();
        d.damage = in.u32();
    }
    a.hits.resize(in.count(40));
    for (PendingHit& h : a.hits) {
        h.due_ms = in.u64();
        h.shooter = get_target(in);
        h.attacker = in.u8();
        h.cannon = in.boolean();
        h.damage = in.u32();
        h.target = get_target(in);
        h.from_x = in.i32();
        h.from_y = in.i32();
    }
    a.ships.resize(in.count(13));
    for (auto& ship : a.ships) {
        ship.id = in.u32();
        ship.sinking = in.boolean();
        ship.next_shot_ms = in.u64();
    }
    a.cities.resize(in.count(12));
    for (auto& city : a.cities) {
        city.island = in.u16();
        city.index = in.u8();
        city.last_attacked_ms = in.u64();
        city.cursed = in.boolean();
    }
    a.castles.resize(in.count(16));
    for (auto& [weapons, slots] : a.castles) {
        for (std::uint32_t& weapon : weapons) {
            weapon = in.u32();
        }
        slots.resize(in.count(10));
        for (CastleSlot& slot : slots) {
            slot.figure = in.u16();
            slot.training = in.boolean();
            slot.armed = in.boolean();
            slot.health = in.u32();
            slot.progress = in.u16();
        }
    }
    return a;
}

void write_diplomacy(Out& out, const World& w) {
    const Diplomacy& d = w.diplomacy();
    const auto n = static_cast<PlayerId>(d.size());
    out.u32(n);
    for (PlayerId a = 0; a < n; ++a) {
        for (PlayerId b = 0; b < n; ++b) {
            const Relation& r = d.relation(a, b);
            out.u8(static_cast<std::uint8_t>(r.peace));
            out.u8(static_cast<std::uint8_t>(r.trade));
            out.u64(r.war_time_ms);
            out.u32(r.tribute);
            out.u32(r.trade_mood);
            out.u32(r.damage);
        }
        for (const DiplomacyEvent& e : d.events(a)) {
            out.u8(static_cast<std::uint8_t>(e.type));
            out.u8(e.partner);
            out.u64(e.time_ms);
        }
        out.u8(d.blamed(a));
    }
}

Diplomacy read_diplomacy(StateIn& in) {
    Diplomacy d;
    const std::uint32_t n = in.u32();
    if (n > 254) {
        in.fail();
        return d;
    }
    d.resize(n);
    for (std::uint32_t a = 0; a < n && in.ok(); ++a) {
        const auto pa = static_cast<PlayerId>(a);
        for (std::uint32_t b = 0; b < n; ++b) {
            Relation& r = d.relation(pa, static_cast<PlayerId>(b));
            r.peace = static_cast<Treaty>(in.u8() & 3U);
            r.trade = static_cast<Treaty>(in.u8() & 3U);
            r.war_time_ms = in.u64();
            r.tribute = in.u32();
            r.trade_mood = in.u32();
            r.damage = in.u32();
        }
        std::array<DiplomacyEvent, Diplomacy::kEvents> events{};
        for (DiplomacyEvent& e : events) {
            const std::uint8_t type = in.u8();
            e.type = type <= 13 ? static_cast<DiplomacyEventType>(type) : DiplomacyEventType::None;
            e.partner = in.u8();
            e.time_ms = in.u64();
        }
        d.set_events(pa, events);
        d.set_blamed(pa, in.u8());
    }
    return d;
}

void write_epochs(Out& out, const World& w) {
    out.u8(w.island_phase());
    out.u32(static_cast<std::uint32_t>(w.islands().size()));
    for (const WorldIsland& i : w.islands()) {
        out.u8(i.header.number);
        out.u8(i.last_phase);
    }
}

Decoded::EpochPart read_epochs(StateIn& in) {
    Decoded::EpochPart e;
    e.phase = static_cast<std::uint8_t>(in.u8() & 7U);
    e.islands.resize(in.count(2));
    for (auto& [number, phase] : e.islands) {
        number = in.u8();
        phase = static_cast<std::uint8_t>(in.u8() & 7U);
    }
    return e;
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

Mission::CityGoal get_city_goal(StateIn& in) {
    Mission::CityGoal g;
    g.inhabitants = in.i32();
    g.resident_class = in.u8();
    g.count = in.u32();
    return g;
}

Decoded::MissionPart read_missions(StateIn& in, std::size_t islands) {
    Decoded::MissionPart part;
    part.missions.resize(in.count(40));
    for (Mission& m : part.missions) {
        m.goals = in.u32();
        m.losses = in.u32();
        m.target = in.u8();
        m.min_defeated = in.u16();
        m.defeat.resize(in.count(1));
        for (PlayerId& p : m.defeat) {
            p = in.u8();
        }
        m.min_cities = in.i32();
        m.min_inhabitants = in.i32();
        m.min_balance = in.i32();
        m.min_trade_balance = in.i32();
        m.protected_min_cities = in.i32();
        m.success_message = in.u16();
        m.text = in.string();
        for (Mission::GoodsGoal& g : m.goods) {
            g.ware = in.u8();
            g.amount = in.u32();
        }
        for (Mission::IslandGoal& g : m.islands) {
            g.resource = in.u8();
            g.islands.resize(in.count(2));
            for (std::uint16_t& i : g.islands) {
                i = in.u16();
                if (i >= islands) {
                    in.fail();  // the island list and the islands do not belong together
                }
            }
        }
        for (Mission::CityGoal& g : m.cities) {
            g = get_city_goal(in);
        }
        m.every_city = get_city_goal(in);
        m.target_city = get_city_goal(in);
        m.protected_city = get_city_goal(in);
        m.unused_48 = in.i32();
        m.unused_58 = in.i32();
        if ((m.goals & ~(goal::kKnown | goal::kComputers)) != 0 || (m.losses & ~loss::kKnown) != 0) {
            in.fail();  // goals of a later version
        }
    }
    if (in.boolean()) {
        World::GameEnd end;
        end.time_ms = in.u64();
        end.winner = in.u8();
        part.end = end;
    }
    return part;
}

std::vector<Player> read_player_state(StateIn& in) {
    std::vector<Player> list(in.count(23));
    for (Player& p : list) {
        p.mission = in.u8();
        const std::uint8_t status = in.u8();
        if (status > static_cast<std::uint8_t>(PlayerStatus::Absent)) {
            in.fail();
        }
        p.status = static_cast<PlayerStatus>(status);
        p.won = in.boolean();
        p.opponents_defeated = in.u16();
        p.bankrupt_checks = in.u8();
        p.empty_checks = in.u8();
        p.purchases = in.i64();
        p.sales = in.i64();
    }
    return list;
}

std::vector<Decoded::CityStats> read_city_state(StateIn& in) {
    std::vector<Decoded::CityStats> list(in.count(19));
    for (Decoded::CityStats& c : list) {
        c.island = in.u16();
        c.index = in.u8();
        c.purchases = in.i64();
        c.sales = in.i64();
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
        for (CargoSlot& c : s.cargo) {
            c.ware = in.u8();
            c.amount = in.u32();
        }
        s.place(s.x, s.y);
    }
    return list;
}

std::vector<Soldier> read_soldiers(StateIn& in) {
    std::vector<Soldier> list(in.count(21));
    for (Soldier& s : list) {
        s.pos_x = static_cast<std::int32_t>(in.real() * kPosUnit);
        s.pos_y = static_cast<std::int32_t>(in.real() * kPosUnit);
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
    if (d.world && d.world->randoms.size() > static_cast<std::size_t>(RandomStream::Count)) {
        return bad("random generators do not match");
    }
    if (d.fleet) {
        for (const Ship& s : d.fleet->ships) {
            if (!w.contains(s.x, s.y)) {
                return bad("ship outside the world");
            }
        }
    }
    if (d.army) {
        for (const Soldier& s : d.army->soldiers) {
            if (s.island != 0xFFFF && !island_ok(w, s.island)) {
                return bad("unit on an unknown island");
            }
        }
    }
    return core::Ok{};
}

/// Checks everything decoded against the world and applies it; the world stays as it was if a
/// check fails.
core::Status apply_decoded(World& world, Decoded& d) {
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
        World::GameSettings settings = w.settings;
        settings.pirate_interval_s = d.pirate_interval_s.value_or(world.game_settings().pirate_interval_s);
        world.set_game_settings(settings);
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
    if (d.explored && d.explored->size() == world.islands().size()) {
        for (std::size_t i = 0; i < d.explored->size(); ++i) {
            const auto& part = (*d.explored)[i];
            WorldIsland& island = world.island(i);
            island.explored = part.explored;
            if (part.deposit_masks.size() == island.header.deposits.size()) {
                for (std::size_t k = 0; k < part.deposit_masks.size(); ++k) {
                    island.header.deposits[k].mask = part.deposit_masks[k];
                }
            }
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
        // Files of an earlier version have no PSTA: what the original's chunks gave stays.
        if (!d.player_state && d.players->size() == world.players().size()) {
            d.player_state = world.players();
        }
        world.set_players(std::move(*d.players));
    }
    if (d.missions) {
        world.set_missions(std::move(d.missions->missions));
        world.set_game_end(d.missions->end);
    }
    if (d.player_state && d.player_state->size() == world.players().size()) {
        for (std::size_t i = 0; i < d.player_state->size(); ++i) {
            const Player& from = (*d.player_state)[i];
            Player* pp = world.player(static_cast<PlayerId>(i));
            if (pp == nullptr) {
                continue;
            }
            Player& p = *pp;
            p.mission = from.mission < world.missions().size() ? from.mission : kNoMission;
            p.status = from.status;
            p.won = from.won;
            p.opponents_defeated = from.opponents_defeated;
            p.bankrupt_checks = from.bankrupt_checks;
            p.empty_checks = from.empty_checks;
            p.purchases = from.purchases;
            p.sales = from.sales;
        }
    }
    if (d.cities) {
        // Files of an earlier version have no trade settings: those of the original's chunks stay.
        for (City& c : *d.cities) {
            if (const City* old = world.city(c.island, c.index)) {
                c.trade = old->trade;
                c.goodwill = old->goodwill;
                // Files of an earlier version have no CSTA: the statistics of the original's chunks stay.
                c.purchases = old->purchases;
                c.sales = old->sales;
            }
        }
        world.set_cities(std::move(*d.cities));
        if (d.city_state) {
            for (const Decoded::CityStats& st : *d.city_state) {
                if (City* c = world.city(st.island, st.index)) {
                    c->purchases = st.purchases;
                    c->sales = st.sales;
                }
            }
        }
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
        // The shipyard fields of files of an earlier version come from WERFT.
        for (CostBuilding& b : *d.costs) {
            for (const CostBuilding& old : world.cost_buildings()) {
                if (old.kind == b.kind && old.island == b.island && old.x == b.x && old.y == b.y) {
                    const bool enabled = b.enabled;
                    b = old;
                    b.enabled = enabled;
                }
            }
        }
        world.cost_buildings() = std::move(*d.costs);
    }
    if (d.ships && !d.fleet && d.ships->size() != world.ships().size()) {
        // Files of an earlier version: the ships of SHIP4 are the same ones with more fields;
        // only a different number replaces them.
        world.ships().clear();
        for (Ship& ship : *d.ships) {
            world.add_ship(std::move(ship));
        }
    }
    if (d.epochs) {
        world.set_island_phase(d.epochs->phase);
        for (const auto& [number, phase] : d.epochs->islands) {
            if (const auto i = world.island_index(number)) {
                world.island(*i).last_phase = phase;
            }
        }
    }
    if (d.fleet) {
        world.set_next_ship_id(d.fleet->next_ship_id);
        for (const auto& [number, players] : d.fleet->discovered) {
            if (const auto i = world.island_index(number)) {
                world.island(*i).discovered = players;
            }
        }
        for (auto& c : d.fleet->cities) {
            if (City* city = world.city(c.island, c.index)) {
                city->trade = std::move(c.trade);
                city->goodwill = std::move(c.goodwill);
                city->trader_inbound = c.trader_inbound;
            }
        }
        if (d.fleet->yards.size() == world.cost_buildings().size()) {
            for (std::size_t i = 0; i < d.fleet->yards.size(); ++i) {
                CostBuilding& b = world.cost_buildings()[i];
                const CostBuilding& y = d.fleet->yards[i];
                b.order = y.order;
                b.progress = y.progress;
                b.wood = y.wood;
                b.cloth = y.cloth;
                b.rate = y.rate;
                b.countdown = y.countdown;
                b.repair_ship = y.repair_ship;
                b.failed_launches = y.failed_launches;
                b.idle_cycles = y.idle_cycles;
                b.fetching = y.fetching;
            }
        }
        world.ships() = std::move(d.fleet->ships);
        for (const Ship& ship : world.ships()) {
            world.set_next_ship_id(std::max(world.next_ship_id(), ship.id + 1));
        }
    }
    if (d.soldiers && !d.army && d.soldiers->size() != world.soldiers().size()) {
        // Files of an earlier version: the units of SOLDAT3 are the same ones with more fields;
        // only a different number replaces them.
        world.set_soldiers(std::move(*d.soldiers));
    }
    if (d.army) {
        world.set_soldiers(std::move(d.army->soldiers));
        world.set_next_soldier_id(d.army->next_soldier_id);
        for (const Soldier& s : world.soldiers()) {
            world.set_next_soldier_id(std::max(world.next_soldier_id(), s.id + 1));
        }
        world.towers() = std::move(d.army->towers);
        world.building_damage() = std::move(d.army->damage);
        world.pending_hits() = std::move(d.army->hits);
        for (const auto& part : d.army->ships) {
            if (Ship* ship = world.ship(part.id)) {
                ship->sinking = part.sinking;
                ship->next_shot_ms = part.next_shot_ms;
            }
        }
        for (const auto& part : d.army->cities) {
            if (City* city = world.city(part.island, part.index)) {
                city->last_attacked_ms = part.last_attacked_ms;
                city->cursed = part.cursed;
            }
        }
        if (d.army->castles.size() == world.cost_buildings().size()) {
            for (std::size_t i = 0; i < d.army->castles.size(); ++i) {
                world.cost_buildings()[i].weapons = d.army->castles[i].first;
                world.cost_buildings()[i].slots = std::move(d.army->castles[i].second);
            }
        }
    }
    if (d.diplomacy && d.diplomacy->size() == world.players().size()) {
        world.diplomacy() = std::move(*d.diplomacy);
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

}  // namespace

core::Bytes encode_map_size(std::uint32_t width, std::uint32_t height) {
    Out out;
    out.u32(width);
    out.u32(height);
    return out.take();
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> map_size(const assets::ChunkFile& file) {
    const assets::Chunk* c = file.find(kMapChunk);
    if (c == nullptr) {
        return std::nullopt;
    }
    core::ByteReader in(file.payload(*c));
    const auto w = in.u32();
    const auto h = in.u32();
    if (!w || !h || *w == 0 || *h == 0 || *w > 0xFFFF || *h > 0xFFFF) {
        return std::nullopt;
    }
    return std::pair{*w, *h};
}

std::uint64_t savegame_fingerprint(std::span<const std::uint8_t> bytes) noexcept {
    std::uint64_t h = 0xCBF29CE484222325ULL;
    for (const std::uint8_t b : bytes) {
        h ^= b;
        h *= 0x100000001B3ULL;
    }
    return h;
}

namespace detail {

core::Bytes encode_sections(const World& world, const assets::BuildingTable& buildings, const Simulation* simulation,
                            TileMode tiles, std::span<const std::uint8_t> before) {
    Out out;
    if (tiles == TileMode::Full) {
        section(out, kMeta, [&](Out& o) {
            o.u32(world.width());
            o.u32(world.height());
            o.u64(world.time_ms());
            o.u64(buildings_fingerprint(buildings));
            const PlayerId local = first_human_player(world);
            const Player* p = world.player(local);
            o.string(p != nullptr ? p->name : std::string());
        });
        section(out, kMap, [&](Out& o) { write_map(o, world, buildings); });
    }
    // The settings: those of the running game, or those the world was loaded with.
    if (simulation != nullptr || world.saved_rules()) {
        const Rules rules = simulation != nullptr ? simulation->rules() : *world.saved_rules();
        section(out, kRules, [&](Out& o) { put_rules(o, rules); });
    }
    section(out, kWorld, [&](Out& o) { write_world(o, world); });
    if (tiles == TileMode::Full) {
        section(out, kWorld2, [&](Out& o) { o.u32(world.game_settings().pirate_interval_s); });
    }
    section(out, kIslands, [&](Out& o) { write_islands(o, world); });
    if (tiles == TileMode::Differences) {
        section(out, kTiles, [&](Out& o) { write_tiles(o, world, buildings, before); });
    }
    section(out, kPlayers, [&](Out& o) { write_players(o, world); });
    section(out, kPlayerState, [&](Out& o) { write_player_state(o, world); });
    section(out, kMissions, [&](Out& o) { write_missions(o, world); });
    section(out, kCities, [&](Out& o) { write_cities(o, world); });
    section(out, kCityState, [&](Out& o) { write_city_state(o, world); });
    section(out, kProductions, [&](Out& o) { write_productions(o, world); });
    section(out, kHouses, [&](Out& o) { write_houses(o, world); });
    section(out, kGrowth, [&](Out& o) { write_growth(o, world); });
    section(out, kCosts, [&](Out& o) { write_costs(o, world); });
    if (tiles == TileMode::Differences) {
        section(out, kShips, [&](Out& o) { write_ships(o, world); });  // for readers older than FLEE
    }
    section(out, kAnimals, [&](Out& o) { write_animals(o, world); });
    section(out, kDisasters, [&](Out& o) { write_disasters(o, world); });
    section(out, kVolcanoes, [&](Out& o) { write_volcanoes(o, world); });
    section(out, kFleet, [&](Out& o) { write_fleet(o, world); });
    section(out, kEpochs, [&](Out& o) { write_epochs(o, world); });
    section(out, kArmy, [&](Out& o) { write_army(o, world); });
    section(out, kDiplomacy, [&](Out& o) { write_diplomacy(o, world); });
    section(out, kExplored, [&](Out& o) { write_explored(o, world); });
    section(out, kQueues, [&](Out& o) {
        const auto put = [&](std::uint32_t id, const std::vector<Target>& queue) {
            o.u32(id);
            o.u8(static_cast<std::uint8_t>(queue.size()));
            for (const Target& t : queue) {
                put_target(o, t);
            }
        };
        std::uint32_t ships = 0;
        for (const Ship& s : world.ships()) {
            ships += s.queue.empty() ? 0U : 1U;
        }
        o.u32(ships);
        for (const Ship& s : world.ships()) {
            if (!s.queue.empty()) {
                put(s.id, s.queue);
            }
        }
        std::uint32_t soldiers = 0;
        for (const Soldier& s : world.soldiers()) {
            soldiers += s.queue.empty() ? 0U : 1U;
        }
        o.u32(soldiers);
        for (const Soldier& s : world.soldiers()) {
            if (!s.queue.empty()) {
                put(s.id, s.queue);
            }
        }
    });
    if (tiles == TileMode::Full && world.source() != nullptr) {
        section(out, kOrigin, [&](Out& o) {
            assets::ChunkWriter chunks;
            for (const assets::Chunk& c : world.source()->chunks()) {
                if (c.name != "INSELHAUS") {
                    chunks.add(c.name, world.source()->payload(c));
                }
            }
            o.bytes(chunks.bytes());
        });
    }
    if (simulation != nullptr) {
        section(out, kSimulation, [&](Out& o) { simulation->save_state(o); });
    } else if (!world.simulation_state().empty()) {
        // A world loaded but not yet run keeps the figures it was saved with.
        section(out, kSimulation, [&](Out& o) { o.bytes(world.simulation_state()); });
    }
    return out.take();
}

namespace {

/// Calls `fn(id, body)` for every section of version 1; false if the sections are damaged.
template <class Fn>
bool for_each_section(std::span<const std::uint8_t> data, Fn&& fn) {
    core::ByteReader reader(data);
    while (!reader.at_end()) {
        const auto id = reader.u32();
        const auto section_version = reader.u16();
        const auto reserved = reader.u16();
        const auto size = reader.u32();
        if (!id || !section_version || !reserved || !size) {
            return false;
        }
        const auto body = reader.bytes(*size);
        if (!body) {
            return false;
        }
        if (*section_version != 1) {
            continue;  // a later version of this part: skipped
        }
        if (!fn(*id, *body)) {
            return false;
        }
    }
    return true;
}

}  // namespace

core::Result<World> world_from_sections(std::span<const std::uint8_t> sections, const assets::BuildingTable& buildings,
                                        std::vector<std::string>* warnings) {
    std::optional<core::Result<World>> world;
    bool meta_ok = true;
    const bool ok = for_each_section(sections, [&](std::uint32_t id, std::span<const std::uint8_t> body) {
        core::ByteReader sr(body);
        StateIn in(sr);
        if (id == kMeta) {
            in.u32();
            in.u32();
            in.u64();
            const std::uint64_t fingerprint = in.u64();
            meta_ok = in.ok() && fingerprint == buildings_fingerprint(buildings);
        } else if (id == kMap && !world) {
            world = read_map(in, buildings);
        }
        return true;
    });
    if (!ok) {
        return make_error(ErrorCode::InvalidFormat, "save: damaged section header");
    }
    if (!world) {
        return make_error(ErrorCode::InvalidFormat, "save: no map");
    }
    if (*world && !meta_ok && warnings != nullptr) {
        warnings->push_back("the save was made with other game data (haeuser.cod); buildings are matched by id");
    }
    return std::move(*world);
}

core::Status apply_sections(World& world, const assets::BuildingTable& buildings,
                            std::span<const std::uint8_t> sections) {
    Decoded d;
    core::Bytes origin;
    std::vector<std::pair<std::uint32_t, std::vector<Target>>> ship_queues;
    std::vector<std::pair<std::uint32_t, std::vector<Target>>> soldier_queues;
    bool damaged = false;
    const bool ok = for_each_section(sections, [&](std::uint32_t id, std::span<const std::uint8_t> body) {
        core::ByteReader sr(body);
        StateIn in(sr);
        switch (id) {
            case kRules:
                d.rules = get_rules(in);
                break;
            case kWorld:
                d.world = read_world(in);
                break;
            case kWorld2:
                d.pirate_interval_s = in.u32();
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
            case kPlayerState:
                d.player_state = read_player_state(in);
                break;
            case kMissions:
                d.missions = read_missions(in, world.islands().size());
                break;
            case kCities:
                d.cities = read_cities(in);
                break;
            case kCityState:
                d.city_state = read_city_state(in);
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
            case kFleet:
                d.fleet = read_fleet(in);
                break;
            case kEpochs:
                d.epochs = read_epochs(in);
                break;
            case kSoldiers:
                d.soldiers = read_soldiers(in);
                break;
            case kArmy:
                d.army = read_army(in);
                break;
            case kDiplomacy:
                d.diplomacy = read_diplomacy(in);
                break;
            case kExplored:
                d.explored = read_explored(in);
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
                d.simulation = std::vector<std::uint8_t>(body.begin(), body.end());
                break;
            case kOrigin:
                origin.assign(body.begin(), body.end());
                break;
            case kQueues:
                for (auto* list : {&ship_queues, &soldier_queues}) {
                    const std::uint32_t n = in.u32();
                    for (std::uint32_t k = 0; k < n && in.ok(); ++k) {
                        const std::uint32_t unit = in.u32();
                        const std::uint8_t count = in.u8();
                        std::vector<Target> queue;
                        for (std::uint8_t q = 0; q < count && in.ok(); ++q) {
                            queue.push_back(get_target(in));
                        }
                        if (queue.size() > kMaxQueuedOrders) {
                            queue.resize(kMaxQueuedOrders);
                        }
                        list->emplace_back(unit, std::move(queue));
                    }
                }
                break;
            default:
                return true;  // META, MAPW (read before), or a part of a later version
        }
        damaged = damaged || !in.ok();
        return !damaged;
    });
    if (!ok || damaged) {
        return make_error(ErrorCode::InvalidFormat,
                          damaged ? "O1602: damaged section" : "O1602: damaged section header");
    }
    if (auto status = apply_decoded(world, d); !status) {
        return status;
    }
    for (auto& [id, queue] : ship_queues) {
        if (Ship* ship = world.ship(id)) {
            ship->queue = std::move(queue);
        }
    }
    for (auto& [id, queue] : soldier_queues) {
        if (Soldier* soldier = world.soldier(id)) {
            soldier->queue = std::move(queue);
        }
    }
    if (!origin.empty() && world.source() == nullptr) {
        if (auto file = assets::ChunkFile::from_bytes(std::move(origin))) {
            world.set_source(std::make_shared<const assets::ChunkFile>(std::move(file).value()));
        }
    }
    return core::Ok{};
}

}  // namespace detail

core::Bytes encode_extension(const World& world, const assets::BuildingTable& buildings, const Simulation* simulation,
                             std::span<const std::uint8_t> before) {
    Out out;
    out.bytes(kMagic);
    out.u16(kFormatVersion);
    out.u16(0);
    out.u64(savegame_fingerprint(before));
    out.bytes(detail::encode_sections(world, buildings, simulation, detail::TileMode::Differences, before));
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
    return detail::apply_sections(world, buildings, payload.subspan(payload.size() - reader.remaining()));
}

}  // namespace o1602::sim
