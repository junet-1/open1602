// SPDX-License-Identifier: GPL-3.0-or-later
// Values of the balance as data (MD-9, docs/modding/balance.md): the original's defaults, a
// package's balance.toml and the section BALA that keeps them with the game.

#include <string>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/sim/balance.hpp"
#include "o1602/sim/save_game.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20101\n  Kind: 11\n  Gfx: 0\n  Size: 1, 1\n"
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
