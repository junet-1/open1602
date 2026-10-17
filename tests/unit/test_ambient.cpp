// SPDX-License-Identifier: GPL-3.0-or-later
// Townsfolk, sea creatures and jugglers (docs/re/flavor.md): life in the picture, no part of the game.

#include <set>

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/render/ambient.hpp"

using namespace o1602;

namespace {

constexpr std::uint32_t kGround = 20101;
constexpr std::uint32_t kRoad = 20401;
constexpr std::uint32_t kHouse = 20605;
constexpr std::uint32_t kTavern = 20907;
constexpr std::uint32_t kMarket = 20801;
constexpr std::uint32_t kSea = 21201;

assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}, {"BGRUPPE", 5}};
    const auto doc = assets::cod::parse(
        "Objekt: BGRUPPE\n"
        "  Nummer: 0\n  Prozent: 0\n  Maxwohn: 2\n  Steuer: 1.4\n"
        "EndObj\n"
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20101\n  Kind: 11\n  Gfx: 0\n  Size: 1, 1\n"
        "  @Nummer: 1\n  Id: 20401\n  Kind: 1\n  Gfx: 1\n  Size: 1, 1\n"
        "  @Nummer: 2\n  Id: 20605\n  Kind: 14\n  Gfx: 10\n  Size: 2, 2\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 13\n    BGruppe: 0\n  EndObj\n"
        "  @Nummer: 3\n  Id: 20907\n  Kind: 14\n  Gfx: 20\n  Size: 2, 2\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 17\n  EndObj\n"
        "  @Nummer: 4\n  Id: 20801\n  Kind: 14\n  Gfx: 30\n  Size: 4, 3\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 7\n    Rauchfignr: 0, 70\n  EndObj\n"
        "  @Nummer: 5\n  Id: 21201\n  Kind: 19\n  Gfx: 50\n  Size: 1, 1\n"
        "EndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

/// The townsfolk, the sea creatures and the juggler with the values of figuren.cod that matter.
assets::FigureTable make_figures() {
    const assets::CodPredefinedSymbol types[] = {{"FIGUR", 1}};
    std::string cod = "Objekt: FIGUR\n";
    for (int n = 93; n <= 100; ++n) {
        cod += "  Nummer: " + std::to_string(n) +
               "\n  Speed: 200\n  Rotate: 8\n  Gfx: 0\n"
               "  Objekt: ANIM\n    Nummer: 0\n    Kind: 1\n    AnimOffs: 0\n    AnimAdd: 1\n    AnimAnz: 8\n"
               "    AnimSpeed: 85\n  EndObj\n";
    }
    cod +=
        "  Nummer: 101\n  Speed: 330\n  Worktime: 5.7\n  Rotate: 24\n  Gfx: 0\n"
        "  Objekt: ANIM\n    Nummer: 0\n    Kind: 1\n    AnimAnz: 15\n    AnimSpeed: 71\n  EndObj\n"
        "  Nummer: 102\n  Speed: 200\n  Worktime: 3\n  Rotate: 16\n  Gfx: 456\n"
        "  Objekt: ANIM\n    Nummer: 0\n    Kind: 1\n    AnimAnz: 16\n    AnimSpeed: 80\n"
        "    Nummer: 2\n    Kind: 1\n    AnimAnz: 1\n    AnimSpeed: 600\n"
        "    Nummer: 3\n    Kind: 1\n    AnimAnz: 1\n    AnimSpeed: 1100\n  EndObj\n"
        "  Nummer: 103\n  Speed: 200\n  Worktime: 5\n  Gfx: 384\n"
        "  Objekt: ANIM\n    Nummer: 0\n    Kind: 1\n    AnimAnz: 24\n    AnimSpeed: 60\n  EndObj\n"
        "  Nummer: 70\n  Rotate: 8\n  Gfx: 0\n  Fahnoffs: 2.1, 0, 0.6\n"
        "  Objekt: ANIM\n"
        "    Nummer: 0\n    Kind: 1\n    AnimAnz: 8\n    AnimSpeed: 90\n"
        "    Nummer: 1\n    Kind: 1\n    AnimAnz: 8\n    AnimSpeed: 90\n"
        "    Nummer: 2\n    Kind: 1\n    AnimAnz: 8\n    AnimSpeed: 90\n"
        "    Nummer: 3\n    Kind: 1\n    AnimAnz: 8\n    AnimSpeed: 90\n"
        "    Nummer: 4\n    Kind: 1\n    AnimAnz: 8\n    AnimSpeed: 90\n"
        "  EndObj\n"
        "EndObj\n";
    auto table = assets::FigureTable::from_cod(assets::cod::parse(cod, types));
    REQUIRE(table.ok());
    return std::move(table).value();
}

/// A 30 x 20 island at (100, 100) with a road along row 10, a house of city 0 at its west end, a
/// tavern 9 steps east (inside the 21 x 21 window of the search) and a market; open sea around it.
struct Scene {
    assets::BuildingTable buildings = make_buildings();
    assets::FigureTable figures = make_figures();
    sim::World world;
    sim::IslandMap map{30, 20};
    render::AmbientView view;

    Scene() {
        for (std::int32_t y = 0; y < 20; ++y) {
            for (std::int32_t x = 0; x < 30; ++x) {
                REQUIRE(
                    map.place(*buildings.by_id(y == 10 && x >= 3 && x <= 10 ? kRoad : kGround), x, y, 0, 0, 0, 0).ok());
            }
        }
        map.capture_ground();
        REQUIRE(map.place(*buildings.by_id(kHouse), 1, 9, 0, 0, 0, 0).ok());
        REQUIRE(map.place(*buildings.by_id(kTavern), 11, 9, 0, 0, 0, 0).ok());
        REQUIRE(map.place(*buildings.by_id(kMarket), 20, 2, 0, 0, 0, 0).ok());
        REQUIRE(world.add_island(sim::WorldIsland{assets::IslandHeader{}, std::move(map), 100, 100}).ok());
        sim::City city;
        city.owner = 0;
        city.inhabitants = {10, 40, 0, 0, 0};
        world.add_city(city);
        std::vector<sim::Player> players(1);
        players[0].role = sim::PlayerRole::Human;
        world.set_players(players);
        sim::House h;
        h.x = 1;
        h.y = 9;
        h.city = 0;
        world.add_house(h);
        view.islands = {true};
    }
    sim::City& city() { return *world.city(0, 0); }
};

}  // namespace

