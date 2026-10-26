// SPDX-License-Identifier: GPL-3.0-or-later
// Food per minute of the meat and bread chains with the original's haeuser.cod and figuren.cod, and of
// the revised bread chain (Rules::revised_bread, data/balance/revised_bread.toml): the numbers the
// revised chain was balanced on. Meat is not changed by it.

#include <string>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/game_data.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/sim/balance.hpp"
#include "o1602/sim/growth.hpp"
#include "o1602/sim/rules.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

constexpr std::uint32_t kGround = 20101;
constexpr std::uint32_t kRoad = 20401;
constexpr std::uint32_t kPastureRipe = 22702;
constexpr std::uint32_t kWheatGrowing = 21301;
constexpr std::uint32_t kGrainFarm = 21503;
constexpr std::uint32_t kCattleFarm = 21513;
constexpr std::uint32_t kMill = 20501;
constexpr std::uint32_t kBakery = 20503;
constexpr std::uint32_t kButcher = 20511;
constexpr std::uint32_t kMarket = 20809;
constexpr std::uint32_t kKontor = 22101;

constexpr std::int32_t kW = 64;
constexpr std::int32_t kH = 40;
constexpr std::int32_t kRoadY = 20;

struct Site {
    const assets::BuildingTable& buildings;
    sim::World world;
    sim::IslandMap map{kW, kH};
    std::vector<std::pair<std::int32_t, std::int32_t>> productions;
    std::uint32_t seed = 12345;

    Site(const assets::BuildingTable& b, std::uint32_t ground) : buildings(b) {
        for (std::int32_t y = 0; y < kH; ++y) {
            for (std::int32_t x = 0; x < kW; ++x) {
                put(y == kRoadY ? kRoad : ground, x, y);
            }
        }
    }
    std::uint32_t random() {
        seed = seed * 1103515245U + 12345U;
        return (seed >> 16) & 0x7FFF;
    }
    void put(std::uint32_t id, std::int32_t x, std::int32_t y, std::uint8_t phase = 0) {
        const assets::BuildingType* b = buildings.by_id(id);
        REQUIRE(b != nullptr);
        REQUIRE(map.place(*b, x, y, 0, phase, 0, 0).ok());
    }
    void building(std::uint32_t id, std::int32_t x, std::int32_t y) {
        put(id, x, y);
        productions.emplace_back(x, y);
    }
    /// A grain farm at (x, y) with `fields` wheat fields in its ring (radius 1: the 12 tiles around).
    void farm(std::int32_t x, std::int32_t y, int fields) {
        building(kGrainFarm, x, y);
        const std::pair<int, int> ring[] = {{-1, -1}, {0, -1}, {1, -1}, {2, -1}, {-1, 0}, {2, 0},
                                            {-1, 1},  {2, 1},  {-1, 2}, {0, 2},  {1, 2},  {2, 2}};
        for (int i = 0; i < fields && i < 12; ++i) {
            put(kWheatGrowing, x + ring[i].first, y + ring[i].second, static_cast<std::uint8_t>(random() % 5));
        }
    }
    void finish() {
        REQUIRE(world.add_island(sim::WorldIsland{assets::IslandHeader{}, std::move(map), 10, 10}).ok());
        sim::City city;
        city.island = 0;
        city.index = 0;
        city.owner = 0;
        world.add_city(city);
        std::vector<sim::Player> players(1);
        players[0].role = sim::PlayerRole::Human;
        world.set_players(players);
        for (const auto& [x, y] : productions) {
            sim::Production p;
            p.island = 0;
            p.x = static_cast<std::uint16_t>(x);
            p.y = static_cast<std::uint16_t>(y);
            world.set_production(p);
        }
        sim::add_missing_growth(world, buildings);
    }
};

constexpr std::uint32_t kWarmup = 15;
constexpr std::uint32_t kMinutes = 30;

