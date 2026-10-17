// SPDX-License-Identifier: GPL-3.0-or-later
// Building and demolishing (docs/re/construction.md).

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/sim/construction.hpp"
#include "o1602/sim/growth.hpp"
#include "o1602/sim/simulation.hpp"

using namespace o1602;

namespace {

constexpr std::uint32_t kGround = 20101;
constexpr std::uint32_t kSea = 21201;
constexpr std::uint32_t kBeach = 20501;
constexpr std::uint32_t kTree = 20202;
constexpr std::uint32_t kRoad = 20401;
constexpr std::uint32_t kRoadCurve = 20402;
constexpr std::uint32_t kRoadT = 20403;
constexpr std::uint32_t kKontor = 22001;
constexpr std::uint32_t kKontor2 = 22002;
constexpr std::uint32_t kMarket = 20701;
constexpr std::uint32_t kHouse = 20801;
constexpr std::uint32_t kMeadow = 22722;
constexpr std::uint32_t kField = 21301;
constexpr std::uint32_t kCastle = 21501;

assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20101\n  Kind: 11\n  Gfx: 0\n  Size: 1, 1\n  Randwachs: 100\n"
        "  @Nummer: 1\n  Id: 21201\n  Kind: 19\n  Gfx: 1\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 9\n  EndObj\n"
        "  @Nummer: 2\n  Id: 20501\n  Kind: 23\n  Gfx: 2\n  Size: 1, 1\n"
        "  @Nummer: 3\n  Id: 20202\n  Kind: 10\n  Gfx: 3\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 9\n    Ware: 53\n  EndObj\n"
        // Road family: straight, curve, T, cross (KreuzBase = record 4).
        "  @Nummer: 4\n  Id: 20401\n  Kind: 1\n  Gfx: 4\n  Size: 1, 1\n  KreuzBase: 4\n"
        "  Objekt: HAUS_BAUKOST\n    Money: 5\n  EndObj\n"
        "  @Nummer: 5\n  Id: 20402\n  Kind: 1\n  Gfx: 5\n  Size: 1, 1\n  KreuzBase: 4\n"
        "  @Nummer: 6\n  Id: 20403\n  Kind: 1\n  Gfx: 6\n  Size: 1, 1\n  KreuzBase: 4\n"
        "  @Nummer: 7\n  Id: 20404\n  Kind: 1\n  Gfx: 7\n  Size: 1, 1\n  KreuzBase: 4\n"
        // Kontor I and II (family record 8).
        "  @Nummer: 8\n  Id: 22001\n  Kind: 35\n  Gfx: 20\n  Size: 2, 3\n  Strandflg: 1\n  PlaceFlg: 1\n"
        "  KreuzBase: 8\n  Ausbauflg: 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 8\n    Radius: 6\n    Maxlager: 30\n  EndObj\n"
        "  Objekt: HAUS_BAUKOST\n    Werkzeug: 3\n    Holz: 4\n    Money: 100\n  EndObj\n"
        "  @Nummer: 9\n  Id: 22002\n  Kind: 35\n  Gfx: 26\n  Size: 2, 3\n  Strandflg: 1\n  PlaceFlg: 1\n"
        "  KreuzBase: 8\n  Ausbauflg: 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 8\n    Radius: 6\n    Maxlager: 50\n    Bauinfra: 17\n  EndObj\n"
        "  Objekt: HAUS_BAUKOST\n    Money: 200\n  EndObj\n"
        "  @Nummer: 10\n  Id: 20701\n  Kind: 14\n  Gfx: 32\n  Size: 2, 2\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 7\n    Radius: 5\n  EndObj\n"
        "  Objekt: HAUS_BAUKOST\n    Money: 50\n  EndObj\n"
        "  @Nummer: 11\n  Id: 20801\n  Kind: 14\n  Gfx: 36\n  Size: 2, 2\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 13\n    BGruppe: 0\n  EndObj\n"
        "  Objekt: HAUS_BAUKOST\n    Holz: 1\n    Money: 10\n  EndObj\n"
        "  @Nummer: 12\n  Id: 22722\n  Kind: 11\n  Gfx: 40\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 9\n    Ware: 52\n  EndObj\n"
        // A crop: growing, ripe, withered.
        "  @Nummer: 13\n  Id: 21301\n  Kind: 10\n  Gfx: 41\n  Size: 1, 1\n  AnimAnz: 5\n  AnimAdd: 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 10\n    Interval: 300\n  EndObj\n"
        "  Objekt: HAUS_BAUKOST\n    Money: 5\n  EndObj\n"
        "  @Nummer: 14\n  Id: 21302\n  Kind: 10\n  Gfx: 46\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 9\n    Ware: 46\n  EndObj\n"
        "  @Nummer: 15\n  Id: 21303\n  Kind: 10\n  Gfx: 47\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 10\n    Doerrflg: 1\n  EndObj\n"
        // A castle (Bauinfra 11: one per player).
        "  @Nummer: 16\n  Id: 21501\n  Kind: 14\n  Gfx: 50\n  Size: 2, 2\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 3\n    Bauinfra: 11\n  EndObj\n"
        "  Objekt: HAUS_BAUKOST\n    Money: 30\n  EndObj\n"
        "EndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

/// An island of 30 x 30 tiles: sea x 0..3, beach x 4, ground beyond, a wood at x 20..24, y 5..9.
struct Island {
    assets::BuildingTable buildings = make_buildings();
    sim::World world{100, 100};
    std::vector<std::string> names{"Abemsberg", "Abmont"};