TEST_CASE("ambient: townsfolk walk from their house along the road to a tavern and vanish") {
    Scene s;
    render::Ambient ambient(5);
    std::set<std::uint16_t> seen;
    bool walked = false;
    // Over many rounds a walker comes: the house list rate of the original is low.
    for (int t = 0; t < 400'000 && !walked; ++t) {
        ambient.update(s.world, s.buildings, s.figures, s.view, 100);
        for (const render::FigureInstance& f : ambient.walkers()) {
            walked = true;
            seen.insert(f.figure);
            // Drawn only on the road (row 10, between the house and the tavern).
            CHECK(f.y >= 110.0);
            CHECK(f.y <= 111.0);
            CHECK(f.x >= 103.0);
            CHECK(f.x <= 111.5);
            CHECK(f.sheet == assets::FigureSheet::Traeger);
        }
    }
    REQUIRE(walked);
    for (const std::uint16_t f : seen) {
        const auto choices = render::townsfolk_for(17);
        CHECK(std::find(choices.begin(), choices.end(), f) != choices.end());
    }
    // ... and vanishes at the tavern: 9 tiles at 0.4 tiles per second.
    for (int t = 0; t < 400; ++t) {
        ambient.update(s.world, s.buildings, s.figures, s.view, 100);
    }
    CHECK(ambient.walkers().size() <= 1);
}

