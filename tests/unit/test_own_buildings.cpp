// SPDX-License-Identifier: GPL-3.0-or-later
// Buildings a package adds: own_buildings.toml, the Ids the building table gives them, their
// values from buildings.toml and the savegame section OWNB
// (engine/assets/include/o1602/assets/own_buildings.hpp, docs/modding/own-buildings.md).

#include <string>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/own_buildings.hpp"
#include "o1602/sim/balance.hpp"
#include "o1602/sim/save_game.hpp"
#include "o1602/sim/world.hpp"
#include "o1602/ui/build_menu.hpp"

using namespace o1602;

namespace {

/// A bakery and a watchtower of the original, enough to copy from.
assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20503\n  Kind: 14\n  Gfx: 10\n  Size: 2, 2\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 1\n    Ware: 14\n    Rohmenge: 2\n    Prodmenge: 1\n"
        "    Interval: 19\n    Maxlager: 4\n    Radius: 8\n  EndObj\n"
        "  Objekt: HAUS_BAUKOST\n    Money: 150\n    Werkzeug: 2\n    Holz: 6\n  EndObj\n"
        "  @Nummer: 1\n  Id: 20305\n  Kind: 14\n  Gfx: 20\n  Size: 1, 1\n"
        "EndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

}  // namespace

TEST_CASE("own buildings: a package adds a building that copies one of the original") {
    auto buildings = make_buildings();
    std::vector<assets::OwnBuilding> own;
    std::vector<std::string> warnings;
    assets::apply_own_buildings_file(buildings, own, "lights",
                                     "[lighthouse]\n"
                                     "like = \"anno1602:watchtower\"\n"
                                     "gfx = 500\n"
                                     "size_x = 2\n"
                                     "size_y = 3\n"
                                     "[bakery2]\n"
                                     "like = \"anno1602:20503\"\n",
                                     warnings);
    CHECK(warnings.empty());
    REQUIRE(own.size() == 2);
    CHECK(own[0].id == "lights:lighthouse");
    CHECK(own[1].id == "lights:bakery2");
    // The Ids are the highest free ones, in the order the file names them.
    CHECK(own[0].number == assets::BuildingTable::kMaxId);
    CHECK(own[1].number == assets::BuildingTable::kMaxId - 1);
    CHECK(assets::find_own_building(own, "lights:lighthouse") == &own[0]);
    CHECK(assets::find_own_building(own, "lights:nothing") == nullptr);

    const assets::BuildingType* tower = buildings.by_id(own[0].number);
    REQUIRE(tower != nullptr);
    CHECK(tower->gfx == 500);
    CHECK(tower->size_x == 2);
    CHECK(tower->size_y == 3);
    CHECK(tower->kind == buildings.by_id(20305)->kind);
    CHECK(buildings.by_sprite(500) == tower);  // its own sprites, the original's stay as they are
    CHECK(buildings.by_sprite(20) == buildings.by_id(20305));
    const assets::BuildingType* second = buildings.by_id(own[1].number);
    REQUIRE(second != nullptr);
    CHECK(second->gfx == buildings.by_id(20503)->gfx);  // without 'gfx' the one it copies
    CHECK(second->produce_amount == buildings.by_id(20503)->produce_amount);
    CHECK(second->record != buildings.by_id(20503)->record);
    CHECK(buildings.by_record(second->record) == second);
}

TEST_CASE("own buildings: a table that is wrong is passed over with a warning") {
    auto buildings = make_buildings();
    std::vector<assets::OwnBuilding> own;
    std::vector<std::string> warnings;
    assets::apply_own_buildings_file(buildings, own, "lights",
                                     "[Lighthouse]\n"  // line 1: upper case
                                     "like = \"anno1602:watchtower\"\n"
                                     "[pier]\n"  // line 3: no 'like'
                                     "gfx = 7\n"
                                     "[tower]\n"
                                     "like = \"anno1602:ziggurat\"\n"  // line 6: no such building
                                     "[hut]\n"
                                     "like = \"other:hut\"\n"  // line 8: not the original's
                                     "[shed]\n"
                                     "like = \"anno1602:watchtower\"\n"
                                     "size_x = 0\n"  // line 11: below its limit
                                     "[barn]\n"
                                     "like = \"anno1602:watchtower\"\n"
                                     "colour = 3\n",  // line 14: no field of a building
                                     warnings);
    REQUIRE(own.size() == 1);
    CHECK(own[0].id == "lights:barn");
    REQUIRE(warnings.size() == 6);
    CHECK(warnings[0] ==
          "lights/own_buildings.toml line 1: [Lighthouse]: a building's name holds only lower case letters, "
          "digits and '_'");
    CHECK(warnings[1] == "lights/own_buildings.toml line 3: lights:pier names no building to copy ('like')");
    CHECK(warnings[2] ==
          "lights/own_buildings.toml line 6: 'like' names no building of the original "
          "(anno1602:<number or name>)");
    CHECK(warnings[3] ==
          "lights/own_buildings.toml line 8: 'like' names no building of the original "
          "(anno1602:<number or name>)");
    CHECK(warnings[4] == "lights/own_buildings.toml line 11: 'size_x' is a number from 1 to 255");
    CHECK(warnings[5] ==
          "lights/own_buildings.toml line 14: 'colour' is no field of a building of a package and is "
          "passed over");
}

