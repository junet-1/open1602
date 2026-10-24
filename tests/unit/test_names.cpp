// SPDX-License-Identifier: GPL-3.0-or-later
// The names the game shows for players, cities and ships (engine/app/src/names.hpp, MD-10).

#include <string>
#include <vector>

#include "../test_framework.hpp"
#include "names.hpp"

#include "o1602/ui/localization.hpp"

using namespace o1602;

TEST_CASE("names: an empty name is shown in the game's language, a given one as it is (MD-10)") {
    ui::set_language("de");
    sim::Player red;
    red.banner = 0;
    CHECK(app::player_name(red) == ui::tr("player.colour.red"));
    CHECK(app::colour_player_name(1, {"Rot", "Blau"}) == "Blau");
    CHECK(app::colour_player_name(1, {}) == ui::tr("player.colour.blue"));
    CHECK(app::colour_player_name(sim::kFirstExtraBanner, {}) == ui::tr("player.colour.orange"));
    red.name = "Merlin";
    CHECK(app::player_name(red) == "Merlin");

    sim::City city;
    city.island = 2;
    city.index = 0;
    CHECK(app::city_name(city) == ui::tr("city.default", {"3", "1"}));
    city.name = "Neustadt";
    CHECK(app::city_name(city) == "Neustadt");

    sim::Ship ship;
    ship.id = 7;
    CHECK(app::ship_name(ship) == ui::tr("ship.default", {"7"}));
    CHECK(app::ship_name(ship).find('7') != std::string::npos);
    ui::set_language("en");
    CHECK(app::ship_name(ship) == "Ship 7");
}

TEST_CASE("names: players five on of a new world get their colours' names only while unnamed") {
    ui::set_language("en");
    sim::World world;
    std::vector<sim::Player> players(sim::kMaxPlayerSlots);
    for (std::size_t n = sim::kOriginalPlayers; n < sim::kMaxPlayers; ++n) {
        sim::Player& p = players[sim::player_slot(n)];
        p.role = sim::PlayerRole::Computer;
        p.banner = static_cast<sim::BannerId>(sim::kFirstExtraBanner + (n - sim::kOriginalPlayers));
    }
    players[sim::player_slot(sim::kOriginalPlayers + 1)].name = "Player 6";  // named otherwise: kept
    world.set_players(std::move(players));
    app::name_more_players(world);
    CHECK(world.player(sim::player_slot(sim::kOriginalPlayers))->name == "Orange player");
    CHECK(world.player(sim::player_slot(sim::kOriginalPlayers + 1))->name == "Player 6");
}