    explicit Island(std::uint32_t fertility = 0x2U, std::uint8_t climate = 0) {
        sim::IslandMap map(30, 30);
        for (std::int32_t y = 0; y < 30; ++y) {
            for (std::int32_t x = 0; x < 30; ++x) {
                std::uint32_t id = kGround;
                if (x < 4) {
                    id = kSea;
                } else if (x == 4) {
                    id = kBeach;
                } else if (x >= 20 && x <= 24 && y >= 5 && y <= 9) {
                    id = kTree;
                }
                REQUIRE(map.place(*buildings.by_id(id), x, y, 0, 0).ok());
            }
        }
        map.capture_ground();
        sim::WorldIsland island{assets::IslandHeader{}, std::move(map), 10, 10};
        island.header.fertility = fertility;
        island.header.climate = climate;
        REQUIRE(world.add_island(std::move(island)).ok());
        std::vector<sim::Player> players(2);
        players[0].role = sim::PlayerRole::Human;
        players[0].money = 10000;
        players[0].infra_mask = 0xFFFFFFFFU & ~(1U << 16);  // all but level 17 (Kontor II)
        players[1].role = sim::PlayerRole::Computer;
        players[1].money = 10000;
        world.set_players(players);
        sim::Ship ship{"Patria", 12, 21, 21, 0, 0, {}};
        ship.cargo.push_back(sim::Ship::Cargo{buildings.materials().tools, 10 * 32});
        ship.cargo.push_back(sim::Ship::Cargo{buildings.materials().wood, 10 * 32});
        world.add_ship(ship);
    }

    sim::BuildPlan build(std::vector<sim::BuildItem> items, std::optional<std::size_t> ship = std::nullopt,
                         sim::PlayerId player = 0) {
        sim::BuildOrder order;
        order.player = player;
        order.island = 0;
        order.items = std::move(items);
        order.ship = ship;
        return sim::execute_build(world, buildings, order, names, &events);
    }
    std::size_t demolish(sim::TilePos a, sim::TilePos b) {
        sim::DemolishOrder order{0, 0, a, b};
        return sim::execute_demolish(world, buildings, order, &events).removed;
    }
    const assets::BuildingType* at(std::int32_t x, std::int32_t y) const {
        const auto& cell = world.islands()[0].map.top(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
        return buildings.by_sprite(cell.sprite);
    }
    std::uint8_t city_at(std::int32_t x, std::int32_t y) const {
        return world.islands()[0].map.top(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)).city;
    }
    /// Kontor at the beach, founded from the ship.
    void found() {
        const sim::BuildPlan plan = build({{kKontor, 5, 10, 0, 0}}, 0);
        REQUIRE(plan.placed == 1);
        // The ship unloads its wood (the founding leaves the cargo on board).
        sim::City* city = world.city(0, 0);
        city->ensure_ware(buildings.materials().wood);
        city->goods[buildings.materials().wood] = 6 * 32;
    }

    std::vector<sim::ConstructionEvent> events;
};

}  // namespace

