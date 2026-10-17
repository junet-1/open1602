// SPDX-License-Identifier: GPL-3.0-or-later
// New endless games (docs/re/worldgen.md, docs/re/newgame.md) with the original data: the islands
// a world plan gets, the start state, and the savegame of a new game.

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/scenario.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/sim/new_game.hpp"
#include "o1602/sim/savegame_writer.hpp"
#include "o1602/sim/world_generator.hpp"

using namespace o1602;

namespace {

assets::GameDirectory game_or_skip() {
    auto dir = assets::GameDirectory::from_environment();
    if (!dir) {
        SKIP("ANNO1602_DIR not set");
    }
    if (!dir->resolve("Szenes/Endlosspiel00.szs")) {
        SKIP("no endless game files");
    }
    return std::move(dir).value();
}

std::uint32_t u32(std::span<const std::uint8_t> d, std::size_t o) {
    return std::uint32_t{d[o]} | (std::uint32_t{d[o + 1]} << 8) | (std::uint32_t{d[o + 2]} << 16) |
           (std::uint32_t{d[o + 3]} << 24);
}

}  // namespace

TEST_CASE("new game: the game id of the original") {
    // game00 was started on 16 October 2026 at 14:06 (TIMERS +0xAC = 0x2A83867A).
    CHECK(sim::new_game_id(2026, 10, 16, 14, 6, 0x7A) == 0x2A83867AU);
    CHECK(sim::endless_file(2, 1) == "Szenes/Endlosspiel21.szs");
}

TEST_CASE("data: the world plan of an endless game gives the original's islands") {
    const auto game = game_or_skip();
    auto buildings = assets::BuildingTable::load(game);
    REQUIRE(buildings.ok());
    auto file = assets::ChunkFile::load(*game.resolve("Szenes/Endlosspiel00.szs"));
    REQUIRE(file.ok());
    REQUIRE(sim::plans_islands(*file));
    auto plan = assets::load_scenario_plan(*file);
    REQUIRE(plan.ok());
    REQUIRE(plan->has_value());
    CHECK((*plan)->islands.size() == 18);
    CHECK((*plan)->natives[0] == 2);
    CHECK((*plan)->ores[2].building == 22405);  // gold mine: 3 units

    // Start index 0 of the table: templates, villages, crops and veins as the original's algorithm
    // gives them (worked example of docs/re/worldgen.md).
    sim::WorldStart start;
    start.seed = 0;
    auto world = sim::generate_islands(*file, game, *buildings, start);
    REQUIRE(world.ok());
    struct Expect {
        std::uint8_t climate;
        std::uint8_t size;
        std::uint16_t template_number;
        bool natives;
        std::set<std::uint8_t> crops;
        std::vector<std::array<std::uint8_t, 4>> ores;  // ware, kind, x, y
    };
    const std::vector<Expect> expected = {
        {0, 0, 11, false, {48}, {}},
        {0, 0, 31, false, {46, 50}, {}},
        {1, 0, 44, false, {47, 49}, {}},
        {1, 0, 48, false, {49, 51}, {}},
        {1, 0, 31, false, {47, 51}, {}},
        {1, 1, 5, true, {49, 51}, {{2, 1, 7, 27}}},
        {1, 1, 41, true, {49, 51}, {}},
        {0, 1, 36, false, {50}, {{2, 0, 22, 29}, {2, 1, 30, 11}}},
        {0, 1, 35, false, {46, 50}, {}},
        {1, 2, 19, false, {47, 49}, {}},
        {0, 2, 11, false, {50}, {{2, 1, 11, 15}, {3, 1, 14, 13}}},
        {0, 2, 9, false, {46, 48}, {}},
        {0, 2, 20, true, {46, 48}, {}},
        {1, 3, 19, false, {47, 51}, {{2, 0, 50, 15}, {2, 1, 45, 7}}},
        {0, 3, 2, true, {46, 50}, {{2, 0, 22, 10}, {2, 1, 30, 23}}},
        {0, 3, 37, false, {46, 48}, {{2, 0, 32, 18}}},
        {1, 3, 17, false, {47, 49}, {{2, 1, 45, 27}}},
        {1, 3, 62, false, {47, 51}, {{2, 0, 22, 14}, {2, 1, 54, 16}}},
    };
    REQUIRE(world->islands.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const sim::GeneratedIsland& g = world->islands[i];
        const Expect& e = expected[i];
        CHECK(g.number == i);
        CHECK(g.climate == e.climate);
        CHECK(g.size == e.size);
        CHECK(g.template_number == e.template_number);
        CHECK(g.natives == e.natives);
        std::set<std::uint8_t> crops;
        for (std::uint8_t w = 46; w <= 51; ++w) {
            if ((g.fertility >> (w - 45)) & 1U) {
                crops.insert(w);
            }
        }
        CHECK(crops == e.crops);
        CHECK((g.fertility & 0x1181U) == 0x1181U);  // grain, grass, trees, fish everywhere
        std::vector<std::array<std::uint8_t, 4>> ores;
        for (const auto& d : g.deposits) {
            if (d.ware == 2 || d.ware == 3) {
                ores.push_back({d.ware, d.kind, d.x, d.y});
            }
        }
        CHECK(ores == e.ores);
    }
    // 2 of the 3 gold units find no free mountain (S-019); one big iron vein no island.
    CHECK(world->lost >= 4);

    // With Rules::guarantee_resources nothing that the plan asks for is missing.
    start.guarantee_resources = true;
    for (std::uint64_t seed = 0; seed < 512; seed += 37) {
        start.seed = seed;
        auto w = sim::generate_islands(*file, game, *buildings, start);
        REQUIRE(w.ok());
        bool gold = false;
        std::set<std::uint8_t> crops;
        for (const auto& g : w->islands) {
            for (const auto& d : g.deposits) {
                gold = gold || d.ware == 3;
            }
            for (std::uint8_t c = 46; c <= 51; ++c) {
                if ((g.fertility >> (c - 45)) & 1U) {
                    crops.insert(c);
                }
            }
        }
        CHECK(gold);
        CHECK(crops.size() == 6);
    }
}

