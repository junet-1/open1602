// SPDX-License-Identifier: GPL-3.0-or-later
// The build menu as data: the packages' build_menu.toml (engine/ui/include/o1602/ui/build_menu.hpp,
// docs/modding/build-menu.md, MD-5).

#include <string>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/ui/build_menu.hpp"

using namespace o1602;

namespace {

/// The Kirche and the Schaffarm of the original and a building of a mod (Id 22201).
assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20805\n  Kind: 10\n  Gfx: 0\n  Size: 1, 1\n"
        "  @Nummer: 1\n  Id: 21515\n  Kind: 10\n  Gfx: 1\n  Size: 1, 1\n"
        "  @Nummer: 2\n  Id: 22201\n  Kind: 10\n  Gfx: 2\n  Size: 1, 1\n"
        "EndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

constexpr std::int32_t kFarms = 31006;
constexpr std::int32_t kHarbour = 31007;
constexpr std::int32_t kPublic = 31008;

}  // namespace

TEST_CASE("build menu: the original's buildings are named by their Text.cod line and have their icons") {
    const ui::BuildCategory* farms = ui::original_build_menu().category(kFarms);
    REQUIRE(farms != nullptr);
    const ui::BuildButton* sheep = farms->button_at(5);
    REQUIRE(sheep != nullptr);
    CHECK(sheep->id == 21515);
    CHECK(sheep->icon == 44 + 5);
    CHECK(farms->name(5, {"a", "b", "c", "d", "e", "Schaffarm"}) == "Schaffarm");
    CHECK(farms->name(5, {"a"}).empty());  // a Text.cod with fewer lines
    CHECK(&ui::build_menu() != &ui::original_build_menu());
    CHECK(ui::build_menu().categories.size() == ui::original_build_menu().categories.size());
}

TEST_CASE("build menu: a package adds a building with its icon and name, moves one, removes one (MD-5)") {
    const assets::BuildingTable buildings = make_buildings();
    ui::BuildMenuDefinition menu = ui::original_build_menu();
    std::vector<std::string> warnings;
    const auto status = ui::apply_build_menu_package(menu, "lights",
                                                     "[lighthouse]\n"
                                                     "id = 22201\n"
                                                     "category = 31007\n"
                                                     "row = 3\n"
                                                     "icon = 90\n"
                                                     "name = \"Leuchtturm\"\n"
                                                     "\n"
                                                     "[church]\n"
                                                     "id = 20805\n"
                                                     "category = 31007\n"
                                                     "row = 3\n"
                                                     "position = 1\n"
                                                     "\n"
                                                     "[sheep]\n"
                                                     "id = 21515\n"
                                                     "remove = true\n",
                                                     buildings, warnings);
    REQUIRE(status.ok());
    CHECK(warnings.empty());
    const ui::BuildCategory* harbour = menu.category(kHarbour);
    REQUIRE(harbour->rows.size() == 3);
    REQUIRE(harbour->rows[2].size() == 2);
    // The church moved from the public buildings and keeps its icon and Text.cod line.
    CHECK(menu.category_of(20805) == harbour);
    CHECK(harbour->rows[2][0].id == 20805);
    CHECK(harbour->rows[2][0].icon == 71 + 3);
    CHECK(harbour->rows[2][0].text == 3);
    CHECK(menu.category(kPublic)->size() == 15);
    // The new building: its own icon and name.
    const auto index = harbour->index_of(22201);
    REQUIRE(index.has_value());
    CHECK(harbour->button_at(*index)->icon == 90);
    CHECK(harbour->name(*index, {}) == "Leuchtturm");
    CHECK(!menu.category_of(21515));
    CHECK(menu.category(kFarms)->size() == 18);
}

TEST_CASE("build menu: what is wrong in a table is passed over with a warning naming the line (MD-5)") {
    const assets::BuildingTable buildings = make_buildings();
    ui::BuildMenuDefinition menu = ui::original_build_menu();
    std::vector<std::string> warnings;
    const auto status =
        ui::apply_build_menu_package(menu, "bad",
                                     "[unknown]\nid = 22999\ncategory = 31007\nicon = 1\nname = \"x\"\n"
                                     "[noicon]\nid = 22201\ncategory = 31007\nname = \"x\"\n"
                                     "[demolish]\nid = 22201\ncategory = 31004\nicon = 1\nname = \"x\"\n"
                                     "[full]\nid = 22201\ncategory = 31007\nrow = 1\nicon = 1\nname = \"x\"\n"
                                     "[far]\nid = 22201\ncategory = 31007\nrow = 4\nicon = 1\nname = \"x\"\n"
                                     "[typo]\nid = 22201\ncategory = 31007\nicon = 1\nname = \"x\"\nicno = 2\n",
                                     buildings, warnings);
    REQUIRE(status.ok());
    REQUIRE(warnings.size() == 6);
    CHECK(warnings[0].find("bad/build_menu.toml line 1: haeuser.cod has no HAUS with Id 22999") == 0);
    CHECK(warnings[1].find("needs an 'icon'") != std::string::npos);
    CHECK(warnings[2].find("no category 31004") != std::string::npos);
    CHECK(warnings[3].find("row 1 is full") != std::string::npos);
    CHECK(warnings[4].find("row 4") != std::string::npos);
    CHECK(warnings[5].find("'icno'") != std::string::npos);  // the field is passed over, the rest is used
    CHECK(menu.category_of(22201) == menu.category(kHarbour));
    CHECK(menu.category(kHarbour)->rows.size() == 3);  // its two rows are full: a new one
    // A file that is no TOML changes nothing.
    const ui::BuildMenuDefinition before = menu;
    CHECK(!ui::apply_build_menu_package(menu, "bad", "[x\n", buildings, warnings).ok());
    CHECK(menu.category(kHarbour)->size() == before.category(kHarbour)->size());
}
