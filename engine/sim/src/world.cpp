// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/world.hpp"

#include <algorithm>
#include <map>

#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/savegame.hpp"
#include "o1602/sim/growth.hpp"

namespace o1602::sim {

using core::ErrorCode;
using core::make_error;

World::World(std::uint32_t width, std::uint32_t height)
    : m_width(width), m_height(height), m_cells(std::size_t{width} * height) {}

core::Status World::add_island(WorldIsland island) {
    const std::uint32_t w = island.map.width();
    const std::uint32_t h = island.map.height();
    if (island.x + w > m_width || island.y + h > m_height || w > kSea || h > kSea) {
        return make_error(ErrorCode::OutOfRange,
                          "island " + std::to_string(island.header.number) + " leaves the world map");
    }
    if (m_islands.size() >= kSea) {  // named limit instead of the original's 255 (L-003)
        return make_error(ErrorCode::OutOfRange, "too many islands (65535)");
    }
    for (std::uint32_t ly = 0; ly < h; ++ly) {
        for (std::uint32_t lx = 0; lx < w; ++lx) {
            if (m_cells[std::size_t{island.y + ly} * m_width + island.x + lx].island != kSea) {
                return make_error(ErrorCode::InvalidFormat,
                                  "island " + std::to_string(island.header.number) + " overlaps another island");
            }
        }
    }
    const auto index = static_cast<std::uint16_t>(m_islands.size());
    for (std::uint32_t ly = 0; ly < h; ++ly) {
        for (std::uint32_t lx = 0; lx < w; ++lx) {
            m_cells[std::size_t{island.y + ly} * m_width + island.x + lx] =
                Cell{index, static_cast<std::uint16_t>(lx), static_cast<std::uint16_t>(ly)};
        }
    }
    m_islands.push_back(std::move(island));
    return core::Ok{};
}

City* World::city(std::uint16_t island, std::uint8_t index) noexcept {
    for (City& c : m_cities) {
        if (c.island == island && c.index == index) {
            return &c;
        }
    }
    return nullptr;
}

const City* World::city(std::uint16_t island, std::uint8_t index) const noexcept {
    for (const City& c : m_cities) {
        if (c.island == island && c.index == index) {
            return &c;
        }
    }
    return nullptr;
}

namespace {

constexpr std::uint64_t production_key(std::uint16_t island, std::uint16_t x, std::uint16_t y) noexcept {
    return (std::uint64_t{island} << 32) | (std::uint64_t{y} << 16) | x;
}

}  // namespace

const Production* World::production(std::uint16_t island, std::uint16_t x, std::uint16_t y) const {
    const auto it = m_production_index.find(production_key(island, x, y));
    return it == m_production_index.end() ? nullptr : &m_productions[it->second];
}

std::optional<std::uint32_t> World::production_index(std::uint16_t island, std::uint16_t x, std::uint16_t y) const {
    const auto it = m_production_index.find(production_key(island, x, y));
    return it == m_production_index.end() ? std::nullopt : std::optional<std::uint32_t>(it->second);
}

void World::set_production(const Production& p) {
    const auto [it, inserted] = m_production_index.try_emplace(production_key(p.island, p.x, p.y),
                                                               static_cast<std::uint32_t>(m_productions.size()));
    if (inserted) {
        m_productions.push_back(p);
    } else {
        m_productions[it->second] = p;
    }
}

std::optional<std::uint16_t> World::island_index(std::uint32_t number) const noexcept {
    for (std::size_t i = 0; i < m_islands.size(); ++i) {
        if (m_islands[i].header.number == number) {
            return static_cast<std::uint16_t>(i);
        }
    }
    return std::nullopt;
}

PlayerId first_human_player(const World& world) noexcept {
    const auto& players = world.players();
    for (std::size_t i = 0; i < players.size() && i < kNoPlayer; ++i) {
        if (players[i].role == PlayerRole::Human) {
            return static_cast<PlayerId>(i);
        }
    }
    return kNoPlayer;
}

core::Result<World> load_world(const assets::ChunkFile& file, const assets::BuildingTable& buildings,
                               const assets::GameDirectory& game, std::vector<std::string>* warnings) {
    auto islands = assets::load_islands(file);
    if (!islands) {
        return islands.error();
    }
    const auto warn = [&](std::string message) {
        if (warnings != nullptr) {
            warnings->push_back(std::move(message));
        }
    };

    std::map<std::string, std::vector<assets::Island>> templates;  // cache: many islands share one
    World world(World::kOriginalWidth, World::kOriginalHeight);    // the original map size
    for (const auto& island : *islands) {
        const assets::Island* base = nullptr;
        if (const std::string path = island.header.template_path(); !path.empty()) {
            auto it = templates.find(path);
            if (it == templates.end()) {
                std::vector<assets::Island> loaded;
                if (const auto resolved = game.resolve(path)) {
                    if (auto tfile = assets::ChunkFile::load(*resolved)) {
                        if (auto tislands = assets::load_islands(*tfile)) {
                            loaded = std::move(tislands).value();
                        }
                    }
                }
                if (loaded.empty()) {
                    warn("island template not found or invalid: " + path);
                }
                it = templates.emplace(path, std::move(loaded)).first;
            }
            if (!it->second.empty()) {
                base = &it->second.front();
            }
        }
        WorldIsland wi{island.header, IslandMap::from_island(island, buildings, warnings, base), island.header.pos_x,
                       island.header.pos_y};
        if (auto status = world.add_island(std::move(wi)); !status) {
            warn(status.error().message);
        }
    }

    auto players = assets::load_players(file);
    if (!players) {
        return players.error();
    }
    std::vector<Player> slots;
    for (const auto& p : *players) {
        slots.push_back(Player{role_from_original(p.kind), banner_from_original(p.color), p.money, p.name});
    }
    world.set_players(std::move(slots));

    auto ships = assets::load_ships(file);
    if (!ships) {
        return ships.error();
    }
    for (const auto& s : *ships) {
        if (!world.contains(s.x, s.y)) {
            warn("ship '" + s.name + "' outside the world map");
            continue;
        }
        const auto direction = static_cast<std::uint8_t>(s.direction & 7U);
        world.add_ship(Ship{s.name, s.x, s.y, s.figure, player_from_original(s.owner), direction});
    }
    auto state = assets::load_game_state(file);
    if (!state) {
        return state.error();
    }
    for (const auto& c : state->cities) {
        // STADT4 names the island by its number; find its index in the world.
        const auto island = world.island_index(c.island);
        if (!island) {
            warn("city '" + c.name + "' on unknown island " + std::to_string(c.island));
            continue;
        }
        City city;
        city.island = *island;
        city.index = c.index;
        city.owner = player_from_original(c.owner);
        city.name = c.name;
        city.inhabitants.assign(c.inhabitants.begin(), c.inhabitants.end());
        city.satisfaction.assign(c.satisfaction.begin(), c.satisfaction.end());
        // A tax rate of 0 is not possible (the original loads it as 128).
        for (const std::uint8_t t : c.tax) {
            city.tax_rate.push_back(t == 0 ? std::uint8_t{128} : t);
        }
        city.food_ratio = c.food_ratio;
        world.add_city(std::move(city));
    }
    for (const auto& w : state->warehouses) {
        const auto island = world.island_index(w.island);
        City* city = island ? world.city(*island, w.city) : nullptr;
        if (city == nullptr) {
            warn("warehouse of unknown city " + std::to_string(w.island) + "/" + std::to_string(w.city));
            continue;
        }
        city->warehouse_x = w.x;
        city->warehouse_y = w.y;
        for (const auto& slot : w.wares) {
            city->ensure_ware(slot.ware);
            city->goods[slot.ware] = slot.stock();
        }
    }
    for (const auto& m : state->consumption) {
        const auto island = world.island_index(m.island);
        City* city = island ? world.city(*island, m.city) : nullptr;
        if (city == nullptr) {
            warn("consumption of unknown city " + std::to_string(m.island) + "/" + std::to_string(m.city));
            continue;
        }
        for (const auto& g : m.goods) {
            const assets::BuildingType* producer = buildings.by_id(g.producer_id);
            if (producer == nullptr || producer->ware == 0) {
                continue;
            }
            city->ensure_ware(producer->ware);
            if (city->supply.size() < city->goods.size()) {
                city->supply.resize(city->goods.size());
            }
            City::Supply& s = city->supply[producer->ware];
            s.demand = g.demand;
            s.supplied = g.supplied;
            s.ratio = g.ratio;
        }
    }
    for (const auto& p : state->production) {
        const auto island = world.island_index(p.island);
        if (!island || p.x >= world.islands()[*island].map.width() || p.y >= world.islands()[*island].map.height()) {
            warn("production building outside its island " + std::to_string(p.island));
            continue;
        }
        Production q;
        q.island = *island;
        q.x = p.x;
        q.y = p.y;
        q.enabled = p.enabled;
        q.rate = p.rate;
        q.anim_phase = p.anim_phase;
        q.idle_cycles = p.idle_cycles;
        q.flags = p.flags;
        q.countdown = p.countdown;
        q.raw = p.raw;
        q.raw2 = p.raw2;
        q.stock = p.stock;
        q.produced = p.produced;
        q.elapsed = p.elapsed;
        world.set_production(q);
    }
    for (const auto& r : state->soldiers) {
        Soldier soldier{r.x2 / 2.0, r.y2 / 2.0, r.figure, player_from_original(r.owner), r.direction, r.anim};
        if (!world.contains(static_cast<std::uint32_t>(soldier.x), static_cast<std::uint32_t>(soldier.y))) {
            warn("soldier outside the world map");
            continue;
        }
        if (const auto variant = assets::FigureTable::unit_variant(r.figure)) {
            soldier.figure = static_cast<std::uint16_t>(variant->family);
        }
        world.add_soldier(soldier);
    }
    // HIRSCH2 stores only position and timer; the kind follows the island's climate: deer
    // (HIRSCH) in the north, antelopes (ANTILOPE) in the south (confirmed in the running game).
    constexpr std::uint16_t kFigureHirsch = 89;
    constexpr std::uint16_t kFigureAntilope = 91;
    for (const auto& a : state->animals) {
        const auto island = world.island_index(a.island);
        if (!island || a.x >= world.islands()[*island].map.width() || a.y >= world.islands()[*island].map.height()) {
            warn("animal outside its island " + std::to_string(a.island));
            continue;
        }
        const bool south = (world.islands()[*island].header.climate & 1U) != 0;
        world.add_animal(Animal{*island, a.x, a.y, south ? kFigureAntilope : kFigureHirsch});
    }
    // Growth: the clocks of TIMERS and the entries of ROHWACHS2; growing tiles the file lists no
    // entry for get one (add_missing_growth).
    GrowthState& growth = world.growth();
    if (state->growth_clocks) {
        growth.counter = state->growth_clocks->counter;
        for (std::size_t c = 0; c < GrowthState::kClasses; ++c) {
            growth.time_ms[c] = state->growth_clocks->time_ms[c] % growth_period_ms(static_cast<std::uint32_t>(c));
        }
    }
    for (const auto& r : state->growth) {
        const auto island = world.island_index(r.island);
        if (!island || !world.islands()[*island].map.contains(r.x, r.y)) {
            warn("growing tile outside its island " + std::to_string(r.island));
            continue;
        }
        GrowthEntry e;
        e.island = *island;
        e.x = r.x;
        e.y = r.y;
        e.growth_class = static_cast<std::uint8_t>(std::min<std::uint8_t>(r.growth_class, 31));
        e.seen = static_cast<std::uint8_t>(r.seen & 7U);
        e.stage = r.stage;
        growth.entries.push_back(e);
    }
    add_missing_growth(world, buildings);
    return world;
}

}  // namespace o1602::sim
