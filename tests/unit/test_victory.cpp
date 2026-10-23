// SPDX-License-Identifier: GPL-3.0-or-later
// Victory conditions of the endless game (sim/victory.hpp): what each condition asks for, how
// they combine, the time limit, and that they stay with the savegame.

#include <algorithm>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/sim/save_game.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/sim/victory.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;
using sim::GameEvent;

namespace {

constexpr std::uint32_t kGround = 20101;

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

assets::FigureTable make_figures() {
    const assets::CodPredefinedSymbol types[] = {{"FIGUR", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: FIGUR\n"
        "  Nummer: 21\n  Gfx: 0\n  Blocknr: 2\n  Rotate: 1\n  Maxware: 4\n  Maxkanon: 6\n  Speed: 500\n"
        "  Drehtime: 0.13\n  Maxenergy: 50\n  Hitpoint: 2.0\n  Shotradius: 7\n  Shottime: 1.0\n  Worktime: 5.0\n"
        "EndObj\n",
        types);
    auto table = assets::FigureTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

/// Four islands of 10 x 10 tiles. Players: 0 human, 1 computer, 2 computer, 3 natives; every
/// player but the natives with a ship, so nobody is out at the first check.
struct Game {
    assets::BuildingTable buildings = make_buildings();
    assets::FigureTable figures = make_figures();
    sim::World world{160, 60};

    Game() {
        for (std::uint16_t i = 0; i < 4; ++i) {
            sim::IslandMap map(10, 10);
            for (std::int32_t y = 0; y < 10; ++y) {
                for (std::int32_t x = 0; x < 10; ++x) {
                    REQUIRE(map.place(*buildings.by_id(kGround), x, y, 0, 0).ok());
                }
            }
            assets::IslandHeader header;
            header.number = static_cast<std::uint8_t>(i);
            header.width = 10;
            header.height = 10;
            REQUIRE(world.add_island(sim::WorldIsland{header, std::move(map), 10U + 30U * i, 20}).ok());
        }
        std::vector<sim::Player> players(4);
        players[0].role = sim::PlayerRole::Human;
        players[1].role = sim::PlayerRole::Computer;
        players[2].role = sim::PlayerRole::Computer;
        sim::make_neutral(players[3], sim::faction::kNatives);
        for (sim::Player& p : players) {
            p.money = 10'000;
        }
        world.set_players(players);
        for (sim::PlayerId p = 0; p < 3; ++p) {
            ship(p);
        }
    }

    void city(std::uint16_t island, std::uint8_t index, sim::PlayerId owner, std::vector<std::uint32_t> inh) {
        sim::City c;
        c.island = island;
        c.index = index;
        c.owner = owner;
        c.inhabitants = std::move(inh);
        c.inhabitants.resize(5, 0);
        world.add_city(c);
    }
    void ship(sim::PlayerId owner) {
        sim::Ship s;
        s.owner = owner;
        s.x = 5;
        s.y = 5;
        s.figure = 21;
        world.add_ship(s);
    }
    [[nodiscard]] sim::VictoryCheck check(const sim::VictoryConditions& v, sim::PlayerId p) const {
        return sim::check_victory(world, buildings, figures, v, p);
    }
};

std::vector<GameEvent> run(sim::Simulation& s, std::uint32_t ms) {
    std::vector<GameEvent> events;
    for (std::uint32_t t = 0; t < ms; t += sim::Simulation::kTickMs) {
        s.tick();
        for (const GameEvent& e : s.take_game_events()) {
            events.push_back(e);
        }
    }
    return events;
}

const GameEvent* find(const std::vector<GameEvent>& events, GameEvent::Kind kind) {
    const auto e = std::find_if(events.begin(), events.end(), [&](const GameEvent& x) { return x.kind == kind; });
    return e != events.end() ? &*e : nullptr;
}

sim::Rules with(const sim::VictoryConditions& v) {
    sim::Rules r;
    r.victory = v;
    return r;
}

}  // namespace

TEST_CASE("victory: without conditions the endless game never ends") {
    Game g;
    g.city(0, 0, 0, {5000, 5000, 5000, 5000, 5000});
    g.world.player(0)->money = 5'000'000;
    CHECK(!sim::Rules{}.victory.active());
    CHECK(sim::Rules::original().victory == sim::VictoryConditions{});
    sim::Simulation s(g.world, g.buildings, g.figures);
    const auto events = run(s, 30'000);
    CHECK(find(events, GameEvent::Kind::GameOver) == nullptr);
    CHECK(!g.world.game_end().has_value());
}

TEST_CASE("victory: each condition asks for what it says") {
    Game g;
    g.city(0, 0, 0, {100, 200, 300, 0, 50});  // 650 inhabitants, 350 citizens or higher, 50 aristocrats
    g.city(1, 0, 0, {100});
    g.city(2, 0, 1, {100});

    sim::VictoryConditions v;
    v.inhabitants = 750;
    CHECK(g.check(v, 0).won);
    CHECK(g.check(v, 0).required == sim::victory::kInhabitants);
    v.inhabitants = 751;
    CHECK(!g.check(v, 0).won);

    v = {};
    v.resident_class = 2;  // citizens
    v.class_inhabitants = 350;
    CHECK(g.check(v, 0).won);
    v.resident_class = 4;  // aristocrats
    CHECK(!g.check(v, 0).won);
    v.class_inhabitants = 50;
    CHECK(g.check(v, 0).won);

    v = {};
    v.money = 10'000;
    CHECK(g.check(v, 0).won);
    g.world.player(0)->money = -5;
    CHECK(!g.check(v, 0).won);

    v = {};
    v.islands_percent = 50;  // two of four islands
    CHECK(g.check(v, 0).won);
    CHECK(!g.check(v, 1).won);  // one of four
    v.islands_percent = 51;
    CHECK(!g.check(v, 0).won);

    v = {};
    v.monument = sim::Monument::Both;
    g.world.player(0)->castles_built = 1;
    CHECK(!g.check(v, 0).won);
    g.world.player(0)->cathedrals_built = 1;
    CHECK(g.check(v, 0).won);

    v = {};
    v.score = 1;
    CHECK(g.check(v, 0).won);
    v.score = 1'000'000'000;
    CHECK(!g.check(v, 0).won);

    v = {};
    v.eliminate = true;
    CHECK(!g.check(v, 0).won);
    g.world.player(1)->status = sim::PlayerStatus::Defeated;
    CHECK(!g.check(v, 0).won);
    g.world.player(2)->status = sim::PlayerStatus::Absent;  // never took part: not an opponent to beat
    CHECK(g.check(v, 0).won);
    CHECK(!g.check(v, 1).won);  // defeated players win nothing
}

TEST_CASE("victory: all conditions at once, or any one of them") {
    Game g;
    g.city(0, 0, 0, {1000});
    sim::VictoryConditions v;
    v.inhabitants = 1000;
    v.money = 1'000'000;
    const sim::VictoryCheck all = g.check(v, 0);
    CHECK(all.required == (sim::victory::kInhabitants | sim::victory::kMoney));
    CHECK(all.achieved == sim::victory::kInhabitants);
    CHECK(!all.won);
    v.all = false;
    CHECK(g.check(v, 0).won);
    g.world.player(0)->money = 1'000'000;
    v.all = true;
    CHECK(g.check(v, 0).won);
}

TEST_CASE("victory: the first to meet the conditions wins; the others lose") {
    Game g;
    g.city(0, 0, 0, {500});
    g.city(1, 0, 1, {2000});
    sim::VictoryConditions v;
    v.inhabitants = 1000;
    sim::Simulation s(g.world, g.buildings, g.figures, with(v));
    const auto events = run(s, 5000);
    const GameEvent* won = find(events, GameEvent::Kind::MissionAccomplished);
    REQUIRE(won != nullptr);
    CHECK(won->player == 1);
    CHECK(won->message == 0x54);  // one human: "you have fulfilled your task"
    const GameEvent* over = find(events, GameEvent::Kind::GameOver);
    REQUIRE(over != nullptr);
    CHECK(over->player == 1);
    REQUIRE(g.world.game_end().has_value());
    CHECK(g.world.game_end()->winner == 1);
    CHECK(g.world.players()[1].won);
    CHECK(!g.world.players()[0].won);
    // Over is over: no second winner later.
    g.city(2, 0, 0, {5000});
    CHECK(find(run(s, 10'000), GameEvent::Kind::MissionAccomplished) == nullptr);
}

TEST_CASE("victory: computers can be kept from winning") {
    Game g;
    g.city(1, 0, 1, {2000});
    sim::VictoryConditions v;
    v.inhabitants = 1000;
    v.computers = false;
    CHECK(!sim::can_win(g.world, v, 1));
    CHECK(sim::can_win(g.world, v, 0));
    CHECK(!sim::can_win(g.world, v, 3));  // the natives never
    sim::Simulation s(g.world, g.buildings, g.figures, with(v));
    CHECK(find(run(s, 10'000), GameEvent::Kind::GameOver) == nullptr);
}

TEST_CASE("victory: with several humans the winner's colour is announced") {
    Game g;
    g.world.player(1)->role = sim::PlayerRole::Human;
    g.city(0, 0, 0, {2000});
    g.city(1, 0, 1, {2000});  // both at the same moment: both win
    sim::VictoryConditions v;
    v.inhabitants = 1000;
    sim::Simulation s(g.world, g.buildings, g.figures, with(v));
    const auto events = run(s, 5000);
    std::vector<sim::PlayerId> winners;
    for (const GameEvent& e : events) {
        if (e.kind == GameEvent::Kind::MissionAccomplished) {
            winners.push_back(e.player);
            CHECK(e.message == 0x6B);
        }
    }
    CHECK((winners == std::vector<sim::PlayerId>{0, 1}));
    const GameEvent* over = find(events, GameEvent::Kind::GameOver);
    REQUIRE(over != nullptr);
    CHECK(over->player == 0);
    CHECK(over->message == 0x6B);
}

TEST_CASE("victory: when the time is up the most points win") {
    Game g;
    g.city(0, 0, 0, {300});
    g.city(1, 0, 1, {900});
    sim::VictoryConditions v;
    v.time_limit_minutes = 1;
    CHECK(v.active());
    CHECK(!v.goals());
    CHECK(!sim::time_limit_over(v, 59'999));
    CHECK(sim::time_limit_over(v, 60'000));
    CHECK(sim::time_limit_winner(g.world, g.buildings, v) == 1);
    v.computers = false;
    CHECK(sim::time_limit_winner(g.world, g.buildings, v) == 0);
    v.computers = true;

    sim::Simulation s(g.world, g.buildings, g.figures, with(v));
    auto events = run(s, 55'000);
    CHECK(find(events, GameEvent::Kind::GameOver) == nullptr);
    events = run(s, 5000);
    const GameEvent* won = find(events, GameEvent::Kind::MissionAccomplished);
    REQUIRE(won != nullptr);
    CHECK(won->player == 1);
    CHECK(std::count_if(events.begin(), events.end(), [](const GameEvent& e) {
              return e.kind == GameEvent::Kind::MissionAccomplished;
          }) == 1);  // one winner, whatever else holds
    REQUIRE(g.world.game_end().has_value());
    CHECK(g.world.game_end()->time_ms == 60'000);
}

TEST_CASE("victory: equal points at the time limit go to the first slot") {
    Game g;
    g.city(1, 0, 1, {400});
    g.city(2, 0, 2, {400});
    sim::VictoryConditions v;
    v.time_limit_minutes = 30;
    g.world.player(0)->status = sim::PlayerStatus::Defeated;
    g.world.player(0)->money = 0;
    g.world.player(1)->money = 0;
    g.world.player(2)->money = 0;
    CHECK(sim::time_limit_winner(g.world, g.buildings, v) == 1);
}

TEST_CASE("victory: the conditions stay with the savegame") {
    Game g;
    sim::Rules rules;
    sim::VictoryConditions& v = rules.victory;
    v.inhabitants = 12'345;
    v.resident_class = 3;
    v.class_inhabitants = 678;
    v.money = 2'000'000;
    v.balance = 750;
    v.eliminate = true;
    v.islands_percent = 66;
    v.score = 42'000;
    v.monument = sim::Monument::Cathedral;
    v.time_limit_minutes = 90;
    v.all = false;
    v.computers = false;
    g.world.set_saved_rules(rules);
    auto bytes = sim::write_game(g.world, g.buildings);
    REQUIRE(bytes.ok());
    auto back = sim::read_game(*bytes, g.buildings);
    REQUIRE(back.ok());
    REQUIRE(back->saved_rules().has_value());
    CHECK(back->saved_rules()->victory == v);
    // And the simulation of the loaded game plays by them.
    sim::Simulation s(*back, g.buildings, g.figures);
    CHECK(s.rules().victory == v);

    // A game without them keeps them off.
    g.world.set_saved_rules(sim::Rules{});
    bytes = sim::write_game(g.world, g.buildings);
    REQUIRE(bytes.ok());
    back = sim::read_game(*bytes, g.buildings);
    REQUIRE(back.ok());
    REQUIRE(back->saved_rules().has_value());
    CHECK(back->saved_rules()->victory == sim::VictoryConditions{});
}
