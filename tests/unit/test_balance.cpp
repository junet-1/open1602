// SPDX-License-Identifier: GPL-3.0-or-later
// Values of the balance as data (MD-9, docs/modding/balance.md): the original's defaults, a
// package's balance.toml and the sections BALA and BLDV that keep them with the game; the values of
// buildings and the rule of the revised bread chain.

#include <string>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/game_data.hpp"
#include "o1602/sim/balance.hpp"
#include "o1602/sim/rules.hpp"
#include "o1602/sim/save_game.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20101\n  Kind: 11\n  Gfx: 0\n  Size: 1, 1\n"
        // A bakery and a grain farm with the original's values.
        "  @Nummer: 1\n  Id: 20503\n  Kind: 14\n  Gfx: 1\n  Size: 2, 2\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 1\n    Ware: 14\n    Rohstoff: 9\n    Rohmenge: 2\n    Prodmenge: 1\n"
        "    Interval: 19\n    Maxlager: 4\n    Kosten: 5, 0\n  EndObj\n"
        "  Objekt: HAUS_BAUKOST\n    Money: 150\n    Werkzeug: 2\n    Holz: 6\n  EndObj\n"
        "  @Nummer: 2\n  Id: 21503\n  Kind: 14\n  Gfx: 5\n  Size: 2, 2\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 2\n    Ware: 8\n    Rohstoff: 45\n    Rohmenge: 1\n    Prodmenge: 1\n"
        "    Interval: 8\n    Maxlager: 6\n    Kosten: 5, 0\n  EndObj\n"
        "  Objekt: HAUS_BAUKOST\n    Money: 100\n    Werkzeug: 2\n    Holz: 5\n  EndObj\n"
        "EndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

}  // namespace

TEST_CASE("balance: the defaults are the original's and within their limits") {
    const sim::Balance balance;
    CHECK(balance.trader_max_held == 50);
    CHECK(balance.trader_max_buy == 10);
    CHECK(balance.trader_restock_s == 60);
    CHECK(balance.castle_interval_s == 10);
    CHECK(balance.castle_unload_range == 6);
    CHECK(balance.fire_spread == 19);
    CHECK(balance.plague_radius == 4);
    CHECK(sim::valid(balance));
    CHECK(sim::World(10, 10).balance() == balance);
    sim::Balance zero = balance;
    zero.trader_max_buy = 0;
    CHECK(!sim::valid(zero));
}

TEST_CASE("balance: a package changes the values one by one") {
    sim::Balance balance;
    std::vector<std::string> warnings;
    sim::apply_balance_file(balance, "harsh",
                            "# A harder game.\n"
                            "[trader]\n"
                            "max_buy = 5\n"
                            "restock_seconds = 120\n"
                            "max_held = 0\n"
                            "\n"
                            "[disasters]\n"
                            "fire_spread = 40\n"
                            "plague = 3\n"
                            "[weather]\n"
                            "rain = 1\n",
                            warnings);
    CHECK(balance.trader_max_buy == 5);
    CHECK(balance.trader_restock_s == 120);
    CHECK(balance.trader_max_held == 50);  // out of its limits: passed over
    CHECK(balance.fire_spread == 40);
    CHECK(balance.castle_interval_s == 10);
    REQUIRE(warnings.size() == 3);
    CHECK(warnings[0] == "harsh/balance.toml line 5: 'max_held' must be a number from 1 to 200");
    CHECK(warnings[1] == "harsh/balance.toml line 9: unknown field 'plague'");
    CHECK(warnings[2] == "harsh/balance.toml line 10: there is no table [weather]");

    // A file that is no data file changes nothing.
    warnings.clear();
    const sim::Balance before = balance;
    sim::apply_balance_file(balance, "broken", "[trader]\nmax_buy = \"3\n", warnings);
    CHECK(balance == before);
    CHECK(warnings.size() == 1);
}

TEST_CASE("balance: a game keeps its values in the savegame, a damaged value loads nothing") {
    const auto buildings = make_buildings();
    sim::World world(60, 40);
    // The original's values are not written (docs/formats/o1602-save.md, BALA).
    auto plain = sim::write_game(world, buildings);
    REQUIRE(plain.ok());
    sim::Balance balance;
    balance.castle_unload_range = 9;
    balance.plague_radius = 0;
    world.set_balance(balance);
    auto bytes = sim::write_game(world, buildings);
    REQUIRE(bytes.ok());
    auto loaded = sim::read_game(*bytes, buildings);
    REQUIRE(loaded.ok());
    CHECK(loaded->balance() == balance);
    auto from_plain = sim::read_game(*plain, buildings);
    REQUIRE(from_plain.ok());
    CHECK(from_plain->balance() == sim::Balance{});

    balance.castle_unload_range = 100;
    world.set_balance(balance);
    auto damaged = sim::write_game(world, buildings);
    REQUIRE(damaged.ok());
    CHECK(!sim::read_game(*damaged, buildings).ok());
}