TEST_CASE("ambient: no townsfolk in small cities, off screen or with foreign soldiers") {
    Scene s;
    s.city().inhabitants = {50, 29, 0, 0, 0};  // settlers and better: 29
    render::Ambient small(5);
    for (int t = 0; t < 400'000; ++t) {
        small.update(s.world, s.buildings, s.figures, s.view, 100);
    }
    CHECK(small.stats().walkers == 0);

    s.city().inhabitants = {10, 40, 0, 0, 0};
    s.view.islands = {false};
    render::Ambient away(5);
    for (int t = 0; t < 400'000; ++t) {
        away.update(s.world, s.buildings, s.figures, s.view, 100);
    }
    CHECK(away.stats().walkers == 0);

    s.view.islands = {true};
    std::vector<sim::Player> players(2);
    players[1].role = sim::PlayerRole::Computer;
    s.world.set_players(players);
    s.world.add_soldier(sim::Soldier{105.0, 105.0, 1, 1, 0, 0});
    render::Ambient troops(5);
    for (int t = 0; t < 400'000; ++t) {
        troops.update(s.world, s.buildings, s.figures, s.view, 100);
    }
    CHECK(troops.stats().walkers == 0);
}

TEST_CASE("ambient: sea creatures keep away from islands and ships, live a few seconds") {
    Scene s;
    // Open sea far from the island.
    s.view.tile_at = [](double, double) { return std::optional<sim::TilePos>(sim::TilePos{300, 250}); };
    render::Ambient ambient(9);
    std::set<std::uint16_t> species;
    for (int t = 0; t < 4000; ++t) {  // 400 s: 80 chances
        ambient.update(s.world, s.buildings, s.figures, s.view, 100);
        for (const render::EffectFigure& e : ambient.effects()) {
            if (e.sheet == assets::FigureSheet::Fische) {
                species.insert(e.figure);
                CHECK(e.x > 293.0);  // a pod spreads over 3 tiles, a whale swims 5.7
                CHECK(e.x < 308.0);
                CHECK(e.y > 243.0);
                CHECK(e.y < 258.0);
            }
        }
    }
    CHECK(ambient.stats().creatures == 80);
    CHECK(species.count(render::kFigureDolphin) == 1);
    CHECK(species.count(render::kFigureWhale) == 1);
    CHECK(species.count(render::kFigureKraken) == 1);
    // Gone 12.5 s after the last one at the latest.
    s.view.tile_at = nullptr;
    for (int t = 0; t < 130; ++t) {
        ambient.update(s.world, s.buildings, s.figures, s.view, 100);
    }
    CHECK(ambient.effects().empty());

    // Inside the island's rectangle (water or not) and close to a ship: none.
    s.view.tile_at = [](double, double) { return std::optional<sim::TilePos>(sim::TilePos{110, 105}); };
    render::Ambient inside(9);
    for (int t = 0; t < 1000; ++t) {
        inside.update(s.world, s.buildings, s.figures, s.view, 100);
    }
    CHECK(inside.stats().creatures == 0);
    s.world.add_ship(sim::Ship{"Sea Lion", 302, 252, 21, 0, 0, {}});
    s.view.tile_at = [](double, double) { return std::optional<sim::TilePos>(sim::TilePos{300, 250}); };
    render::Ambient ship(9);
    for (int t = 0; t < 1000; ++t) {
        ship.update(s.world, s.buildings, s.figures, s.view, 100);
    }
    CHECK(ship.stats().creatures == 0);
}

TEST_CASE("ambient: a juggler on the market of a city with more than 59 citizens") {
    Scene s;
    sim::Production market;
    market.x = 20;
    market.y = 2;
    s.world.set_production(market);
    s.city().inhabitants = {0, 0, 59, 0, 0};
    render::Ambient ambient(1);
    ambient.update(s.world, s.buildings, s.figures, s.view, 100);
    CHECK(ambient.effects().empty());
    s.city().inhabitants = {0, 0, 30, 30, 0};
    ambient.update(s.world, s.buildings, s.figures, s.view, 100);
    REQUIRE(ambient.effects().size() == 1);
    const render::EffectFigure& j = ambient.effects()[0];
    CHECK(j.figure == render::kFigureJuggler);
    CHECK(j.sheet == assets::FigureSheet::Gaukler);
    CHECK(j.anim == (20 + 2) % 5);       // the act follows the market's position
    CHECK(j.x == 100 + 20 + 2.0 + 2.1);  // centre of the 4 x 3 market and Fahnoffs 2.1
    CHECK(j.y == 100 + 2 + 1.5);
    CHECK(j.z == 0.6);
    CHECK(j.direction == 2);
}
