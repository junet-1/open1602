// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/world.hpp"

#include <algorithm>
#include <map>

#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/savegame.hpp"
#include "o1602/sim/growth.hpp"
#include "o1602/sim/savegame_extension.hpp"
#include "o1602/sim/volcanoes.hpp"

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

const House* World::house(std::uint16_t island, std::uint16_t x, std::uint16_t y) const {
    const auto it = m_house_index.find(production_key(island, x, y));
    return it == m_house_index.end() ? nullptr : &m_houses[it->second];
}

House* World::house(std::uint16_t island, std::uint16_t x, std::uint16_t y) {
    const auto it = m_house_index.find(production_key(island, x, y));
    return it == m_house_index.end() ? nullptr : &m_houses[it->second];
}

void World::add_house(const House& house) {
    const auto [it, inserted] = m_house_index.try_emplace(production_key(house.island, house.x, house.y),
                                                          static_cast<std::uint32_t>(m_houses.size()));
    if (inserted) {
        m_houses.push_back(house);
    } else {
        m_houses[it->second] = house;
    }
}

void World::remove_house(std::uint16_t island, std::uint16_t x, std::uint16_t y) {
    const auto it = m_house_index.find(production_key(island, x, y));
    if (it == m_house_index.end()) {
        return;
    }
    const std::uint32_t index = it->second;
    m_house_index.erase(it);
    if (index + 1 != m_houses.size()) {
        m_houses[index] = m_houses.back();
        const House& moved = m_houses[index];
        m_house_index[production_key(moved.island, moved.x, moved.y)] = index;
    }
    m_houses.pop_back();
}

void World::set_houses(std::vector<House> houses) {
    m_houses = std::move(houses);
    m_house_index.clear();
    for (std::uint32_t i = 0; i < m_houses.size(); ++i) {
        m_house_index[production_key(m_houses[i].island, m_houses[i].x, m_houses[i].y)] = i;
    }
}

void World::set_productions(std::vector<Production> productions, std::vector<std::uint32_t> free) {
    m_productions = std::move(productions);
    m_free_productions = std::move(free);
    m_production_index.clear();
    for (std::uint32_t i = 0; i < m_productions.size(); ++i) {
        if (m_productions[i].used) {
            m_production_index[production_key(m_productions[i].island, m_productions[i].x, m_productions[i].y)] = i;
        }
    }
}

std::optional<std::uint32_t> World::production_index(std::uint16_t island, std::uint16_t x, std::uint16_t y) const {
    const auto it = m_production_index.find(production_key(island, x, y));
    return it == m_production_index.end() ? std::nullopt : std::optional<std::uint32_t>(it->second);
}

void World::set_production(const Production& p) {
    const auto key = production_key(p.island, p.x, p.y);
    if (const auto it = m_production_index.find(key); it != m_production_index.end()) {
        m_productions[it->second] = p;
        m_productions[it->second].used = true;
        return;
    }
    std::uint32_t index = static_cast<std::uint32_t>(m_productions.size());
    if (!m_free_productions.empty()) {
        index = m_free_productions.back();
        m_free_productions.pop_back();
        m_productions[index] = p;
    } else {
        m_productions.push_back(p);
    }
    m_productions[index].used = true;
    m_production_index.emplace(key, index);
}

std::optional<std::uint32_t> World::remove_production(std::uint16_t island, std::uint16_t x, std::uint16_t y) {
    const auto it = m_production_index.find(production_key(island, x, y));
    if (it == m_production_index.end()) {
        return std::nullopt;
    }
    const std::uint32_t index = it->second;
    m_production_index.erase(it);
    Production free;
    free.island = island;
    free.x = x;
    free.y = y;
    free.used = false;
    free.enabled = false;
    m_productions[index] = free;
    m_free_productions.push_back(index);
    return index;
}