TEST_CASE("balance: a package changes values of the original's buildings") {
    sim::Balance balance;
    std::vector<std::string> warnings;
    sim::apply_balance_file(balance, "bread",
                            "[\"anno1602:20503\"]\n"  // line 1
                            "produce = 2\n"
                            "running_cost = 15\n"
                            "smell = 3\n"              // line 4
                            "interval = 0\n"           // line 5
                            "[\"anno1602:0x5401\"]\n"  // 21505 in hex
                            "cost_wood = 4\n"
                            "[\"anno1602:farm\"]\n"  // line 8
                            "produce = 2\n"
                            "[\"brewery:mash_tun\"]\n"  // line 10
                            "produce = 2\n"
                            "[trader]\n"
                            "max_buy = 5\n"
                            "[\"anno1602:20503\"]\n",  // a table given twice: the file is no data file
                            warnings);
    CHECK(balance == sim::Balance{});
    REQUIRE(warnings.size() == 1);

    warnings.clear();
    sim::apply_balance_file(balance, "bread",
                            "[\"anno1602:20503\"]\n"
                            "produce = 2\n"
                            "running_cost = 15\n"
                            "smell = 3\n"
                            "interval = 0\n"
                            "[\"anno1602:0x5401\"]\n"
                            "cost_wood = 4\n"
                            "[\"anno1602:farm\"]\n"
                            "produce = 2\n"
                            "[\"brewery:mash_tun\"]\n"
                            "produce = 2\n"
                            "[trader]\n"
                            "max_buy = 5\n",
                            warnings);
    CHECK(balance.trader_max_buy == 5);
    REQUIRE(balance.buildings.size() == 3);
    CHECK((balance.buildings[0] == sim::BuildingValue{20503, sim::BuildingField::Produce, 2}));
    CHECK((balance.buildings[1] == sim::BuildingValue{20503, sim::BuildingField::RunningCost, 15}));
    CHECK((balance.buildings[2] == sim::BuildingValue{21505, sim::BuildingField::BuildWood, 4}));
    CHECK(sim::valid(balance));
    REQUIRE(warnings.size() == 4);
    CHECK(warnings[0] == "bread/balance.toml line 4: unknown field 'smell'");
    CHECK(warnings[1] == "bread/balance.toml line 5: 'interval' must be a number from 1 to 3600");
    CHECK(warnings[2] == "bread/balance.toml line 8: [anno1602:farm]: no building Id after 'anno1602:'");
    CHECK(
        warnings[3] ==
        "bread/balance.toml line 10: [brewery:mash_tun]: only the original's buildings (anno1602:<Id>) can be changed");

    // A later package changes a value again; the order stays by building and field.
    warnings.clear();
    sim::apply_balance_file(balance, "later", "[\"anno1602:20503\"]\nproduce = 3\ninterval = 12\n", warnings);
    CHECK(warnings.empty());
    REQUIRE(balance.buildings.size() == 4);
    CHECK((balance.buildings[0] == sim::BuildingValue{20503, sim::BuildingField::Produce, 3}));
    CHECK((balance.buildings[1] == sim::BuildingValue{20503, sim::BuildingField::Interval, 12}));
    CHECK(sim::valid(balance));
    std::swap(balance.buildings[0], balance.buildings[1]);
    CHECK(!sim::valid(balance));  // out of order
    std::swap(balance.buildings[0], balance.buildings[1]);

    // Buildings the data does not have are dropped with a warning.
    const auto buildings = make_buildings();
    warnings.clear();
    sim::drop_unknown_buildings(balance, buildings, warnings);
    CHECK(balance.buildings.size() == 3);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0] == "balance.toml: haeuser.cod has no building anno1602:21505, its values are passed over");
}

