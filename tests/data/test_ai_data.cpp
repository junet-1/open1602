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
#include "o1602/sim/save_game.hpp"
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

/// The sections of two uncompressed Open1602 savegames that differ.
void report(const core::Bytes& one, const core::Bytes& two) {
    const auto sections = [](const core::Bytes& b) {
        std::vector<std::pair<std::string, std::span<const std::uint8_t>>> out;
        std::size_t o = 32;
        while (o + 12 <= b.size()) {
            const std::uint32_t size =
                static_cast<std::uint32_t>(b[o + 8]) | static_cast<std::uint32_t>(b[o + 9]) << 8U |
                static_cast<std::uint32_t>(b[o + 10]) << 16U | static_cast<std::uint32_t>(b[o + 11]) << 24U;
            out.emplace_back(
                std::string(b.begin() + static_cast<std::ptrdiff_t>(o), b.begin() + static_cast<std::ptrdiff_t>(o) + 4),
                std::span<const std::uint8_t>(b).subspan(o + 12, std::min<std::size_t>(size, b.size() - o - 12)));
            o += 12 + size;
        }
        return out;
    };
    const auto a = sections(one);
    const auto b = sections(two);
    for (std::size_t k = 0; k < std::min(a.size(), b.size()); ++k) {
        const auto& x = a[k].second;
        const auto& y = b[k].second;
        if (a[k].first != b[k].first || x.size() != y.size() || !std::equal(x.begin(), x.end(), y.begin())) {
            std::size_t d = 0;
            while (d < x.size() && d < y.size() && x[d] == y[d]) {
                ++d;
            }
            std::fprintf(stderr, "  section %s: %zu / %zu bytes, first difference at %zu\n", a[k].first.c_str(),
                         x.size(), y.size(), d);
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
    auto saved = sim::write_game(world, s.buildings, &a);
    REQUIRE(saved.ok());
    auto loaded = sim::read_game(*saved, s.buildings);
    REQUIRE(loaded.ok());
    sim::Simulation b(*loaded, s.buildings, s.figures);
    run_minutes(a, 4);
    run_minutes(b, 4);
    auto first = sim::write_game(world, s.buildings, &a);
    auto second = sim::write_game(*loaded, s.buildings, &b);
    REQUIRE(first.ok());
    REQUIRE(second.ok());
    CHECK(*first == *second);
    // The computers kept building meanwhile.
    std::size_t plans = 0;
    for (const auto& v : b.computers().view()) {
        plans += v.plans.size();
    }
    CHECK(plans >= 3);
    // The same through a savegame in the original's format with Open1602's chunk: the cities'
    // records keep some bytes of the scenario (a city founded during play has none), so only the
    // game itself is compared, through Open1602's own savegame.
    auto legacy = sim::write_savegame(world, s.buildings, &a);
    REQUIRE(legacy.ok());
    auto file = assets::ChunkFile::from_bytes(*legacy);
    REQUIRE(file.ok());
    auto reloaded = sim::load_world(*file, s.buildings, s.game);
    REQUIRE(reloaded.ok());
    sim::Simulation c(*reloaded, s.buildings, s.figures);
    run_minutes(a, 2);
    run_minutes(c, 2);
    auto third = sim::write_game(world, s.buildings, &a, sim::SaveOptions{false});
    auto fourth = sim::write_game(*reloaded, s.buildings, &c, sim::SaveOptions{false});
    REQUIRE(third.ok());
    REQUIRE(fourth.ok());
    if (*third != *fourth) {
        report(*third, *fourth);
    }
    CHECK(*third == *fourth);
}
