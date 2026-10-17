// SPDX-License-Identifier: GPL-3.0-or-later
// Ships of the original savegames (docs/re/ships.md): what SHIP4 and HANDLER hold, and twenty
// minutes at sea with the free traders, the computer players' routes and the pirates' orders.

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/sim/simulation.hpp"

using namespace o1602;

namespace {

assets::GameDirectory game_or_skip(const char* file) {
    auto dir = assets::GameDirectory::from_environment();
    if (!dir) {
        SKIP("ANNO1602_DIR not set");
    }
    if (!dir->resolve(file)) {
        SKIP("savegame missing");
    }
    return std::move(dir).value();
}

}  // namespace

TEST_CASE("data: ships of a savegame with their routes, the trader's goods and the island it lies at") {
    const auto game = game_or_skip("SAVEGAME/game00.gam");
    auto buildings = assets::BuildingTable::load(game);
    REQUIRE(buildings.ok());
    auto file = assets::ChunkFile::load(*game.resolve("SAVEGAME/game00.gam"));
    REQUIRE(file.ok());
    auto world = sim::load_world(*file, *buildings, game);
    REQUIRE(world.ok());
    std::size_t traders = 0;
    std::size_t routes = 0;
    std::set<std::uint32_t> ids;
    for (const sim::Ship& s : world->ships()) {
        ids.insert(s.id);
        CHECK(s.health != 0);
        if (s.trader) {
            ++traders;
            CHECK(s.figure == 29);
            std::uint32_t goods = 0;
            for (const sim::TraderGood& g : s.trader_goods) {
                goods += g.amount;
                CHECK(g.price <= 1023);
            }
            CHECK(goods != 0);
        }
        if (s.route.has_stops()) {
            ++routes;
        }
    }
    CHECK(ids.size() == world->ships().size());  // every ship its own id
    CHECK(traders == 7);                         // (20 Kontors + 2) / 3
    CHECK(routes >= 2);
    // Kontor trade settings of the computer players: food sold at half the base price.
    bool food_offer = false;
    for (const sim::City& c : world->cities()) {
        const sim::City::Trade* t = c.trade_of(14);
        if (t != nullptr && t->sell && t->sell_price == 13) {
            food_offer = true;
        }
    }
    CHECK(food_offer);
}

TEST_CASE("data: twenty minutes at sea: ships stay on water, traders trade, nobody gives up") {
    const auto game = game_or_skip("SAVEGAME/game00.gam");
    for (const char* name : {"SAVEGAME/game00.gam", "SAVEGAME/game03.gam"}) {
        if (!game.resolve(name)) {
            continue;
        }
        auto buildings = assets::BuildingTable::load(game);
        auto figures = assets::FigureTable::load(game);
        REQUIRE(buildings.ok());
        REQUIRE(figures.ok());
        auto file = assets::ChunkFile::load(*game.resolve(name));
        REQUIRE(file.ok());
        auto world = sim::load_world(*file, *buildings, game);
        REQUIRE(world.ok());
        sim::Simulation sim(*world, *buildings, *figures);
        sim.set_speed(8);
        std::size_t traded = 0;
        std::size_t gave_up = 0;
        std::size_t on_land = 0;
        for (int i = 0; i < 20 * 600 / 8; ++i) {
            sim.advance(100);
            for (const sim::ShipEvent& e : sim.take_ship_events()) {
                traded += e.kind == sim::ShipEvent::Kind::Traded ? 1U : 0U;
                // Pirates sail to their hideout, whose narrow harbour another pirate may block;
                // their own behaviour comes with the military.
                const sim::Player* owner = world->player(e.player);
                const bool pirate = owner != nullptr && owner->role == sim::PlayerRole::Pirates;
                gave_up += e.kind == sim::ShipEvent::Kind::GaveUp && !pirate ? 1U : 0U;
            }
            for (const sim::Ship& s : world->ships()) {
                on_land += sim.fleet().sea().sailable(s.x, s.y) ? 0U : 1U;
            }
        }
        std::fprintf(stderr, "  %s: %zu trades\n", name, traded);
        CHECK(traded > 20);
        CHECK(gave_up == 0);
        CHECK(on_land == 0);
    }
}
