// SPDX-License-Identifier: GPL-3.0-or-later
// The status line over the island tile under the mouse (engine/app/src/tile_status.hpp).

#include <string>

#include "../test_framework.hpp"
#include "tile_status.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

constexpr sim::PlayerId kMe = 0;

assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20101\n  Kind: 11\n  Gfx: 0\n  Size: 1, 1\n"
        // The sea of a fishing ground: fished and growing back, then ripe with fish.
        "  @Nummer: 1\n  Id: 21251\n  Kind: 19\n  Gfx: 1\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 10\n    Interval: 450\n  EndObj\n"
        "  @Nummer: 2\n  Id: 21201\n  Kind: 19\n  Gfx: 2\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 9\n    Ware: 57\n  EndObj\n"
        "  @Nummer: 3\n  Id: 20201\n  Kind: 10\n  Gfx: 3\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 10\n    Interval: 900\n  EndObj\n"
        "  @Nummer: 4\n  Id: 20202\n  Kind: 10\n  Gfx: 4\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 9\n    Ware: 53\n  EndObj\n"
        // A withered field: the record after it is the next crop's growing one.
        "  @Nummer: 5\n  Id: 20301\n  Kind: 11\n  Gfx: 5\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 10\n    Interval: 300\n  EndObj\n"
        "  @Nummer: 6\n  Id: 20302\n  Kind: 11\n  Gfx: 6\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 10\n    Interval: 300\n  EndObj\n"
        // A workshop of the build menu's HANDWERK list (third entry).
        "  @Nummer: 7\n  Id: 20501\n  Kind: 14\n  Gfx: 7\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 1\n  EndObj\n"
        // A house.
        "  @Nummer: 8\n  Id: 20901\n  Kind: 14\n  Gfx: 8\n  Size: 1, 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 13\n  EndObj\n"
        "EndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

assets::TextTable make_texts() {
    std::string game = "[GAME]\n";
    for (int i = 0; i < 40; ++i) {
        game += i == 22 ? "Geeignet für: %s: %d%% - %s: %d%% - %s: %d%%\n" : i == 37 ? "Verdorrtes Feld\n" : "-\n";
    }
    return assets::TextTable::parse(game +
                                    "[END]\n"
                                    "[ROHST]\nGetreide\nTabak\nGewürze\nZuckerrohr\nBaumwolle\nWeintrauben\nKakao\n"
                                    "Wiese\nBaum\nSteine\nErze\nWild\nFische\n[END]\n"
                                    "[ROHSTFELD]\nGetreidefeld\nTabakfeld\nGewürzfeld\nZuckerrohrfeld\nBaumwollfeld\n"
                                    "Weintrauben\nKakaofeld\nWiese\nWald\nSteine\nErze\nWild\nFischgründe\n[END]\n"
                                    "[HANDWERK]\nSteinmetz\nWerkzeugschmiede\nWeberei\n[END]\n"
                                    "[WOHNHAUS]\nHaus\n[END]\n");
}

/// An island of one row: ground, fished sea, sea with fish, forest, withered field, Weberei, house.
sim::WorldIsland make_island(const assets::BuildingTable& buildings) {
    sim::IslandMap map(7, 1);
    const std::uint32_t ids[] = {20101, 21251, 21201, 20202, 20301, 20501, 20901};
    for (std::int32_t x = 0; x < 7; ++x) {
        REQUIRE(map.place(*buildings.by_id(ids[static_cast<std::size_t>(x)]), x, 0, 0, 0, 0, 0).ok());
    }
    map.capture_ground();
    assets::IslandHeader header;
    header.climate = 0;
    header.fertility = 1U << (48 - 45);  // sugar cane
    sim::WorldIsland island{header, std::move(map), 0, 0};
    island.discover(kMe);
    return island;
}

struct Island {
    assets::BuildingTable buildings = make_buildings();
    assets::TextTable texts = make_texts();
    sim::WorldIsland island = make_island(buildings);

    [[nodiscard]] std::string at(std::int32_t x) const { return app::tile_status(island, x, 0, kMe, buildings, texts); }
};

}  // namespace

TEST_CASE("tile status: fishing grounds, ripe or fished, are named") {
    const Island i;
    CHECK(i.at(2) == "Fischgründe");
    CHECK(i.at(1) == "Fischgründe");  // the ripe record after the growing one
}

TEST_CASE("tile status: buildings by their menu name, houses, withered fields, bare ground") {
    const Island i;
    CHECK(i.at(5) == "Weberei");
    CHECK(i.at(6) == "Haus");
    CHECK(i.at(4) == "Verdorrtes Feld");
    CHECK(i.at(0).empty());
    CHECK(i.at(9).empty());  // off the island
}

TEST_CASE("tile status: the crops of grass and trees only once the island is explored") {
    Island i;
    CHECK(i.at(3).empty());
    i.island.explore(kMe);
    CHECK(i.at(3) == "Geeignet für: Zuckerrohr: 100% - Weintrauben: 50% - Tabak: 50%");
}

TEST_CASE("tile status: an undiscovered island shows its terrain, not its buildings") {
    Island i;
    REQUIRE(i.island.map.place(*i.buildings.by_id(20501), 0, 0, 0, 0, 0, 0).ok());  // over the ground
    CHECK(i.at(0) == "Weberei");
    i.island.discovered.clear();
    CHECK(i.at(0).empty());
    CHECK(i.at(2) == "Fischgründe");
}
