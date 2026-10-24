// SPDX-License-Identifier: GPL-3.0-or-later
// The windows of what the player selects (BT-2) without the original data: a folder with the files
// an installation needs and a synthetic PROD layout with one gadget per Id the window knows, a
// world with a bakery of the local player and one of another player.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "../test_framework.hpp"
#include "selection_panels.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;
using app::PanelContext;
using app::Selection;
using app::SelectionPanel;
using app::SelectionPanels;

namespace {

constexpr std::array<const ui::Font*, ui::kFontSlots> kNoFonts = {nullptr, nullptr, nullptr};
constexpr sim::PlayerId kLocal = 0;
constexpr sim::PlayerId kOther = 1;
constexpr std::uint32_t kBakery = 20503;
constexpr std::int32_t kStop = 37009;
constexpr std::int32_t kPickup = 37010;
constexpr std::int32_t kCostValue = 37006;

/// A layout with these gadget Ids, each a text box of its own place.
std::string gad(const std::vector<std::int32_t>& ids) {
    std::string out = "Objekt: GADGET\n";
    for (std::size_t i = 0; i < ids.size(); ++i) {
        out += "  @Nummer: " + std::to_string(i) + "\n  Id: " + std::to_string(ids[i]) +
               "\n  Kind: 6\n  Pos: " + std::to_string(static_cast<int>(i % 20) * 30) + ", " +
               std::to_string(100 + static_cast<int>(i / 20) * 20) + "\n  Size: 20, 10\n";
    }
    return out + "EndObj\n";
}

void write(const std::filesystem::path& file, const std::string& text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

/// An installation with the PROD layout (37002..37041) and the GAME texts the window shows.
struct Game {
    std::filesystem::path root;
    std::optional<assets::GameDirectory> dir;
    assets::TextTable texts;

    explicit Game(const std::string& name) {
        root = test::temp_path(name);
        std::filesystem::remove_all(root);
        for (const char* file : {"haeuser.cod", "figuren.cod", "toolgfx/stadtfld.col", "gfx/stadtfld.bsh"}) {
            write(root / file, "");
        }
        std::vector<std::int32_t> ids;
        for (std::int32_t id = 37002; id <= 37041; ++id) {
            ids.push_back(id);
        }
        write(root / "GADDATA" / "PROD.GAD", gad(ids));
        auto opened = assets::GameDirectory::open(root);
        REQUIRE(opened.ok());
        dir = std::move(opened).value();
        std::string game = "[GAME]\n";
        for (int i = 0; i < 60; ++i) {
            game += i == 18   ? "Betriebskosten\n"
                    : i == 48 ? "%d%%\n"
                    : i == 49 ? "%dt\n"
                              : "Text " + std::to_string(i) + "\n";
        }
        texts = assets::TextTable::parse(game);
    }
    ~Game() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
};

/// A bakery (HANDWERK) with running costs 25 at work and 10 at rest.
assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20503\n  Gfx: 0\n  Size: 2, 2\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 1\n    Maxlager: 4\n    Ware: 10\n    Rohstoff: 5\n"
        "    Rohmenge: 1\n    Prodmenge: 1\n    Interval: 20\n    Kosten: 25, 10\n  EndObj\n"
        "EndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

/// One island with a bakery of the local player at (2, 2) and one of another player at (6, 2).
struct Town {
    assets::BuildingTable buildings = make_buildings();
    assets::FigureTable figures;
    sim::World world{40, 40};

    Town() {
        sim::IslandMap map(12, 8);
        REQUIRE(map.place(*buildings.by_id(kBakery), 2, 2, 0, 0, kLocal).ok());
        REQUIRE(map.place(*buildings.by_id(kBakery), 6, 2, 0, 0, kOther).ok());
        sim::WorldIsland island{assets::IslandHeader{}, std::move(map), 4, 4};
        island.header.width = 12;
        island.header.height = 8;
        REQUIRE(world.add_island(std::move(island)).ok());
        world.set_players(std::vector<sim::Player>(2));
        for (const std::uint16_t x : {std::uint16_t{2}, std::uint16_t{6}}) {
            sim::Production p;
            p.island = 0;
            p.x = x;
            p.y = 2;
            p.enabled = true;
            world.set_production(p);
        }
    }
};

Selection bakery_at(std::uint16_t x) {
    Selection s;
    s.kind = Selection::Kind::Building;
    s.island = 0;
    s.x = x;
    s.y = 2;
    return s;
}

std::vector<std::string> texts(SelectionPanel& panel) {
    std::vector<std::string> out;
    for (const ui::DrawItem& d : panel.panel().draw(kNoFonts)) {
        if (d.kind == ui::DrawItem::Kind::Text && !d.text.empty()) {
            out.push_back(d.text);
        }
    }
    return out;
}

}  // namespace

TEST_CASE("selection panels: an own workshop opens the production window with its running cost") {
    Game g("o1602-test-selection-prod");
    Town t;
    sim::Simulation simulation(t.world, t.buildings, t.figures);
    SelectionPanels panels(*g.dir);
    const PanelContext c{simulation, t.buildings, t.figures, &g.texts, kLocal};
    SelectionPanel* p = panels.panel_for(bakery_at(2), c);
    REQUIRE(p != nullptr);
    p->refresh(bakery_at(2), c);
    const auto shown = texts(*p);
    CHECK(std::find(shown.begin(), shown.end(), "Betriebskosten") != shown.end());
    CHECK(std::find(shown.begin(), shown.end(), "25") != shown.end());
    CHECK(!p->panel().selected(kStop));
    // Another player's workshop has no window.
    CHECK(panels.panel_for(bakery_at(6), c) == nullptr);
}

TEST_CASE("selection panels: the production window's buttons stop the workshop and keep the carts away") {
    Game g("o1602-test-selection-prod-click");
    Town t;
    sim::Simulation simulation(t.world, t.buildings, t.figures);
    SelectionPanels panels(*g.dir);
    const PanelContext c{simulation, t.buildings, t.figures, &g.texts, kLocal};
    SelectionPanel* p = panels.panel_for(bakery_at(2), c);
    REQUIRE(p != nullptr);
    std::vector<sim::Command> out;
    p->clicked(ui::PanelEvent{ui::PanelEvent::Kind::Clicked, kStop}, bakery_at(2), c, out);
    p->clicked(ui::PanelEvent{ui::PanelEvent::Kind::Clicked, kPickup}, bakery_at(2), c, out);
    p->clicked(ui::PanelEvent{ui::PanelEvent::Kind::Pressed, kStop}, bakery_at(2), c, out);  // only clicks count
    REQUIRE(out.size() == 2);
    const auto* stop = std::get_if<sim::ProductionSwitch>(&out[0]);
    REQUIRE(stop != nullptr);
    CHECK(stop->op == sim::ProductionSwitch::Op::Enable);
    CHECK(!stop->on);
    CHECK(stop->player == kLocal);
    CHECK(stop->x == 2);
    CHECK(stop->y == 2);
    const auto* pickup = std::get_if<sim::ProductionSwitch>(&out[1]);
    REQUIRE(pickup != nullptr);
    CHECK(pickup->op == sim::ProductionSwitch::Op::Pickup);
    CHECK(!pickup->on);

    // Run by the game, the window shows the stopped workshop and its cost at rest.
    simulation.execute(out[0]);
    p->refresh(bakery_at(2), c);
    CHECK(p->panel().selected(kStop));
    const auto shown = texts(*p);
    CHECK(std::find(shown.begin(), shown.end(), "10") != shown.end());
    CHECK(p->help(kCostValue, bakery_at(2), c) == std::nullopt);
}
