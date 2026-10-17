// SPDX-License-Identifier: GPL-3.0-or-later
// The computer players (docs/re/ai.md) in a new endless game with the original data: they found
// their cities and build them up, and a game saved in between goes on exactly the same.

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/sim/new_game.hpp"
#include "o1602/sim/savegame_writer.hpp"
#include "o1602/sim/simulation.hpp"

using namespace o1602;

namespace {

struct Setup {
    assets::GameDirectory game;
    assets::BuildingTable buildings;
    assets::FigureTable figures;
    assets::TextTable text;
};

Setup setup_or_skip() {
    auto dir = assets::GameDirectory::from_environment();
    if (!dir) {
        SKIP("ANNO1602_DIR not set");
    }
    if (!dir->resolve("Szenes/Endlosspiel10.szs")) {
        SKIP("no endless game files");
    }
    auto buildings = assets::BuildingTable::load(*dir);
    auto figures = assets::FigureTable::load(*dir);
    auto text = assets::TextTable::load(*dir);
    REQUIRE(buildings.ok());
    REQUIRE(figures.ok());
    REQUIRE(text.ok());
    return Setup{std::move(dir).value(), std::move(buildings).value(), std::move(figures).value(),
                 std::move(text).value()};
}

sim::World new_game(const Setup& s, std::uint64_t seed) {
    sim::NewGameOptions o;
    o.difficulty = 1;
    o.layout = 0;
    o.name = "Test";
    o.world.seed = seed;
    auto world = sim::start_new_game(s.game, s.buildings, s.text, o);
    REQUIRE(world.ok());
    return std::move(world).value();
}

/// Where two savegames differ first (chunk, and the section of the Open1602 state).
void report(const core::Bytes& one, const core::Bytes& two) {
    auto fa = assets::ChunkFile::from_bytes(one);
    auto fb = assets::ChunkFile::from_bytes(two);
    if (!fa.ok() || !fb.ok()) {
        return;
    }
    for (std::size_t k = 0; k < std::min(fa->chunks().size(), fb->chunks().size()); ++k) {
        const auto x = fa->payload(fa->chunks()[k]);
        const auto y = fb->payload(fb->chunks()[k]);
        if (x.size() == y.size() && std::equal(x.begin(), x.end(), y.begin())) {
            continue;
        }
        std::size_t d = 0;
        while (d < x.size() && d < y.size() && x[d] == y[d]) {
            ++d;
        }
        std::fprintf(stderr, "  chunk %zu %s: %zu / %zu bytes, first difference at %zu\n", k,
                     fa->chunks()[k].name.c_str(), x.size(), y.size(), d);
        if (fa->chunks()[k].name != "O1602") {
            continue;
        }
        std::size_t o = 20;  // magic, version, flags, fingerprint
        while (o + 12 <= x.size()) {
            const std::uint32_t size =
                static_cast<std::uint32_t>(x[o + 8]) | static_cast<std::uint32_t>(x[o + 9]) << 8U |
                static_cast<std::uint32_t>(x[o + 10]) << 16U | static_cast<std::uint32_t>(x[o + 11]) << 24U;
            if (o + 12 + size <= x.size() && o + 12 + size <= y.size()) {
                for (std::size_t i = o; i < o + 12 + size; ++i) {
                    if (x[i] != y[i]) {
                        std::fprintf(stderr, "  section %c%c%c%c differs at %zu\n", x[o], x[o + 1], x[o + 2], x[o + 3],
                                     i - o - 12);
                        break;
                    }
                }
            }
            o += 12 + size;
        }
    }
}

void run_minutes(sim::Simulation& sim, std::uint32_t minutes) {
    for (std::uint32_t t = 0; t < minutes * 60'000 / sim::Simulation::kTickMs; ++t) {
        sim.tick();
    }
}

}  // namespace