TEST_CASE("balance: a game's building table has its values, the next game the original's again") {
    const auto original = make_buildings();
    auto table = original;
    const assets::BuildingType* bakery = table.by_id(20503);
    REQUIRE(bakery != nullptr);
    constexpr std::uint32_t kTon = assets::kStockUnitsPerTon;
    CHECK(bakery->produce_amount == kTon);
    CHECK(bakery->raw_amount == 2 * kTon);
    CHECK(bakery->running_cost == 5);

    sim::Balance balance;
    for (const auto& [field, value] :
         std::vector<std::pair<sim::BuildingField, std::uint32_t>>{{sim::BuildingField::Produce, 2},
                                                                   {sim::BuildingField::Raw, 3},
                                                                   {sim::BuildingField::Raw2, 1},
                                                                   {sim::BuildingField::Interval, 12},
                                                                   {sim::BuildingField::MaxStock, 8},
                                                                   {sim::BuildingField::RunningCost, 15},
                                                                   {sim::BuildingField::IdleCost, 2},
                                                                   {sim::BuildingField::BuildMoney, 220},
                                                                   {sim::BuildingField::BuildTools, 3},
                                                                   {sim::BuildingField::BuildWood, 7},
                                                                   {sim::BuildingField::BuildBricks, 4}}) {
        sim::set_building_value(balance, sim::BuildingValue{20503, field, value});
    }
    sim::set_building_value(balance, sim::BuildingValue{29999, sim::BuildingField::Produce, 9});  // no such building
    sim::apply_building_values(table, original, balance);
    CHECK(table.by_id(20503) == bakery);  // the same place
    CHECK(bakery->produce_amount == 2 * kTon);
    CHECK(bakery->raw_amount == 3 * kTon);
    CHECK(bakery->raw2_amount == kTon);
    CHECK(bakery->interval == 12);
    CHECK(bakery->max_stock == 8 * kTon);
    CHECK(bakery->running_cost == 15);
    CHECK(bakery->idle_cost == 2);
    CHECK(bakery->build_cost.money == 220);
    CHECK(bakery->build_cost.tools == 3 * kTon);
    CHECK(bakery->build_cost.wood == 7 * kTon);
    CHECK(bakery->build_cost.bricks == 4 * kTon);
    CHECK(table.by_id(21503)->produce_amount == kTon);  // untouched

    // The next game without values plays with the original's.
    sim::apply_building_values(table, original, sim::Balance{});
    CHECK(bakery->produce_amount == kTon);
    CHECK(bakery->raw_amount == 2 * kTon);
    CHECK(bakery->raw2_amount == 0);
    CHECK(bakery->interval == 19);
    CHECK(bakery->max_stock == 4 * kTon);
    CHECK(bakery->running_cost == 5);
    CHECK(bakery->idle_cost == 0);
    CHECK(bakery->build_cost.money == 150);
    CHECK(bakery->build_cost.tools == 2 * kTon);
    CHECK(bakery->build_cost.wood == 6 * kTon);
    CHECK(bakery->build_cost.bricks == 0);
}

TEST_CASE("balance: the revised bread chain goes under the packages' values") {
    CHECK(sim::rule_building_values(sim::Rules{}).empty());
    CHECK(!sim::Rules::original().revised_bread);
    CHECK(!sim::Rules::open1602().revised_bread);
    sim::Rules rules;
    rules.revised_bread = true;
    const std::vector<sim::BuildingValue> bread = sim::rule_building_values(rules);
    REQUIRE(bread.size() == 3);
    CHECK((bread[0] == sim::BuildingValue{20503, sim::BuildingField::Produce, 2}));  // bakery
    CHECK((bread[1] == sim::BuildingValue{20503, sim::BuildingField::RunningCost, 15}));
    CHECK((bread[2] == sim::BuildingValue{21503, sim::BuildingField::Produce, 2}));  // grain farm

    // A package that sets the bakery's running cost itself keeps it.
    sim::Balance balance;
    sim::set_building_value(balance, sim::BuildingValue{20503, sim::BuildingField::RunningCost, 10});
    sim::add_building_values(balance, bread);
    REQUIRE(balance.buildings.size() == 3);
    CHECK((balance.buildings[1] == sim::BuildingValue{20503, sim::BuildingField::RunningCost, 10}));
    CHECK(sim::valid(balance));
}

TEST_CASE("balance: a game keeps its building values and the rule in the savegame") {
    const auto buildings = make_buildings();
    sim::World world(60, 40);
    sim::Balance balance;
    sim::set_building_value(balance, sim::BuildingValue{21503, sim::BuildingField::Produce, 2});
    sim::set_building_value(balance, sim::BuildingValue{20503, sim::BuildingField::RunningCost, 15});
    world.set_balance(balance);
    sim::Rules rules;
    rules.revised_bread = true;
    world.set_saved_rules(rules);
    auto bytes = sim::write_game(world, buildings);
    REQUIRE(bytes.ok());
    auto loaded = sim::read_game(*bytes, buildings);
    REQUIRE(loaded.ok());
    CHECK(loaded->balance() == balance);
    REQUIRE(loaded->saved_rules().has_value());
    CHECK(loaded->saved_rules()->revised_bread);

    // Only the building values: BALA stays out, BLDV alone brings them back.
    CHECK(sim::original_numbers(loaded->balance()));

    // A building the data does not know, or a value out of its limits, loads nothing.
    sim::Balance unknown = balance;
    sim::set_building_value(unknown, sim::BuildingValue{29999, sim::BuildingField::Produce, 2});
    world.set_balance(unknown);
    auto with_unknown = sim::write_game(world, buildings);
    REQUIRE(with_unknown.ok());
    CHECK(!sim::read_game(*with_unknown, buildings).ok());
    sim::Balance too_much = balance;
    too_much.buildings[0].value = 5000;  // running cost above 1000
    world.set_balance(too_much);
    auto damaged = sim::write_game(world, buildings);
    REQUIRE(damaged.ok());
    CHECK(!sim::read_game(*damaged, buildings).ok());
}