TEST_CASE("construction: the first Kontor at the coast founds a city, paid from the ship") {
    Island island;
    const sim::BuildPlan plan = island.build({{kKontor, 5, 10, 0, 0}}, 0);
    REQUIRE(plan.placed == 1);
    // The long side faces the beach on the left (0x00467AF0).
    CHECK(plan.items[0].rotation == 2);
    REQUIRE(island.world.cities().size() == 1);
    const sim::City& city = island.world.cities()[0];
    CHECK(city.name == "Abemsberg");
    CHECK(city.owner == 0);
    CHECK(city.warehouse_x == 5);
    CHECK(city.warehouse_y == 10);
    CHECK(island.world.city_name_index() == 1);
    // Territory: radius max(6, 8) around the Kontor, never the sea.
    CHECK(island.city_at(5, 10) == city.index);
    CHECK(island.city_at(13, 11) == city.index);
    CHECK(island.city_at(16, 11) == sim::MapCell::kNoCity);
    CHECK(island.city_at(2, 11) == sim::MapCell::kNoCity);
    CHECK(island.world.production_index(0, 5, 10).has_value());
    // The ship paid the goods, the player the gold.
    CHECK(island.world.ships()[0].load(island.buildings.materials().tools) == 7 * 32);
    CHECK(island.world.ships()[0].load(island.buildings.materials().wood) == 6 * 32);
    CHECK(island.world.players()[0].money == 10000 - 100);
    // One city per player and island.
    const sim::BuildPlan again = island.build({{kKontor, 5, 20, 0, 0}}, 0);
    CHECK(again.items[0].refusal == sim::BuildRefusal::CityLimit);
}

TEST_CASE("construction: a Kontor needs the coast along its long side and a ship nearby") {
    Island island;
    CHECK(island.build({{kKontor, 10, 10, 0, 0}}, 0).items[0].refusal == sim::BuildRefusal::NoCoast);
    island.world.ships()[0].x = 90;  // far away
    CHECK(island.build({{kKontor, 5, 10, 0, 0}}, 0).items[0].refusal == sim::BuildRefusal::ShipTooFar);
    // Without a ship there are no goods.
    CHECK(island.build({{kKontor, 5, 10, 0, 0}}).items[0].refusal == sim::BuildRefusal::Tools);
}

TEST_CASE("construction: roads fit to their neighbours, a drag stops when the gold runs out") {
    Island island;
    island.found();
    const sim::BuildPlan plan = island.build({{kRoad, 8, 14, 0, 0}, {kRoad, 9, 14, 0, 0}, {kRoad, 10, 14, 0, 0}});
    CHECK(plan.placed == 3);
    CHECK(plan.bill.money == 15);
    CHECK(island.at(9, 14)->id == kRoad);
    // A branch makes a T piece (mask W + E + S), its end a straight piece.
    CHECK(island.build({{kRoad, 9, 15, 0, 0}}).placed == 1);
    CHECK(island.at(9, 14)->id == kRoadT);
    CHECK(island.world.islands()[0].map.top(9, 14).rotation == 1);
    // A curve: W + S at (10, 15) after adding (10, 15) next to (9, 15) and (10, 14).
    CHECK(island.build({{kRoad, 10, 15, 0, 0}}).placed == 1);
    CHECK(island.at(10, 15)->id == kRoadCurve);
    // Gold for two roads only: the rest of the drag stays unbuilt.
    island.world.player(0)->money = 12;
    const sim::BuildPlan poor = island.build({{kRoad, 8, 17, 0, 0}, {kRoad, 9, 17, 0, 0}, {kRoad, 10, 17, 0, 0}});
    CHECK(poor.placed == 2);
    CHECK(poor.items[2].refusal == sim::BuildRefusal::Money);
    CHECK(island.world.players()[0].money == 2);
}