TEST_CASE("data: the computers of a new endless game found their cities and build them up") {
    const Setup s = setup_or_skip();
    sim::World world = new_game(s, 7);
    sim::Simulation sim(world, s.buildings, s.figures);
    run_minutes(sim, 30);
    const auto view = sim.computers().view();
    std::size_t computers = 0;
    for (const auto& v : view) {
        const sim::Player* p = world.player(v.player);
        REQUIRE(p != nullptr);
        if (p->role != sim::PlayerRole::Computer) {
            continue;
        }
        ++computers;
        CHECK(v.cities >= 1);
        CHECK(v.max_stage >= 0x14);  // grown out of the founding stage
        CHECK(p->money > 0);
        // Its city: a Kontor, a market, houses with people, a woodcutter and roads.
        const sim::City* city = nullptr;
        for (const sim::City& c : world.cities()) {
            if (c.owner == v.player) {
                city = &c;
            }
        }
        REQUIRE(city != nullptr);
        std::uint32_t people = 0;
        for (const std::uint32_t n : city->inhabitants) {
            people += n;
        }
        CHECK(people >= 20);
        std::size_t houses = 0;
        for (const sim::House& h : world.houses()) {
            houses += h.island == city->island && h.city == city->index ? 1U : 0U;
        }
        CHECK(houses >= 8);
        std::set<std::uint32_t> kinds;
        std::size_t roads = 0;
        const sim::IslandMap& map = world.islands()[city->island].map;
        for (std::uint32_t y = 0; y < map.height(); ++y) {
            for (std::uint32_t x = 0; x < map.width(); ++x) {
                const sim::MapCell& c = map.top(x, y);
                if (c.city != city->index || c.empty()) {
                    continue;
                }
                const assets::BuildingType* b = s.buildings.by_sprite(c.sprite);
                if (b == nullptr) {
                    continue;
                }
                roads += b->kind == 1 ? 1U : 0U;
                kinds.insert(b->id);
            }
        }
        CHECK(kinds.contains(22101));  // Kontor I
        CHECK(kinds.contains(20809));  // market
        CHECK(kinds.contains(22001));  // woodcutter
        CHECK(roads >= 20);
    }
    CHECK(computers == 3);
}

TEST_CASE("data: a game with computer players saved in between goes on exactly the same") {
    const Setup s = setup_or_skip();
    sim::World world = new_game(s, 11);
    sim::Simulation a(world, s.buildings, s.figures);
    run_minutes(a, 12);
    auto saved = sim::write_savegame(world, s.buildings, &a);
    REQUIRE(saved.ok());
    auto file = assets::ChunkFile::from_bytes(*saved);
    REQUIRE(file.ok());
    auto loaded = sim::load_world(*file, s.buildings, s.game);
    REQUIRE(loaded.ok());
    sim::Simulation b(*loaded, s.buildings, s.figures);
    auto again = sim::write_savegame(*loaded, s.buildings, &b);
    REQUIRE(again.ok());
    if (*again != *saved) {
        report(*saved, *again);
    }
    CHECK(*again == *saved);
    run_minutes(a, 4);
    run_minutes(b, 4);
    auto first = sim::write_savegame(world, s.buildings, &a);
    auto second = sim::write_savegame(*loaded, s.buildings, &b);
    REQUIRE(first.ok());
    REQUIRE(second.ok());
    // A city founded during play has no record in the scenario: some bytes of its records keep
    // what the first save wrote (satisfaction 0 or 128 both mean content, the producer of an
    // empty ware). So the cities are compared by their values, everything else byte for byte.
    auto fa = assets::ChunkFile::from_bytes(*first);
    auto fb = assets::ChunkFile::from_bytes(*second);
    REQUIRE(fa.ok());
    REQUIRE(fb.ok());
    REQUIRE(fa->chunks().size() == fb->chunks().size());
    std::size_t differing = 0;
    for (std::size_t k = 0; k < fa->chunks().size(); ++k) {
        const std::string& name = fa->chunks()[k].name;
        if (name == "STADT4" || name == "KONTOR2" || name == "WIFF") {
            continue;
        }
        auto x = fa->payload(fa->chunks()[k]);
        auto y = fb->payload(fb->chunks()[k]);
        if (name == "O1602" && x.size() > 20 && y.size() > 20) {
            x = x.subspan(20);  // without the fingerprint of the chunks above
            y = y.subspan(20);
        }
        if (x.size() != y.size() || !std::equal(x.begin(), x.end(), y.begin())) {
            ++differing;
            report(*first, *second);
        }
    }
    CHECK(differing == 0);
    REQUIRE(world.cities().size() == loaded->cities().size());
    for (std::size_t i = 0; i < world.cities().size(); ++i) {
        const sim::City& x = world.cities()[i];
        const sim::City& y = loaded->cities()[i];
        CHECK(x.island == y.island);
        CHECK(x.owner == y.owner);
        CHECK(x.goods == y.goods);
        CHECK(x.inhabitants == y.inhabitants);
        CHECK(x.satisfaction == y.satisfaction);
        CHECK(x.tax_rate == y.tax_rate);
        REQUIRE(x.trade.size() == y.trade.size());
        for (std::size_t w = 0; w < x.trade.size(); ++w) {
            CHECK(x.trade[w].buy == y.trade[w].buy);
            CHECK(x.trade[w].sell == y.trade[w].sell);
            CHECK(x.trade[w].keep == y.trade[w].keep);
            CHECK(x.trade[w].limit == y.trade[w].limit);
        }
    }
    // The computers kept building meanwhile.
    std::size_t plans = 0;
    for (const auto& v : b.computers().view()) {
        plans += v.plans.size();
    }
    CHECK(plans >= 3);
}