TEST_CASE("data: islands drawn from a seed are the same for the same seed") {
    const auto game = game_or_skip();
    auto buildings = assets::BuildingTable::load(game);
    REQUIRE(buildings.ok());
    auto file = assets::ChunkFile::load(*game.resolve("Szenes/Endlosspiel12.szs"));
    REQUIRE(file.ok());
    sim::WorldStart start;
    start.draws = sim::WorldStart::Draws::Seeded;
    start.seed = 0x1602'0000'1234ULL;
    auto a = sim::generate_islands(*file, game, *buildings, start);
    auto b = sim::generate_islands(*file, game, *buildings, start);
    start.seed += 1;
    auto c = sim::generate_islands(*file, game, *buildings, start);
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    REQUIRE(c.ok());
    REQUIRE(a->file.data().size() == b->file.data().size());
    CHECK(std::equal(a->file.data().begin(), a->file.data().end(), b->file.data().begin()));
    bool differs = false;
    for (std::size_t i = 0; i < a->islands.size() && i < c->islands.size(); ++i) {
        differs = differs || a->islands[i].template_number != c->islands[i].template_number;
    }
    CHECK(differs);
}

TEST_CASE("data: a new endless game and its savegame") {
    const auto game = game_or_skip();
    auto buildings = assets::BuildingTable::load(game);
    auto text = assets::TextTable::load(game);
    REQUIRE(buildings.ok());
    REQUIRE(text.ok());
    sim::NewGameOptions options;
    options.difficulty = 1;
    options.layout = 2;
    options.colour = 2;  // yellow
    options.name = "Merlin";
    options.game_id = 0x2B12345A;
    options.world.seed = 99;
    std::vector<std::string> warnings;
    auto world = sim::start_new_game(game, *buildings, *text, options, &warnings);
    REQUIRE(world.ok());
    CHECK(warnings.empty());
    CHECK(world->islands().size() == 17);  // layout 2
    CHECK(world->time_ms() == 0);
    CHECK(world->game_settings().difficulty == 1);
    CHECK(world->game_settings().flags == 9);  // easy levels: no pirates, ore never runs out
    CHECK(world->next_drought_ms() == 5'400'000);
    // The human in slot 0 with the chosen colour, the computers with the others in order.
    const auto& p = world->players();
    REQUIRE(p.size() == 7);
    CHECK(p[0].role == sim::PlayerRole::Human);
    CHECK(p[0].name == "Merlin");
    CHECK(p[0].money == 15'000);
    CHECK(p[1].role == sim::PlayerRole::Computer);
    CHECK(p[1].name == "Roter Spieler");
    CHECK(p[2].name == "Blauer Spieler");
    CHECK(p[3].money == 15'000);
    CHECK(p[4].money == 1'000'000);
    // Four start ships; the human's with 50 t tools, 30 t wood, 10 t food.
    REQUIRE(world->ships().size() == 4);
    CHECK(world->ships()[0].owner == 0);
    const auto ware = [&](std::uint32_t producer) { return buildings->by_id(producer)->ware; };
    CHECK(world->ships()[0].load(ware(20515)) == 50 * 32);  // tools
    CHECK(world->ships()[0].load(ware(22001)) == 30 * 32);  // wood
    CHECK(world->ships()[0].load(ware(21075)) == 10 * 32);  // food
    CHECK(world->ships()[1].load(ware(22001)) == 50 * 32);
    // Native villages belong to the natives.
    for (const sim::City& c : world->cities()) {
        CHECK(c.owner == 6);
    }

    // Saved, it has the chunks of the original's first save, and loads back to itself.
    auto chunks = sim::savegame_chunks(*world, *buildings);
    REQUIRE(chunks.ok());
    std::vector<std::string> names;
    for (const sim::SavedChunk& c : *chunks) {
        if (c.name != "INSEL5" && c.name != "INSELHAUS" && c.name != "HIRSCH2") {
            names.push_back(c.name);
        }
    }
    CHECK(names == (std::vector<std::string>{"NAME", "PRODLIST2", "WERFT", "MILITAR", "SIEDLER", "ROHWACHS2", "STADT4",
                                             "KONTOR2", "MARKT2", "PLAYER4", "AUFTRAG4", "HANDLER", "SHIP4", "SOLDAT3",
                                             "TURM", "TIMERS"}));
    for (const sim::SavedChunk& c : *chunks) {
        if (c.name == "TIMERS") {
            REQUIRE(c.data.size() == 0x298);
            CHECK(u32(c.data, 0x9C) == 12'800);
            CHECK(u32(c.data, 0xA0) == 0);
            CHECK(c.data[0xA7] == 1);
            CHECK(u32(c.data, 0xAC) == 0x2B12345AU);
            CHECK(u32(c.data, 0xB4) == 54'000);
            CHECK(u32(c.data, 0xB8) == 1'500);
            CHECK(u32(c.data, 0xBC) == 1);
            CHECK(u32(c.data, 0xCC) >= 42'000);
            CHECK(u32(c.data, 0xCC) < 52'000);
        }
        if (c.name == "PLAYER4") {
            CHECK(c.data[4] == 0x00);
            CHECK(c.data[7] == 2);
            CHECK(c.data[0x430 + 4] == 0x0C);
            CHECK(c.data[0x430 + 7] == 0);
            CHECK(c.data[4 * 0x430 + 4] == 0x0D);
        }
        if (c.name == "SHIP4") {
            CHECK(u32(c.data, 0x30) == 0);
        }
        if (c.name == "NAME") {
            CHECK(std::string(c.data.begin(), c.data.end()) == "Endlosspiel");
        }
    }
    auto bytes = sim::write_savegame(*world, *buildings);
    REQUIRE(bytes.ok());
    auto saved = assets::ChunkFile::from_bytes(*bytes);
    REQUIRE(saved.ok());
    auto again = sim::load_world(*saved, *buildings, game);
    REQUIRE(again.ok());
    auto rewritten = sim::savegame_chunks(*again, *buildings);
    REQUIRE(rewritten.ok());
    REQUIRE(rewritten->size() == chunks->size());
    for (std::size_t i = 0; i < chunks->size(); ++i) {
        CHECK((*rewritten)[i].name == (*chunks)[i].name);
        CHECK((*rewritten)[i].data == (*chunks)[i].data);
    }
}