TEST_CASE("construction: buildings stay inside their city, do not overlap and need their unlock") {
    Island island;
    island.found();
    CHECK(island.build({{kRoad, 25, 25, 0, 0}}).items[0].refusal == sim::BuildRefusal::Territory);
    // S-008: half outside is refused (the original annexed it).
    CHECK(island.build({{kHouse, 14, 11, 0, 0}}).items[0].refusal == sim::BuildRefusal::Territory);
    const sim::BuildPlan two = island.build({{kHouse, 8, 13, 0, 0}, {kHouse, 9, 13, 0, 0}});
    CHECK(two.placed == 1);
    CHECK(two.items[1].refusal == sim::BuildRefusal::Overlap);
    // Kontor II: locked until level 17, then it replaces Kontor I in place.
    CHECK(island.build({{kKontor2, 5, 11, 0, 0}}).items[0].refusal == sim::BuildRefusal::Locked);
    island.world.player(0)->infra_mask = 0xFFFFFFFFU;
    island.world.productions()[*island.world.production_index(0, 5, 10)].enabled = false;
    const sim::BuildPlan up = island.build({{kKontor2, 5, 11, 0, 0}});
    REQUIRE(up.placed == 1);
    CHECK(up.items[0].upgrade);
    CHECK(up.items[0].x == 5);
    CHECK(up.items[0].y == 10);
    CHECK(island.at(5, 10)->id == kKontor2);
    // The upgrade keeps the state of the building (the original reset it).
    CHECK(!island.world.production(0, 5, 10)->enabled);
    // A second Kontor in the city is refused.
    CHECK(island.build({{kKontor, 5, 16, 0, 0}}).items[0].refusal == sim::BuildRefusal::SecondKontor);
}

TEST_CASE("construction: houses get one inhabitant and leave nothing behind when demolished") {
    Island island;
    island.found();
    REQUIRE(island.build({{kHouse, 8, 13, 0, 0}}).placed == 1);
    const sim::House* h = island.world.house(0, 8, 13);
    REQUIRE(h != nullptr);
    CHECK(h->inhabitants() == 1);
    CHECK(island.world.cities()[0].inhabitants[0] == 1);
    // The Kontor stays while the city has other buildings.
    CHECK(island.demolish({5, 10}, {5, 10}) == 0);
    // A rectangle leaves houses standing; a click removes them.
    CHECK(island.demolish({7, 12}, {10, 15}) == 0);
    CHECK(island.demolish({9, 14}, {9, 14}) == 1);
    CHECK(island.world.house(0, 8, 13) == nullptr);
    CHECK(island.world.cities()[0].inhabitants[0] == 0);
    CHECK(island.at(8, 13)->id == kGround);
    CHECK(island.city_at(8, 13) == 0);
    // Now the Kontor may go, and the city with it.
    CHECK(island.demolish({5, 10}, {5, 10}) == 1);
    CHECK(island.world.cities().empty());
    CHECK(island.city_at(8, 13) == sim::MapCell::kNoCity);
}

TEST_CASE("construction: a market claims land; demolished, the land and what stands on it is lost") {
    Island island;
    island.found();
    REQUIRE(island.build({{kMarket, 11, 11, 0, 0}}).placed == 1);
    CHECK(island.city_at(16, 12) == 0);  // the market's circle (radius 5) goes further
    REQUIRE(island.build({{kRoad, 16, 12, 0, 0}}).placed == 1);
    CHECK(island.demolish({11, 11}, {11, 11}) == 1);
    CHECK(island.city_at(16, 12) == sim::MapCell::kNoCity);
    CHECK(island.at(16, 12)->id == kGround);  // no ruin record: the ground comes back
    CHECK(island.city_at(12, 11) == 0);       // the Kontor still covers this
}

TEST_CASE("construction: fields grow where the island is fertile, forest becomes meadow") {
    Island fertile;  // fertile for ware 46, ground Randwachs 100: every tile carries
    fertile.found();
    const sim::BuildPlan plan = fertile.build({{kField, 9, 12, 0, 0}});
    REQUIRE(plan.placed == 1);
    bool entry = false;
    for (const auto& g : fertile.world.growth().entries) {
        entry |= g.x == 9 && g.y == 12 && g.stage == 0;
    }
    CHECK(entry);
    // Demolished, the field gives back its ground.
    CHECK(fertile.demolish({9, 12}, {9, 12}) == 1);
    CHECK(fertile.at(9, 12)->id == kGround);

    // S-003: on an island where the crop does not grow, it cannot be planted.
    Island barren(0, 1);
    barren.found();
    std::size_t refused = 0;
    for (std::int32_t x = 7; x <= 12; ++x) {
        const sim::BuildPlan p = barren.build({{kField, x, 13, 0, 0}});
        refused += p.items[0].refusal == sim::BuildRefusal::Infertile ? 1U : 0U;
    }
    CHECK(refused == 4);  // pattern 0: 2 of 32 tiles carry (here x = 8 and 12)
}