void World::remove_city(std::uint16_t island, std::uint8_t index) {
    std::erase_if(m_cities, [&](const City& c) { return c.island == island && c.index == index; });
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
                               const assets::GameDirectory& game, std::vector<std::string>* warnings,
                               const WorldStart& start) {
    if (plans_islands(file)) {
        // The SZENE handler of the original makes the islands while the scenario loads (0x0048C5C0).
        auto generated = generate_islands(file, game, buildings, start, warnings);
        if (!generated) {
            return generated.error();
        }
        return load_world(generated->file, buildings, game, warnings, start);
    }
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
        Player player{role_from_original(p.kind), banner_from_original(p.color), p.money, p.name};
        player.infra_mask = p.infra_mask;
        player.castles_built = p.castles_built;
        player.cathedrals_built = p.cathedrals_built;
        player.arches_allowed = p.arches_allowed;
        player.arches_built = p.arches_built;
        player.monuments_allowed = p.monuments_allowed;
        player.monuments_built = p.monuments_built;
        slots.push_back(std::move(player));
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
        Ship ship{s.name, s.x, s.y, s.figure, player_from_original(s.owner), direction, {}};
        for (const auto& c : s.cargo) {
            // Cargo names the good by its producer, like the warehouse chunk KONTOR2.
            const assets::BuildingType* producer = c.producer != 0 ? buildings.by_id(c.producer) : nullptr;
            if (producer != nullptr && producer->ware != 0 && c.amount != 0) {
                ship.cargo.push_back(Ship::Cargo{producer->ware, c.amount});
            }
        }
        world.add_ship(std::move(ship));
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
        city.material_lock = c.material_lock;
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
    // Houses: only on a dwelling (HAUS_PRODTYP Kind WOHNUNG); the class is the building's, as in
    // the loader of the original (0x00483EE0).
    constexpr std::int32_t kProdKindHouse = 13;
    for (const auto& r : state->houses) {
        const auto island = world.island_index(r.island);
        if (!island || !world.islands()[*island].map.contains(r.x, r.y)) {
            warn("house outside its island " + std::to_string(r.island));
            continue;
        }
        const MapCell& cell = world.islands()[*island].map.top(r.x, r.y);
        const assets::BuildingType* b = cell.empty() ? nullptr : buildings.by_sprite(cell.sprite);
        if (b == nullptr || b->prod_kind != kProdKindHouse) {
            continue;
        }
        House h;
        h.island = *island;
        h.x = r.x;
        h.y = r.y;
        h.city = cell.city;
        h.resident_class = b->resident_class;
        h.inhabitants64 = r.inhabitants64;
        h.timer_group = r.timer_group;
        h.epoch = r.epoch;
        h.queue_epoch = r.queue_epoch;
        h.market_distance = r.market_distance;
        h.market = r.market;
        h.services_complete = r.services_complete;
        h.services = r.services;
        world.add_house(h);
    }
    if (state->game_time) {
        world.set_time_ms(std::uint64_t{*state->game_time} * 100);
    }
    if (state->city_name_index) {
        world.set_city_name_index(*state->city_name_index);
    }
    for (std::size_t i = 0; i < world.islands().size(); ++i) {
        WorldIsland& island = world.island(i);
        island.deer_phase = island.header.deer_phase;
        island.drought.strength = island.header.drought_strength;
        island.drought.end_ms = std::uint64_t{island.header.drought_end} * 100;
        island.drought.curse_pending = island.header.curse_pending;
        // Craters: the list of the savegame, searched only for islands without one (INSEL3/4).
        constexpr std::size_t kCraterFields = 0x4C;
        if (island.header.raw.size() >= kCraterFields) {
            for (const auto& c : island.header.craters) {
                island.craters.push_back(Crater{c.x, c.y, c.steps_left});
            }
        } else {
            island.craters = find_craters(island.map, buildings);
        }
        island.volcano_erupted = island.header.volcano_erupted;
    }
    // Volcanoes. The crater count is not taken from TIMERS +0xD0 but follows the islands (a
    // scenario may carry a stale one, S-063).
    VolcanoState& volcanoes = world.volcanoes();
    if (state->next_volcano) {
        volcanoes.next_ms = std::uint64_t{*state->next_volcano} * 100;
    }
    const auto crater_ref = [&](const assets::GameState::CraterRef& r) -> std::optional<CraterRef> {
        const auto island = world.island_index(r.island);
        if (!island) {
            return std::nullopt;
        }
        return CraterRef{*island, r.x, r.y};
    };
    if (state->last_volcano) {
        volcanoes.last = crater_ref(*state->last_volcano);
    }
    if (state->running_volcano) {
        // The original keeps no lava in its savegames and starts a running eruption anew, with a
        // new pattern and without the message (0x0048C0F0, S-062).
        if (const auto ref = crater_ref(*state->running_volcano)) {
            const auto seed = static_cast<std::uint16_t>(world.random(RandomStream::Islands).below(0x8000));
            if (!erupt(world, *ref, seed, false, nullptr)) {
                warn("running eruption on an island without that crater");
            }
        }
    }
    if (state->difficulty) {
        world.set_game_settings(World::GameSettings{*state->difficulty, state->settings.value_or(0)});
    }
    if (state->next_drought) {
        // A stored 0 becomes 60 minutes, as in the original's loader (0x00484E3C).
        const std::uint32_t next = *state->next_drought != 0 ? *state->next_drought : 36'000U;
        world.set_next_drought_ms(std::uint64_t{next} * 100);
    }
    if (state->house_clocks) {
        HouseClocks& clocks = world.house_clocks();
        for (std::size_t g = 0; g < HouseClocks::kGroups; ++g) {
            clocks.epoch[g] = state->house_clocks->epoch[g];
            clocks.time_ms[g] = state->house_clocks->time_ms[g] % (15'000U + 64U * static_cast<std::uint32_t>(g));
        }
    }
    const auto add_costs = [&](const std::vector<assets::CostRecord>& records, CostBuilding::Kind kind) {
        for (const auto& r : records) {
            const auto island = world.island_index(r.island);
            if (!island || !world.islands()[*island].map.contains(r.x, r.y)) {
                continue;
            }
            world.add_cost_building(CostBuilding{kind, *island, r.x, r.y, r.enabled});
        }
    };
    add_costs(state->shipyards, CostBuilding::Kind::Shipyard);
    add_costs(state->castles, CostBuilding::Kind::Castle);
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
        for (DeerSpot& spot : world.island(*island).deer_spots) {
            if (!spot.used) {
                spot = DeerSpot{true, a.x, a.y, std::uint64_t{a.timer} * 100};
                break;
            }
        }
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
    // Open1602's own chunk: the state the original's chunks cannot hold.
    for (const assets::Chunk& c : file.chunks()) {
        if (c.name == kExtensionChunk) {
            constexpr std::size_t kHeader = 20;
            const auto before = file.data().subspan(0, c.offset - kHeader);
            if (auto status = apply_extension(world, buildings, file.payload(c), before); !status) {
                warn(status.error().message + " (loaded without it)");
            }
            break;
        }
    }
    world.set_source(std::make_shared<const assets::ChunkFile>(file));
    return world;
}

}  // namespace o1602::sim
