// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/simulation.hpp"

#include <algorithm>

#include "o1602/sim/growth.hpp"

namespace o1602::sim {

namespace {

constexpr std::uint32_t kProductionPeriodMs = 1000;  // 0x0047DAF0: once per game second
constexpr std::size_t kMaxPendingEvents = 8192;      // events nobody takes are dropped, oldest first
constexpr std::uint32_t kUpkeepPeriodMs = 5000;      // 0x00476350: each player once per 5 s
constexpr std::uint32_t kDiplomacyTickMs = 5000;     // 0x00476350: the diplomacy counters fade

}  // namespace

Simulation::Simulation(World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures,
                       Rules rules)
    : m_world(&world),
      m_buildings(&buildings),
      m_rules(rules),
      m_figures(&figures),
      m_logistics(world, buildings, figures),
      m_fleet(world, buildings, figures, m_rules),
      m_combat(world, buildings, figures),
      m_military(world, buildings, figures, m_rules, m_combat),
      m_ai(*this) {
    m_military.set_fleet(&m_fleet);
    m_fleet.set_combat(&m_combat);
    m_fleet.set_directed([this](std::uint32_t ship) { return m_ai.directs(ship); });
    m_time_ms = world.time_ms();
    m_island_ms = static_cast<std::uint32_t>(m_time_ms % Logistics::kIslandEpochMs);
    // The cities consume at whole multiples of 10 s of game time (game08 -> game09, the video of
    // game04); the extra tick the original runs right after loading is not taken over (S-091).
    m_city_ms = static_cast<std::uint32_t>(m_time_ms % kCityTickMs);
    if (world.saved_rules()) {
        m_rules = *world.saved_rules();  // the settings the game was started with
    }
    recount_disasters(world);
    if (restore_state(world.take_simulation_state())) {
        return;  // a game saved by Open1602 continues where it was
    }
    // As the original after every load: the computer players look at the world anew (0x0042B710).
    m_ai.reset();
    // As when the original loads a game: every deer spot gets its deer.
    m_logistics.spawn_deer_at_spots();
    // The original takes the saved coverage of the houses; Open1602 recomputes it once, which
    // also restores what the savegame does not keep (gallows, fountains).
    update_services(world, buildings);
}

void Simulation::save_state(core::ByteWriter& out) const {
    constexpr std::uint16_t kVersion = 4;
    out.u16(kVersion);
    out.u32(m_production_ms);
    out.u32(m_city_ms);
    out.u32(m_disaster_ms);
    out.u32(m_upkeep_ms);
    out.u32(m_island_ms);
    m_logistics.save(out);
    m_fleet.save(out);
    m_military.save(out);
    m_ai.save(out);
}

bool Simulation::restore_state(const std::vector<std::uint8_t>& state) {
    if (state.empty()) {
        return false;
    }
    core::ByteReader reader(state);
    StateIn in(reader);
    const std::uint16_t version = in.u16();
    if (version < 1 || version > 4) {
        return false;
    }
    const std::uint32_t production = in.u32();
    const std::uint32_t city = in.u32();
    const std::uint32_t disaster = in.u32();
    const std::uint32_t upkeep = in.u32();
    const std::uint32_t island = in.u32();
    if (!in.ok() || !m_logistics.load(reader, version)) {
        return false;
    }
    if (version >= 2 && !m_fleet.load(reader)) {
        return false;
    }
    if (version >= 3 && !m_military.load(reader)) {
        return false;
    }
    if (version >= 4) {
        if (!m_ai.load(reader)) {
            return false;
        }
    } else {
        m_ai.reset();  // games of earlier versions had no computer players' state
    }
    m_production_ms = production;
    m_city_ms = city;
    m_disaster_ms = disaster;
    m_upkeep_ms = upkeep;
    m_island_ms = island;
    return true;
}

void Simulation::set_speed(std::uint32_t speed) noexcept {
    m_speed = std::min(speed, m_rules.max_game_speed);
}

void Simulation::advance(std::uint32_t real_ms) {
    if (m_speed == 0) {
        apply_commands();
        return;
    }
    std::uint64_t game_ms = std::uint64_t{real_ms} * m_speed;
    if (game_ms > kMaxCatchUpMs) {
        game_ms = kTickMs;
    }
    m_pending_ms += static_cast<std::uint32_t>(game_ms);
    while (m_pending_ms >= kTickMs) {
        m_pending_ms -= kTickMs;
        tick();
    }
}

std::uint64_t Simulation::submit(Command command) {
    const std::uint64_t sequence = m_next_command++;
    m_commands.emplace_back(sequence, std::move(command));
    return sequence;
}

void Simulation::apply_commands() {
    while (!m_commands.empty()) {
        auto [sequence, command] = std::move(m_commands.front());
        m_commands.pop_front();
        CommandResult result = execute(command);
        result.sequence = sequence;
        m_command_results.push_back(std::move(result));
    }
    if (m_construction_events.size() > kMaxPendingEvents) {
        m_construction_events.erase(m_construction_events.begin(),
                                    m_construction_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
    }
    if (m_command_results.size() > kMaxPendingEvents) {
        m_command_results.erase(m_command_results.begin(),
                                m_command_results.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
    }
}

CommandResult Simulation::execute(const Command& command) {
    CommandResult result;
    const std::size_t first = m_construction_events.size();
    if (const auto* build = std::get_if<BuildOrder>(&command)) {
        result.build = execute_build(*m_world, *m_buildings, *build, m_city_names, &m_construction_events);
    } else if (const auto* demolish = std::get_if<DemolishOrder>(&command)) {
        const DemolishResult d =
            execute_demolish(*m_world, *m_buildings, *demolish, &m_construction_events, m_rules.refund_share());
        result.demolished = d.removed;
        result.refund = d.refund;
    } else if (const auto* order = std::get_if<ShipOrder>(&command)) {
        result.accepted = m_fleet.order(*order);
    } else if (const auto* transfer = std::get_if<CargoTransfer>(&command)) {
        result.moved = m_fleet.transfer(*transfer);
        result.accepted = result.moved != 0;
    } else if (const auto* route = std::get_if<RouteEdit>(&command)) {
        result.accepted = m_fleet.edit_route(*route);
    } else if (const auto* trade = std::get_if<TradeSetting>(&command)) {
        result.accepted = m_fleet.set_trade(*trade);
    } else if (const auto* yard = std::get_if<ShipyardOrder>(&command)) {
        result.accepted = m_fleet.shipyard(*yard);
    } else if (const auto* explore = std::get_if<ExploreOrder>(&command)) {
        result.accepted = m_fleet.explore(*explore);
    } else if (const auto* dipl = std::get_if<DiplomacyOrder>(&command)) {
        result.accepted = apply_diplomacy(*m_world, *dipl);
    } else if (const auto* soldiers = std::get_if<SoldierOrder>(&command)) {
        result.accepted = m_military.order(*soldiers);
    } else if (const auto* soldier = std::get_if<SoldierCommand>(&command)) {
        result.accepted = m_military.command(*soldier);
    } else if (const auto* castle = std::get_if<CastleOrder>(&command)) {
        result.accepted = m_military.castle(*castle);
    } else if (const auto* unload = std::get_if<UnloadOrder>(&command)) {
        result.accepted = m_military.unload(*unload);
    } else if (const auto* sw = std::get_if<ProductionSwitch>(&command)) {
        result.accepted = switch_production(*sw);
    } else if (const auto* city = std::get_if<CityOrder>(&command)) {
        result.accepted = city_order(*city);
    } else if (const auto* ship = std::get_if<ShipCommand>(&command)) {
        result.accepted = m_fleet.command(*ship);
    }
    handle(m_construction_events, first);
    return result;
}

bool Simulation::switch_production(const ProductionSwitch& s) {
    if (s.island >= m_world->islands().size()) {
        return false;
    }
    const IslandMap& map = m_world->islands()[s.island].map;
    if (!map.contains(s.x, s.y) || (s.player != kNoPlayer && map.top(s.x, s.y).owner != s.player)) {
        return false;
    }
    const auto index = m_world->production_index(s.island, s.x, s.y);
    if (!index) {
        return false;
    }
    Production& p = m_world->productions()[*index];
    if (!p.used || p.enabled == s.on) {
        return false;
    }
    p.enabled = s.on;
    return true;
}

bool Simulation::city_order(const CityOrder& o) {
    City* city = m_world->city(o.island, o.city);
    if (city == nullptr || (o.player != kNoPlayer && city->owner != o.player)) {
        return false;
    }
    switch (o.op) {
        case CityOrder::Op::TaxRate: {
            // 0x0047F130 posts the rate, 0x0047F850 measures the class against it at once.
            const auto& classes = m_buildings->resident_classes();
            if (o.resident_class >= classes.size() || o.resident_class >= city->tax_rate.size() ||
                o.rate < CityOrder::kMinTax || o.rate > CityOrder::kMaxTax) {
                return false;
            }
            city->tax_rate[o.resident_class] = o.rate;
            if (o.resident_class < city->satisfaction.size()) {
                city->satisfaction[o.resident_class] = satisfaction_of(*city, classes[o.resident_class], o.rate);
            }
            return true;
        }
        case CityOrder::Op::Rename:
            if (o.name.empty() || o.name.size() > CityOrder::kMaxNameLength) {
                return false;
            }
            city->name = o.name;
            return true;
        case CityOrder::Op::MaterialLock:
            city->material_lock = o.on;
            return true;
    }
    return false;
}

void Simulation::handle(const std::vector<ConstructionEvent>& events, std::size_t first) {
    bool storage = false;
    for (std::size_t i = first; i < events.size(); ++i) {
        const ConstructionEvent& e = events[i];
        switch (e.kind) {
            case ConstructionEvent::Kind::Removed:
                if (e.production) {
                    // S-002: figures of a building that is gone end their trip; what they carry
                    // is lost and what they reserved is released.
                    m_logistics.building_removed(*e.production);
                }
                break;
            case ConstructionEvent::Kind::CityFounded:
                m_ai.city_founded(e.island, e.city);
                storage = true;
                break;
            case ConstructionEvent::Kind::StorageChanged:
            case ConstructionEvent::Kind::CityTaken:
            case ConstructionEvent::Kind::CityDissolved:
                storage = true;
                break;
            case ConstructionEvent::Kind::Placed:
                break;
        }
        if (e.kind == ConstructionEvent::Kind::Placed || e.kind == ConstructionEvent::Kind::Removed) {
            m_fleet.island_changed(e.island);
            m_combat.island_changed(e.island);
        }
        m_military.update_towers(e.island);
    }
    if (storage) {
        m_logistics.update_storage();
        // A smaller warehouse loses what does not fit (0x00482120).
        for (std::size_t i = first; i < events.size(); ++i) {
            if (events[i].kind != ConstructionEvent::Kind::StorageChanged) {
                continue;
            }
            if (City* c = m_world->city(events[i].island, events[i].city)) {
                for (std::uint32_t& g : c->goods) {
                    g = std::min(g, c->capacity);
                }
            }
        }
    }
}

std::vector<MilitaryEvent> Simulation::take_military_events() {
    std::vector<MilitaryEvent> events;
    events.swap(m_military_events);
    return events;
}

std::vector<CommandResult> Simulation::take_command_results() {
    std::vector<CommandResult> results;
    results.swap(m_command_results);
    return results;
}

std::vector<ConstructionEvent> Simulation::take_construction_events() {
    std::vector<ConstructionEvent> events;
    events.swap(m_construction_events);
    return events;
}

void Simulation::tick() {
    apply_commands();
    m_time_ms += kTickMs;
    m_world->set_time_ms(m_time_ms);
    // Every 5 s the diplomacy counters fade (Player_Tick 0x00476350; the original takes one
    // player per call, each once in 5 s).
    if (m_time_ms % kDiplomacyTickMs == 0) {
        for (std::size_t p = 0; p < m_world->players().size(); ++p) {
            m_world->diplomacy().decay(static_cast<PlayerId>(p));
        }
        pirates_reconsider(*m_world);
    }
    // Growth first, as in the original (0x0047CA80 is the first subsystem of 0x00489670).
    advance_growth(*m_world, *m_buildings, kTickMs);
    m_production_ms += kTickMs;
    if (m_production_ms >= kProductionPeriodMs) {
        m_production_ms -= kProductionPeriodMs;
        const std::size_t first = m_production_events.size();
        advance_production(*m_world, *m_buildings, &m_production_events);
        for (std::size_t i = first; i < m_production_events.size(); ++i) {
            m_logistics.handle(m_production_events[i]);
        }
        if (m_production_events.size() > kMaxPendingEvents) {
            m_production_events.erase(m_production_events.begin(),
                                      m_production_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    // The cities consume every 10 s (0x0047F8A0 comes after the production in the original).
    m_city_ms += kTickMs;
    if (m_city_ms >= kCityTickMs) {
        m_city_ms -= kCityTickMs;
        advance_cities(*m_world, *m_buildings, &m_city_events);
        m_military.natives_tick();
        if (m_city_events.size() > kMaxPendingEvents) {
            m_city_events.erase(m_city_events.begin(),
                                m_city_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    // Every 60-67 s a city may catch the plague, send out a robber or catch fire (0x0047F8A0).
    advance_city_events(*m_world, *m_buildings, &m_disaster_events, &m_robbers);
    for (const RobberSpawn& r : m_robbers) {
        m_logistics.spawn_robber(r);
    }
    m_robbers.clear();
    // Houses after the cities (0x0047B9C0).
    advance_houses(*m_world, *m_buildings, kTickMs, &m_house_events);
    if (m_house_events.size() > kMaxPendingEvents) {
        m_house_events.erase(m_house_events.begin(),
                             m_house_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
    }
    // Soldiers' upkeep of human players every 5 s (0x00476350): a twelfth of the upkeep per minute.
    m_upkeep_ms += kTickMs;
    if (m_upkeep_ms >= kUpkeepPeriodMs) {
        m_upkeep_ms -= kUpkeepPeriodMs;
        pay_upkeep();
    }
    // Disasters in epochs of 10 s (0x0047A020): they spread, end or burn the building down.
    m_disaster_ms += kTickMs;
    if (m_disaster_ms >= kDisasterEpochMs) {
        m_disaster_ms -= kDisasterEpochMs;
        const std::size_t first = m_construction_events.size();
        advance_disasters(*m_world, *m_buildings, &m_disaster_events, &m_construction_events);
        handle(m_construction_events, first);
        if (m_disaster_events.size() > kMaxPendingEvents) {
            m_disaster_events.erase(m_disaster_events.begin(),
                                    m_disaster_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    // Island epochs of 30 s (0x0046B3E0): droughts, then deer come back and settle. The phase
    // steps every 30 s; an island whose last phase differs is ticked. In a new game that happens
    // at once for islands whose template says so (docs/re/newgame.md).
    m_island_ms += kTickMs;
    bool epoch = false;
    if (m_island_ms >= Logistics::kIslandEpochMs) {
        m_island_ms -= Logistics::kIslandEpochMs;
        m_world->set_island_phase(static_cast<std::uint8_t>(m_world->island_phase() + 1U));
        epoch = true;
    }
    std::vector<bool> due(m_world->islands().size(), false);
    bool any = false;
    for (std::uint16_t i = 0; i < m_world->islands().size(); ++i) {
        WorldIsland& island = m_world->island(i);
        if (island.last_phase != m_world->island_phase()) {
            island.last_phase = m_world->island_phase();
            due[i] = true;
            any = true;
        }
    }
    if (any) {
        advance_island_events(
            *m_world, *m_buildings, m_rules,
            [this](std::uint16_t island, TilePos tile) { return m_logistics.field_reserved(island, tile); },
            &m_island_events, &due);
        if (m_island_events.size() > kMaxPendingEvents) {
            m_island_events.erase(m_island_events.begin(),
                                  m_island_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    if (epoch) {
        // The eruption roll; the original makes it only while no drought roll is due (S-042).
        roll_eruptions(*m_world, m_rules, &m_volcano_events);
    }
    if (any) {
        m_logistics.advance_islands(&due);
    }
    // Eruptions and lava (figures of the original, kinds 0x20 and 0x21).
    {
        const std::size_t first = m_construction_events.size();
        const assets::FigureType* lava = m_figures->get(kFigureLava);
        advance_volcanoes(*m_world, *m_buildings, lava != nullptr && lava->speed != 0 ? lava->speed : kLavaSpeed,
                          &m_volcano_events, &m_disaster_events, &m_construction_events);
        handle(m_construction_events, first);
        if (m_volcano_events.size() > kMaxPendingEvents) {
            m_volcano_events.erase(m_volcano_events.begin(),
                                   m_volcano_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    // The computer players (AI_Tick 0x0042B4B0 runs after the players' tick in each slice).
    m_ai.tick(kTickMs);
    // Figures last, as in the original (0x00451890 is the last subsystem of 0x00489670): first who
    // sees whom, then ships and shipyards, then soldiers, towers and the shots that land.
    m_combat.update();
    m_fleet.tick(kTickMs);
    {
        const std::size_t first = m_construction_events.size();
        m_military.tick(kTickMs, &m_construction_events);
        handle(m_construction_events, first);
        for (MilitaryEvent& e : m_military.take_events()) {
            m_military_events.push_back(e);
        }
        // Castles fetch their weapons with a pack donkey, like shipyards their wood.
        for (const Military::FetchRequest& r : m_military.take_fetch_requests()) {
            const assets::BuildingType* castle = nullptr;
            if (r.island < m_world->islands().size()) {
                const MapCell& c = m_world->islands()[r.island].map.top(r.x, r.y);
                castle = c.empty() ? nullptr : m_buildings->by_sprite(c.sprite);
            }
            if (castle == nullptr ||
                !m_logistics.send_yard_carrier(r.island, TilePos{r.x, r.y}, r.ware, castle->worker_figure)) {
                m_military.castle_delivery(r.island, r.x, r.y, r.ware, 0);
            }
        }
        if (m_military_events.size() > kMaxPendingEvents) {
            m_military_events.erase(m_military_events.begin(),
                                    m_military_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
        }
    }
    for (const Fleet::FetchRequest& r : m_fleet.take_fetch_requests()) {
        const assets::BuildingType* yard = nullptr;
        if (r.island < m_world->islands().size()) {
            const MapCell& c = m_world->islands()[r.island].map.top(r.x, r.y);
            yard = c.empty() ? nullptr : m_buildings->by_sprite(c.sprite);
        }
        if (yard == nullptr ||
            !m_logistics.send_yard_carrier(r.island, TilePos{r.x, r.y}, r.ware, yard->worker_figure)) {
            m_fleet.yard_delivery(r.island, r.x, r.y, r.ware, 0);
        }
    }
    for (ShipEvent& e : m_fleet.take_events()) {
        m_ship_events.push_back(e);
    }
    if (m_ship_events.size() > kMaxPendingEvents) {
        m_ship_events.erase(m_ship_events.begin(),
                            m_ship_events.end() - static_cast<std::ptrdiff_t>(kMaxPendingEvents / 2));
    }
    m_logistics.tick();
    for (const Logistics::YardDelivery& d : m_logistics.take_yard_deliveries()) {
        // Shipyards and castles (each ignores what is not its own).
        m_fleet.yard_delivery(d.island, static_cast<std::uint16_t>(d.origin.x), static_cast<std::uint16_t>(d.origin.y),
                              d.ware, d.amount);
        m_military.castle_delivery(d.island, static_cast<std::uint16_t>(d.origin.x),
                                   static_cast<std::uint16_t>(d.origin.y), d.ware, d.amount);
    }
    for (const DisasterEvent& e : m_logistics.take_disaster_events()) {
        m_disaster_events.push_back(e);
    }
    if (m_tick_observer) {
        m_tick_observer(*m_world);
    }
}

void Simulation::pay_upkeep() {
    std::vector<std::uint32_t> upkeep(m_world->players().size(), 0);
    for (const Soldier& s : m_world->soldiers()) {
        const assets::FigureType* type = m_figures->get(s.figure);
        if (type != nullptr && s.owner < upkeep.size() && !s.dying()) {
            upkeep[s.owner] += m_figures->unit_upkeep(type->kind);
        }
    }
    // Units in training or waiting in a castle count too (player + 0xA6 from recruiting on).
    for (const CostBuilding& b : m_world->cost_buildings()) {
        if (b.kind != CostBuilding::Kind::Castle || b.island >= m_world->islands().size()) {
            continue;
        }
        const PlayerId owner = m_world->islands()[b.island].map.top(b.x, b.y).owner;
        for (const CastleSlot& slot : b.slots) {
            const assets::FigureType* type = m_figures->get(slot.figure);
            if (type != nullptr && owner < upkeep.size()) {
                upkeep[owner] += m_figures->unit_upkeep(type->kind);
            }
        }
    }
    // Soldiers on board ships (cargo wares 25..44 = figure + 24).
    for (const Ship& ship : m_world->ships()) {
        for (const CargoSlot& c : ship.cargo) {
            if (c.ware >= 25 && c.ware <= 44 && ship.owner < upkeep.size()) {
                if (const assets::FigureType* type = m_figures->get(static_cast<std::uint32_t>(c.ware - 24))) {
                    upkeep[ship.owner] += m_figures->unit_upkeep(type->kind);
                }
            }
        }
    }
    // Cannon towers (Turm: line of figuren.cod, player + 0xA4).
    for (const Tower& t : m_world->towers()) {
        if (t.owner < upkeep.size()) {
            upkeep[t.owner] += m_figures->tower_upkeep();
        }
    }
    for (std::size_t p = 0; p < upkeep.size(); ++p) {
        Player* player = m_world->player(static_cast<PlayerId>(p));
        // Computer players pay nothing, as in the original (0x00475B00).
        if (player != nullptr && player->role == PlayerRole::Human) {
            player->money -= static_cast<std::int32_t>(upkeep[p] / 12);
        }
    }
}

std::vector<ShipEvent> Simulation::take_ship_events() {
    for (ShipEvent& e : m_fleet.take_events()) {
        m_ship_events.push_back(e);  // events of commands applied outside a tick
    }
    std::vector<ShipEvent> events;
    events.swap(m_ship_events);
    return events;
}

std::vector<HouseEvent> Simulation::take_house_events() {
    std::vector<HouseEvent> events;
    events.swap(m_house_events);
    return events;
}

std::vector<DisasterEvent> Simulation::take_disaster_events() {
    std::vector<DisasterEvent> events;
    events.swap(m_disaster_events);
    return events;
}

std::vector<VolcanoEvent> Simulation::take_volcano_events() {
    std::vector<VolcanoEvent> events;
    events.swap(m_volcano_events);
    return events;
}

std::vector<IslandEvent> Simulation::take_island_events() {
    std::vector<IslandEvent> events;
    events.swap(m_island_events);
    return events;
}

std::vector<CityEvent> Simulation::take_city_events() {
    std::vector<CityEvent> events;
    events.swap(m_city_events);
    return events;
}

std::vector<ProductionEvent> Simulation::take_production_events() {
    std::vector<ProductionEvent> events;
    events.swap(m_production_events);
    return events;
}

}  // namespace o1602::sim