TEST_CASE("own buildings: a second package declares the same name again, and changes values") {
    auto buildings = make_buildings();
    std::vector<assets::OwnBuilding> own;
    std::vector<std::string> warnings;
    assets::apply_own_buildings_file(buildings, own, "lights", "[lighthouse]\nlike = \"anno1602:watchtower\"\n",
                                     warnings);
    assets::apply_own_buildings_file(buildings, own, "lights", "[lighthouse]\nlike = \"anno1602:watchtower\"\n",
                                     warnings);
    REQUIRE(own.size() == 1);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0] ==
          "lights/own_buildings.toml line 1: lights:lighthouse is declared twice; the second is passed over");

    // Another package changes the values of that building by its id.
    warnings.clear();
    sim::Balance balance;
    sim::apply_buildings_file(balance, "harder",
                              "[\"lights:lighthouse\"]\n"
                              "health = 50\n"
                              "[\"lights:nothing\"]\n"  // line 3: no building of any package
                              "health = 50\n",
                              warnings, own);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0] ==
          "harder/buildings.toml line 3: [lights:nothing]: names none of the original's buildings "
          "(anno1602:<number or name>) and none a package added");
    REQUIRE(balance.buildings.size() == 1);
    CHECK((balance.buildings[0] == sim::BuildingValue{own[0].number, sim::BuildingField::Health, 50}));
    const auto original = buildings;
    sim::apply_building_values(buildings, original, balance);
    CHECK(buildings.by_id(own[0].number)->max_health == 50 * assets::kStockUnitsPerTon);
}

TEST_CASE("own buildings: the build menu names a building by its id") {
    auto buildings = make_buildings();
    std::vector<assets::OwnBuilding> own;
    std::vector<std::string> warnings;
    assets::apply_own_buildings_file(buildings, own, "lights", "[lighthouse]\nlike = \"anno1602:watchtower\"\n",
                                     warnings);
    ui::BuildMenuDefinition menu = ui::original_build_menu();
    const auto status = ui::apply_build_menu_package(menu, "lights",
                                                     "[tower]\n"
                                                     "building = \"lights:lighthouse\"\n"
                                                     "category = 31007\n"
                                                     "icon = 90\n"
                                                     "name = \"Leuchtturm\"\n"
                                                     "[bakery]\n"
                                                     "building = \"anno1602:bakery\"\n"
                                                     "category = 31007\n"
                                                     "[gone]\n"
                                                     "building = \"lights:nothing\"\n"  // line 10
                                                     "category = 31007\n",
                                                     buildings, warnings, own);
    CHECK(status.ok());
    const ui::BuildCategory* harbour = menu.category(31007);
    REQUIRE(harbour != nullptr);
    CHECK(harbour->index_of(own[0].number).has_value());
    CHECK(harbour->index_of(20503).has_value());
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].find("line 10: 'building' names no building") != std::string::npos);
}

TEST_CASE("own buildings: a game keeps the ids of the buildings its packages added") {
    auto buildings = make_buildings();
    std::vector<assets::OwnBuilding> own;
    std::vector<std::string> warnings;
    assets::apply_own_buildings_file(buildings, own, "lights", "[lighthouse]\nlike = \"anno1602:watchtower\"\n",
                                     warnings);
    sim::World world(60, 40);
    world.set_own_buildings(own);
    const auto bytes = sim::write_game(world, buildings);
    REQUIRE(bytes.ok());
    auto read = sim::read_game(*bytes, buildings);
    REQUIRE(read.ok());
    REQUIRE(read->own_buildings().size() == 1);
    CHECK(read->own_buildings()[0].id == "lights:lighthouse");
    CHECK(read->own_buildings()[0].number == own[0].number);

    // Without that building the game does not load: its tiles would name nothing.
    const auto plain = make_buildings();
    CHECK(!sim::read_game(*bytes, plain).ok());
}