TEST_CASE("construction: building and demolishing are commands of the simulation") {
    Island island;
    assets::FigureTable figures;
    sim::Simulation simulation(island.world, island.buildings, figures);
    simulation.set_city_names(island.names);
    sim::BuildOrder order;
    order.player = 0;
    order.island = 0;
    order.items = {{kKontor, 5, 10, 0, 0}};
    order.ship = 0;
    const std::uint64_t seq = simulation.submit(order);
    CHECK(island.world.cities().empty());  // applied at the next tick
    simulation.tick();
    REQUIRE(island.world.cities().size() == 1);
    auto results = simulation.take_command_results();
    REQUIRE(results.size() == 1);
    CHECK(results[0].sequence == seq);
    REQUIRE(results[0].build.has_value());
    CHECK(results[0].build->placed == 1);
    bool founded = false;
    for (const auto& e : simulation.take_construction_events()) {
        founded |= e.kind == sim::ConstructionEvent::Kind::CityFounded;
    }
    CHECK(founded);
    // While paused, commands still apply.
    simulation.set_speed(0);
    simulation.submit(sim::DemolishOrder{0, 0, {5, 10}, {5, 10}});
    simulation.advance(100);
    CHECK(island.world.cities().empty());
}

TEST_CASE("construction: the game setting decides what a demolition gives back") {
    const auto house_back = [](sim::Refund mode, std::uint8_t percent) {
        Island island;
        island.found();
        sim::Rules rules;
        rules.demolish_refund = mode;
        rules.refund_percent = percent;
        REQUIRE(island.build({{kHouse, 8, 13, 0, 0}}).placed == 1);  // 10 gold, 1 t wood
        const std::int32_t money = island.world.players()[0].money;
        const std::uint32_t wood = island.world.cities()[0].stock(island.buildings.materials().wood);
        sim::DemolishOrder order{0, 0, {8, 13}, {8, 13}};
        const sim::DemolishResult r =
            sim::execute_demolish(island.world, island.buildings, order, nullptr, rules.refund_share());
        CHECK(r.removed == 1);
        CHECK(island.world.players()[0].money == money + r.refund.money);
        CHECK(island.world.cities()[0].stock(island.buildings.materials().wood) == wood + r.refund.wood);
        return r.refund;
    };
    // Original: nothing.
    const sim::BuildBill none = house_back(sim::Refund::None, 50);
    CHECK(none.money == 0);
    CHECK(none.wood == 0);
    // Full: everything.
    const sim::BuildBill full = house_back(sim::Refund::Full, 50);
    CHECK(full.money == 10);
    CHECK(full.wood == 32);
    // Partial: the share, rounded down (1/32 t for goods).
    const sim::BuildBill half = house_back(sim::Refund::Partial, 50);
    CHECK(half.money == 5);
    CHECK(half.wood == 16);
    const sim::BuildBill quarter = house_back(sim::Refund::Partial, 25);
    CHECK(quarter.money == 2);
    CHECK(quarter.wood == 8);
    CHECK(sim::Rules::original().refund_share() == 0);
}

TEST_CASE("construction: refunded goods only fill the free room of the warehouse") {
    Island island;
    island.found();
    REQUIRE(island.build({{kHouse, 8, 13, 0, 0}}).placed == 1);
    sim::City* city = island.world.city(0, 0);
    city->capacity = 30 * 32;
    city->goods[island.buildings.materials().wood] = 30 * 32;  // full
    sim::DemolishOrder order{0, 0, {8, 13}, {8, 13}};
    const sim::DemolishResult r = sim::execute_demolish(island.world, island.buildings, order, nullptr, 100);
    CHECK(r.refund.wood == 0);
    CHECK(r.refund.money == 10);
    CHECK(city->stock(island.buildings.materials().wood) == 30 * 32);
}

TEST_CASE("construction: a castle makes its city proud, once per player") {
    Island island;
    island.found();
    island.world.player(0)->infra_mask = 0xFFFFFFFFU;
    REQUIRE(island.build({{kCastle, 10, 12, 0, 0}}).placed == 1);
    CHECK(island.world.city(0, 0)->satisfaction_bonus == 800);
    CHECK(island.world.players()[0].castles_built == 1);
    const sim::BuildPlan again = island.build({{kCastle, 10, 15, 0, 0}});
    CHECK(again.placed == 0);
    CHECK(again.items[0].refusal == sim::BuildRefusal::Limit);
    CHECK(island.world.city(0, 0)->satisfaction_bonus == 800);
}
