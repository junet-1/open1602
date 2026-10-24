// SPDX-License-Identifier: GPL-3.0-or-later
// The computers' soldiers on an island (BT-1, the island controller of docs/re/ai.md): at peace
// they gather at their Kontor or, without a city there, are disbanded; at war they go for the
// enemy's soldiers, or for the markets, Kontor and houses of an enemy city on the island.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/cod.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

constexpr std::uint32_t kGround = 20101;
constexpr std::uint32_t kHouse = 20103;
constexpr std::uint32_t kKontor = 20108;
constexpr std::uint16_t kSword = 1;

constexpr sim::PlayerId kHuman = 0;
constexpr sim::PlayerId kComputer = 1;

assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20101\n  Kind: 11\n  Gfx: 0\n  Size: 1, 1\n"  // open ground
        "  @Nummer: 1\n  Id: 20103\n  Kind: 14\n  Gfx: 2\n  Size: 2, 2\n  Maxenergy: 20\n  NoShotFlg: 1\n"
        "  Ruinenr: 255\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 13\n    BGruppe: 0\n    Radius: 6\n  EndObj\n"  // a house
        "  @Nummer: 2\n  Id: 20108\n  Kind: 35\n  Gfx: 30\n  Size: 2, 2\n  Maxenergy: 20\n  NoShotFlg: 1\n"
        "  KreuzBase: 6\n  Ausbauflg: 1\n  Ruinenr: 255\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 8\n    Radius: 8\n  EndObj\n"  // Kontor
        "EndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

