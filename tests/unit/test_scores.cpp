// SPDX-License-Identifier: GPL-3.0-or-later
// Scores, high score lists, campaign progress and the end screen's figures
// (docs/re/missions.md).

#include <cstring>

#include "../test_framework.hpp"

#include "o1602/assets/chunk_writer.hpp"
#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/high_scores.hpp"
#include "o1602/sim/score.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

/// Two classes with Hiscore 1 and 3.
assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}, {"BGRUPPE", 5}};
    const auto doc = assets::cod::parse(
        "Objekt: BGRUPPE\n"
        "  Nummer: 0\n  Prozent: 0\n  Maxwohn: 2\n  Steuer: 1.4\n  Hiscore: 1\n"
        "  Nummer: 1\n  Prozent: 47\n  Maxwohn: 6\n  Steuer: 1.6\n  Hiscore: 3\n"
        "EndObj\n"
        "Objekt: HAUS\n  Nummer: 0\n  Id: 20101\n  Gfx: 0\nEndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

void add_city(sim::World& world, std::uint8_t index, sim::PlayerId owner, std::vector<std::uint32_t> inhabitants) {
    sim::City c;
    c.island = 0;
    c.index = index;
    c.owner = owner;
    c.inhabitants = std::move(inhabitants);
    world.add_city(c);
}

assets::HighScore line(std::string name, std::int64_t points, std::uint32_t game = 0) {
    assets::HighScore h;
    h.name = std::move(name);
    h.points = points;
    h.game_id = game;
    return h;
}

}  // namespace

TEST_CASE("scores: inhabitants weighted by class, doubled for the winner") {
    const assets::BuildingTable buildings = make_buildings();
    sim::World world(100, 100);
    std::vector<sim::Player> players(3);
    players[1].role = sim::PlayerRole::Computer;
    world.set_players(players);
    add_city(world, 0, 0, {10, 20});
    add_city(world, 1, 0, {5, 0});
    add_city(world, 2, 1, {100, 100});

    sim::ScoreParts s = sim::score_parts(world, buildings, 0);
    CHECK(s.inhabitants == 15 * 1 + 20 * 3);
    CHECK(s.total == 75);
    CHECK(!s.won);
    CHECK(sim::player_score(world, buildings, 1) == 400);
    CHECK(sim::player_score(world, buildings, 2) == 0);  // no city
    CHECK(sim::player_score(world, buildings, 9) == 0);  // no such player

    world.player(0)->won = true;
    s = sim::score_parts(world, buildings, 0);
    CHECK(s.won);
    CHECK(s.subtotal == 75);
    CHECK(s.total == 150);
}

TEST_CASE("scores: the game's name, stars and id come from its source") {
    sim::World world(100, 100);
    CHECK(sim::game_identity(world).name.empty());  // no source

    assets::ChunkWriter w;
    const char name[] = "Das Atoll";
    w.add("NAME", std::span(reinterpret_cast<const std::uint8_t*>(name), sizeof(name)));
    const std::array<std::uint8_t, 4> stars{3, 0, 0, 0};
    w.add("SZENE_RANKING", stars);
    const std::array<std::uint8_t, 4> number{5, 0, 0, 0};
    w.add("SZENE_MISSNR", number);
    std::vector<std::uint8_t> timers(0x298, 0);
    const std::uint32_t game = 713262714U;
    std::memcpy(timers.data() + 0xAC, &game, 4);  // little-endian on every supported platform
    w.add("TIMERS", timers);
    auto file = assets::ChunkFile::from_bytes(w.take());
    REQUIRE(file.ok());
    world.set_source(std::make_shared<const assets::ChunkFile>(std::move(file).value()));

    const sim::GameIdentity id = sim::game_identity(world);
    CHECK(id.name == "Das Atoll");
    CHECK(id.stars == 3);
    CHECK(id.mission_number == 5);
    CHECK(id.campaign == sim::GameIdentity::kNone);
    CHECK(id.game_id == game);
}