/// Food in t per minute over `minutes` after `warmup` minutes; the warehouse is emptied every minute.
double measure(Site& site, const assets::BuildingTable& buildings, const assets::FigureTable& figures,
               std::uint32_t warmup = kWarmup, std::uint32_t minutes = kMinutes) {
    site.finish();
    sim::World& world = site.world;
    sim::Rules rules = sim::Rules::open1602();
    sim::Simulation simulation(world, buildings, figures, rules);
    const std::uint32_t ticks_per_minute = 60'000 / sim::Simulation::kTickMs;
    std::uint64_t food = 0;
    for (std::uint32_t m = 0; m < warmup + minutes; ++m) {
        for (std::uint32_t t = 0; t < ticks_per_minute; ++t) {
            simulation.tick();
        }
        sim::City& city = *world.city(0, 0);
        city.ensure_ware(assets::ware::kFood);
        if (m >= warmup) {
            food += city.goods[assets::ware::kFood];
        }
        // So that it never fills up; intermediate goods too.
        for (auto& g : city.goods) {
            g = 0;
        }
    }
    return static_cast<double>(food) / assets::kStockUnitsPerTon / minutes;
}

/// Two cattle farms and a butcher: the meat chain the bread chain is held against.
double meat(const assets::BuildingTable& buildings, const assets::FigureTable& figures) {
    Site site(buildings, kPastureRipe);
    site.building(kCattleFarm, 14, 12);
    site.building(kCattleFarm, 20, 12);
    site.building(kButcher, 17, 18);
    site.building(kKontor, 24, 17);
    site.building(kMarket, 27, 17);
    return measure(site, buildings, figures);
}

/// `farms` grain farms with `fields` wheat fields each, two mills and a bakery.
double bread(const assets::BuildingTable& buildings, const assets::FigureTable& figures, int farms, int fields) {
    Site site(buildings, kGround);
    const std::pair<int, int> at[] = {{22, 15}, {27, 15}, {22, 10}, {27, 10}};
    for (int f = 0; f < farms; ++f) {
        site.farm(at[f].first, at[f].second, fields);
    }
    site.building(kMill, 24, 18);
    site.building(kMill, 27, 18);
    site.building(kBakery, 30, 18);
    site.building(kKontor, 33, 17);
    site.building(kMarket, 36, 17);
    return measure(site, buildings, figures);
}

}  // namespace

TEST_CASE("data: the revised bread chain makes as much food as meat at the same running cost") {
    auto dir = assets::GameDirectory::from_environment();
    if (!dir) {
        SKIP("ANNO1602_DIR not set");
    }
    auto original = assets::BuildingTable::load(*dir);
    auto figures = assets::FigureTable::load(*dir);
    REQUIRE(original.ok());
    REQUIRE(figures.ok());

    // The original: 2.4 t of meat a minute for 15 gold, 1.5 t of bread for 35 (4 farms with 20 fields).
    const double meat_today = meat(*original, *figures);
    const double bread_today = bread(*original, *figures, 4, 5);
    CHECK(meat_today > 2.2);
    CHECK(meat_today < 2.6);
    CHECK(bread_today > 1.3);
    CHECK(bread_today < 1.8);

    sim::Rules rules = sim::Rules::open1602();
    rules.revised_bread = true;
    sim::Balance balance;
    sim::add_building_values(balance, sim::rule_building_values(rules));
    REQUIRE(sim::valid(balance));
    assets::BuildingTable revised = *original;
    sim::apply_building_values(revised, *original, balance);
    REQUIRE(revised.by_id(kBakery) != nullptr);
    CHECK(revised.by_id(kBakery)->running_cost == 15);

    // Revised: meat stays as it is; 3 farms with 21 fields make about 6.3 t of bread for 40 gold,
    // the same 6.3 gold per ton as meat.
    CHECK(meat(revised, *figures) == meat_today);
    const double bread_revised = bread(revised, *figures, 3, 7);
    CHECK(bread_revised > 5.5);
    CHECK(bread_revised < 7.0);
}