assets::FigureTable make_figures() {
    const assets::CodPredefinedSymbol types[] = {{"FIGUR", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: FIGUR\n"
        "  Nummer: 1\n  Kind: 1\n  Gfx: 0\n  Blocknr: 1\n  Speed: 260\n  Shotradius: 0.75\n  Maxstepcnt: 1\n"
        "  Rohstoff: 11\n  Rohmenge: 3\n  Worktime: 0.8\n  Shottime: 0.6\n  Maxenergy: 20\n  Hitpoint: 1.0\n"
        "EndObj\n",
        types);
    auto table = assets::FigureTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

/// A world of 120 x 40 tiles with an island of open ground (80 x 20 at 10, 10) and an islet
/// (8 x 8 at 100, 10). Player 0 is human, 1 a computer without a ship.
struct Island {
    assets::BuildingTable buildings = make_buildings();
    assets::FigureTable figures = make_figures();
    sim::World world{120, 40};

    Island() {
        std::vector<sim::Player> players(2);
        players[kHuman].role = sim::PlayerRole::Human;
        players[kComputer].role = sim::PlayerRole::Computer;
        players[kComputer].money = 20000;
        world.set_players(players);
        add_island(10, 10, 80, 20);
        add_island(100, 10, 8, 8);
    }

    void add_island(std::uint32_t at_x, std::uint32_t at_y, std::int32_t width, std::int32_t height) {
        sim::IslandMap map(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
        for (std::int32_t y = 0; y < height; ++y) {
            for (std::int32_t x = 0; x < width; ++x) {
                REQUIRE(map.place(*buildings.by_id(kGround), x, y, 0, 0).ok());
            }
        }
        map.capture_ground();
        REQUIRE(world.add_island(sim::WorldIsland{assets::IslandHeader{}, std::move(map), at_x, at_y}).ok());
    }

    /// A building of city `city` of `owner` at tile (x, y) of the island, with its house or
    /// production entry; a Kontor is the city's warehouse.
    void build(std::uint32_t id, std::int32_t x, std::int32_t y, sim::PlayerId owner, std::uint8_t city,
               std::uint16_t island = 0) {
        REQUIRE(world.island(island).map.place(*buildings.by_id(id), x, y, 0, 0, owner, city).ok());
        if (world.city(island, city) == nullptr) {
            sim::City c;
            c.island = island;
            c.index = city;
            c.owner = owner;
            world.add_city(c);
        }
        const auto ux = static_cast<std::uint16_t>(x);
        const auto uy = static_cast<std::uint16_t>(y);
        if (id == kHouse) {
            sim::House h;
            h.island = island;
            h.x = ux;
            h.y = uy;
            h.city = city;
            world.add_house(h);
        }
        if (id == kKontor) {
            sim::Production p;
            p.island = island;
            p.x = ux;
            p.y = uy;
            world.set_production(p);
            world.city(island, city)->warehouse_x = ux;
            world.city(island, city)->warehouse_y = uy;
        }
    }

    void add(sim::PlayerId owner, std::int32_t cx, std::int32_t cy) {
        sim::Soldier s;
        s.place_cell(cx, cy);
        s.figure = kSword;
        s.owner = owner;
        s.island = 0;
        world.add_soldier(s);
    }

    void war() { REQUIRE(world.diplomacy().set_peace(world, kHuman, kComputer, sim::Treaty::None, world.time_ms())); }

    [[nodiscard]] std::vector<const sim::Soldier*> soldiers(sim::PlayerId owner) const {
        std::vector<const sim::Soldier*> out;
        for (const sim::Soldier& s : world.soldiers()) {
            if (s.owner == owner && !s.away()) {
                out.push_back(&s);
            }
        }
        return out;
    }
};

void run(sim::Simulation& s, std::uint32_t ms) {
    for (std::uint32_t t = 0; t < ms; t += sim::Simulation::kTickMs) {
        s.tick();
    }
}

}  // namespace

TEST_CASE("ai military: at peace, soldiers on an island without a city of theirs are disbanded") {
    Island a;
    a.build(kKontor, 2, 2, kComputer, 0, 1);  // its city is on the islet
    for (std::int32_t k = 0; k < 3; ++k) {
        a.add(kComputer, 20 + 2 * k, 20);
    }
    a.add(kHuman, 140, 20);
    sim::Simulation s(a.world, a.buildings, a.figures);
    run(s, 60'000);
    CHECK(a.soldiers(kComputer).empty());
    CHECK(a.soldiers(kHuman).size() == 1);  // a human's units are the human's business
}

TEST_CASE("ai military: at peace, soldiers gather at their city's Kontor") {
    Island a;
    a.build(kKontor, 60, 8, kComputer, 0);
    for (std::int32_t k = 0; k < 3; ++k) {
        a.add(kComputer, 20 + 2 * k, 20);
    }
    sim::Simulation s(a.world, a.buildings, a.figures);
    run(s, 60'000);
    const auto mine = a.soldiers(kComputer);
    REQUIRE(mine.size() == 3);  // kept: there is a city of theirs here
    for (const sim::Soldier* soldier : mine) {
        // At the Kontor (island tile 60, 8; half tiles from the world's corner), not where they were.
        const std::int32_t cx = soldier->pos_x / (sim::kPosUnit / 2);
        const std::int32_t cy = soldier->pos_y / (sim::kPosUnit / 2);
        CHECK(std::abs(cx - 2 * (10 + 60)) <= 8);
        CHECK(std::abs(cy - 2 * (10 + 8)) <= 8);
    }
}

TEST_CASE("ai military: at war, soldiers go for the enemy's soldiers on the island, out of sight too") {
    Island a;
    a.build(kKontor, 4, 8, kComputer, 0);
    for (std::int32_t k = 0; k < 5; ++k) {
        a.add(kComputer, 30, 26 + 2 * k);
    }
    a.add(kHuman, 160, 30);
    a.add(kHuman, 160, 34);
    a.war();
    sim::Simulation s(a.world, a.buildings, a.figures);
    run(s, 20'000);
    std::size_t hunting = 0;
    for (const sim::Soldier* soldier : a.soldiers(kComputer)) {
        hunting += soldier->target.kind == sim::Target::Kind::Soldier ? 1U : 0U;
    }
    // Two enemies: a quarter more (rounded down) goes at each look, until all of them go.
    CHECK(hunting == 5);
    run(s, 5 * 60'000);
    CHECK(a.soldiers(kHuman).empty());
    CHECK(!a.soldiers(kComputer).empty());
}

TEST_CASE("ai military: at war, soldiers go for the Kontor and the houses of an enemy city") {
    Island a;
    a.build(kKontor, 4, 8, kComputer, 0);
    a.build(kKontor, 70, 8, kHuman, 1);
    a.build(kHouse, 66, 4, kHuman, 1);
    a.build(kHouse, 66, 12, kHuman, 1);
    for (std::int32_t k = 0; k < 8; ++k) {
        a.add(kComputer, 30, 22 + 2 * k);
    }
    a.war();
    sim::Simulation s(a.world, a.buildings, a.figures);
    run(s, 20'000);
    std::size_t at_kontor = 0;
    std::size_t at_houses = 0;
    for (const sim::Soldier* soldier : a.soldiers(kComputer)) {
        if (soldier->target.kind == sim::Target::Kind::Building) {
            ++(soldier->target.x == 70 ? at_kontor : at_houses);
        }
    }
    CHECK(at_kontor == 4);  // the first goal gets four, each further one two
    CHECK(at_houses == 4);
    const sim::IslandMap& map = a.world.islands()[0].map;
    REQUIRE(map.top(70, 8).owner == kHuman);
    run(s, 5 * 60'000);
    CHECK(map.top(70, 8).empty() || map.top(70, 8).owner != kHuman);  // the Kontor has fallen
}