TEST_CASE("scores: the end screen lists humans and computers, the local player first") {
    const assets::BuildingTable buildings = make_buildings();
    sim::World world(100, 100);
    std::vector<sim::Player> players(5);
    players[0].name = "Merlin";
    players[1].role = sim::PlayerRole::Computer;
    players[1].name = "Ming";
    players[2].role = sim::PlayerRole::Human;
    players[2].status = sim::PlayerStatus::Absent;
    players[3].role = sim::PlayerRole::Pirates;
    players[4].role = sim::PlayerRole::Computer;
    players[4].status = sim::PlayerStatus::Defeated;
    world.set_players(players);
    add_city(world, 0, 0, {10, 0});
    add_city(world, 1, 1, {100, 0});
    add_city(world, 2, 1, {0, 10});
    sim::Ship ship;
    ship.owner = 1;
    world.add_ship(ship);

    const sim::EndScreenSummary s = sim::end_screen_summary(world, buildings, 0, sim::GameOutcome::Lost);
    CHECK(s.outcome == sim::GameOutcome::Lost);
    REQUIRE(s.rows.size() == 3);  // not the absent slot, not the pirates
    CHECK(s.rows[0].player == 0);
    CHECK(s.rows[0].name == "Merlin");
    CHECK(s.rows[0].score == 10);
    CHECK(s.rows[1].player == 1);
    CHECK(s.rows[1].cities == 2);
    CHECK(s.rows[1].ships == 1);
    CHECK(s.rows[1].inhabitants == 110);
    CHECK(s.rows[1].score == 130);
    CHECK(s.rows[2].player == 4);
    CHECK(s.rows[2].status == sim::PlayerStatus::Defeated);
}

TEST_CASE("high scores: best first, twelve lines, one line per game") {
    std::vector<assets::HighScore> list;
    CHECK(assets::insert_high_score(list, line("A", 100)) == 0U);
    CHECK(assets::insert_high_score(list, line("B", 300)) == 0U);
    CHECK(assets::insert_high_score(list, line("C", 100)) == 2U);  // after the equal one
    REQUIRE(list.size() == 3);
    CHECK(list[0].name == "B");
    CHECK(list[1].name == "A");
    CHECK(list[2].name == "C");

    // The same game again: a better result replaces its line, a worse one leaves it.
    CHECK(assets::insert_high_score(list, line("D", 200, 7)) == 1U);
    CHECK(assets::insert_high_score(list, line("D", 150, 7)) == 1U);
    CHECK(list[1].points == 200);
    CHECK(assets::insert_high_score(list, line("D", 400, 7)) == 0U);
    CHECK(list.size() == 4);
    CHECK(list[0].points == 400);

    for (int i = 0; i < 20; ++i) {
        (void)assets::insert_high_score(list, line("E", 1000 + i));
    }
    CHECK(list.size() == assets::kHighScoreLines);
    CHECK(list.front().points == 1019);
    CHECK(!assets::insert_high_score(list, line("F", 1)).has_value());  // not good enough
}

TEST_CASE("high scores: written as the original's .hss and read back") {
    std::vector<assets::HighScore> list{line("Merlin", 381, 713262714U), line("Ming \"der\" Gro\xdf", 12)};
    list[0].stars = 2;
    const std::string text = assets::write_high_scores(list);
    CHECK(text.find("Objekt:   HISCORE") != std::string::npos);
    CHECK(text.find("Entry:    0, \"Merlin\", 381, 2, 713262714") != std::string::npos);
    const auto back = assets::parse_high_scores(text);
    REQUIRE(back.size() == assets::kHighScoreLines);  // padded with empty lines
    CHECK(back[0].name == "Merlin");
    CHECK(back[0].points == 381);
    CHECK(back[0].stars == 2);
    CHECK(back[0].game_id == 713262714U);
    CHECK(back[1].name == "Ming 'der' Gro\xdf");  // a quote would end the name
    CHECK(back[2].name.empty());

    // Empty lines of a read list make no difference when a line goes in.
    std::vector<assets::HighScore> again = back;
    CHECK(assets::insert_high_score(again, line("New", 100)) == 1U);
    CHECK(again.size() == 3);
}

TEST_CASE("campaign progress: wins open the next mission") {
    assets::CampaignProgress p;
    CHECK(p.numbered == 2);
    CHECK(p.record_win(0xFF, 0, 2));
    CHECK(p.numbered == 3);
    CHECK(!p.record_win(0xFF, 0, 2));  // an earlier mission again
    CHECK(p.record_win(0xFF, 0, 5));   // a later one the menu offered
    CHECK(p.numbered == 6);
    CHECK(!p.record_win(0xFF, 0, 1));  // the endless game opens nothing
    p.numbered = 17;
    CHECK(!p.record_win(0xFF, 0, 17));  // at most 17
    CHECK(p.numbered == 17);

    CHECK(p.record_win(2, 1, 0));
    CHECK(p.campaigns.at(2) == 2);
    CHECK(!p.record_win(2, 0, 0));
    CHECK(p.campaigns.at(2) == 2);

    const assets::CampaignProgress back = assets::parse_progress(assets::write_progress(p));
    CHECK(back.numbered == 17);
    CHECK(back.campaigns == p.campaigns);

    const assets::CampaignProgress odd = assets::parse_progress("# x\nnumbered 99\ncampaign -1 3\njunk\n");
    CHECK(odd.numbered == 17);
    CHECK(odd.campaigns.empty());
}
