// SPDX-License-Identifier: GPL-3.0-or-later
// Open1602's own savegame (docs/formats/o1602-save.md): the whole world without the limits of the
// original's format, a checksum, compression, and nothing loaded from a damaged file.

#include <algorithm>

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/core/compress.hpp"
#include "o1602/sim/save_game.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

constexpr std::uint32_t kGround = 20101;
constexpr std::uint32_t kRoad = 20401;
constexpr std::uint32_t kHouse = 20501;

assets::BuildingTable make_buildings(bool with_house = true) {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    std::string cod =
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20101\n  Kind: 11\n  Gfx: 0\n  Size: 1, 1\n"
        "  @Nummer: 1\n  Id: 20401\n  Kind: 1\n  Gfx: 1\n  Size: 1, 1\n";
    if (with_house) {
        cod += "  @Nummer: 2\n  Id: 20501\n  Kind: 14\n  Gfx: 10\n  Size: 2, 2\n";
    }
    cod += "EndObj\n";
    const auto doc = assets::cod::parse(cod, types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

/// A world far beyond the original's limits: 900 x 600 tiles, nine players, 200 ships, 500 units.
sim::World make_world(const assets::BuildingTable& buildings) {
    sim::World world(900, 600);
    for (std::uint16_t i = 0; i < 2; ++i) {
        sim::IslandMap map(30, 20);
        for (std::int32_t y = 0; y < 20; ++y) {
            for (std::int32_t x = 0; x < 30; ++x) {
                REQUIRE(map.place(*buildings.by_id(kGround), x, y, 0, 0).ok());
            }
        }
        map.capture_ground();
        REQUIRE(map.build(*buildings.by_id(kRoad), 3, 4, 0, 0, 1, 0).ok());
        REQUIRE(map.build(*buildings.by_id(kHouse), 5, 4, 1, 2, 1, 0).ok());
        assets::IslandHeader header;
        header.number = static_cast<std::uint8_t>(i);
        header.width = 30;
        header.height = 20;
        header.deposits.push_back({2, 7, 8, 0x101, 0, 900});
        REQUIRE(world.add_island(sim::WorldIsland{header, std::move(map), 100U + 400U * i, 300}).ok());
    }
    std::vector<sim::Player> players(9);
    for (std::size_t p = 0; p < players.size(); ++p) {
        players[p].role = p == 0 ? sim::PlayerRole::Human : sim::PlayerRole::Computer;
        players[p].banner = static_cast<sim::BannerId>(p);
        players[p].money = 1000 * static_cast<std::int32_t>(p);
        players[p].name = "Spieler " + std::to_string(p);
    }
    world.set_players(players);
    sim::City city;
    city.island = 0;
    city.index = 0;
    city.owner = 1;
    city.name = "Neustadt";
    city.goods.assign(30, 64);
    city.purchases = 480 * 75;
    city.sales = 480 * 1234;
    world.add_city(city);
    for (std::uint32_t k = 0; k < 200; ++k) {
        sim::Ship ship;
        ship.x = static_cast<std::uint16_t>(10 + k);
        ship.y = static_cast<std::uint16_t>(50 + k % 7);
        ship.owner = static_cast<sim::PlayerId>(k % 9);
        ship.name = "Schiff " + std::to_string(k);
        ship.cargo.resize(8);
        ship.cargo[0].ware = 23;
        ship.cargo[0].amount = 32 * (k % 50);
        if (k == 3) {
            ship.target = sim::Target::position(60, 60);
            ship.queue = {sim::Target::position(70, 60), sim::Target::position(70, 70)};
        }
        world.add_ship(ship);
    }
    for (std::uint32_t k = 0; k < 500; ++k) {
        sim::Soldier soldier;
        soldier.pos_x = static_cast<std::int32_t>((100 + k % 30) * 4096);
        soldier.pos_y = 300 * 4096;
        soldier.island = 0;
        soldier.owner = static_cast<sim::PlayerId>(k % 9);
        soldier.figure = 1;
        if (k == 7) {
            soldier.queue = {sim::Target::spot(220, 600)};
        }
        world.add_soldier(soldier);
    }
    world.set_time_ms(123'456'789);
    return world;
}

}  // namespace

TEST_CASE("save game: a world beyond the original's limits comes back whole") {
    const auto buildings = make_buildings();
    const sim::World world = make_world(buildings);
    auto bytes = sim::write_game(world, buildings);
    REQUIRE(bytes.ok());
    CHECK(sim::is_open1602_save(*bytes));
    auto info = sim::save_info(*bytes);
    REQUIRE(info.ok());
    CHECK(info->version == 1);
    CHECK(info->compressed == core::has_zlib());
    std::vector<std::string> warnings;
    auto loaded = sim::read_game(*bytes, buildings, &warnings);
    REQUIRE(loaded.ok());
    CHECK(warnings.empty());
    CHECK(loaded->width() == 900);
    CHECK(loaded->height() == 600);
    CHECK(loaded->islands().size() == 2);
    CHECK(loaded->islands()[1].x == 500);
    CHECK(loaded->players().size() == 9);
    CHECK(loaded->players()[8].name == "Spieler 8");
    CHECK(loaded->ships().size() == 200);
    CHECK(loaded->soldiers().size() == 500);
    CHECK(loaded->cities().size() == 1);
    CHECK(loaded->cities()[0].name == "Neustadt");
    CHECK(loaded->cities()[0].purchases == 480 * 75);  // CSTA
    CHECK(loaded->cities()[0].sales == 480 * 1234);
    CHECK(loaded->time_ms() == 123'456'789);
    // Queued orders (QUEU).
    CHECK(loaded->ships()[3].queue == world.ships()[3].queue);
    CHECK(loaded->ships()[3].queue.size() == 2);
    CHECK(loaded->soldiers()[7].queue.size() == 1);
    // Both layers of every tile.
    const sim::IslandMap& a = world.islands()[0].map;
    const sim::IslandMap& b = loaded->islands()[0].map;
    for (std::uint32_t y = 0; y < a.height(); ++y) {
        for (std::uint32_t x = 0; x < a.width(); ++x) {
            CHECK(a.top(x, y).sprite == b.top(x, y).sprite);
            CHECK(a.top(x, y).rotation == b.top(x, y).rotation);
            CHECK(a.top(x, y).owner == b.top(x, y).owner);
            CHECK(a.ground(x, y).sprite == b.ground(x, y).sprite);
        }
    }
    CHECK(loaded->islands()[0].header.deposits.size() == 1);
    CHECK(loaded->islands()[0].header.deposits[0].mask == 0x101U);
    // The format is canonical: saving the loaded world gives the same bytes.
    auto again = sim::write_game(*loaded, buildings);
    REQUIRE(again.ok());
    CHECK(*again == *bytes);
    // Uncompressed it holds the same.
    auto plain = sim::write_game(world, buildings, nullptr, sim::SaveOptions{false});
    REQUIRE(plain.ok());
    auto from_plain = sim::read_game(*plain, buildings);
    REQUIRE(from_plain.ok());
    auto plain_again = sim::write_game(*from_plain, buildings);
    REQUIRE(plain_again.ok());
    CHECK(*plain_again == *bytes);
}

TEST_CASE("save game: a damaged, cut off or newer file loads nothing") {
    const auto buildings = make_buildings();
    const sim::World world = make_world(buildings);
    auto bytes = sim::write_game(world, buildings, nullptr, sim::SaveOptions{false});
    REQUIRE(bytes.ok());
    // One byte of the data changed: the checksum catches it.
    core::Bytes damaged = *bytes;
    damaged[damaged.size() / 2] ^= 0x40;
    auto r1 = sim::read_game(damaged, buildings);
    CHECK(!r1.ok());
    // Cut off.
    core::Bytes cut(bytes->begin(), bytes->end() - 100);
    CHECK(!sim::read_game(cut, buildings).ok());
    CHECK(!sim::read_game(core::Bytes(bytes->begin(), bytes->begin() + 20), buildings).ok());
    // A newer format.
    core::Bytes newer = *bytes;
    newer[8] = 2;
    auto r3 = sim::read_game(newer, buildings);
    REQUIRE(!r3.ok());
    CHECK(r3.error().code == core::ErrorCode::Unsupported);
    // Not a savegame at all.
    CHECK(!sim::is_open1602_save(core::Bytes(64, 0)));
    // A building the game's data does not have.
    const auto without_house = make_buildings(false);
    auto r4 = sim::read_game(*bytes, without_house);
    CHECK(!r4.ok());
}
