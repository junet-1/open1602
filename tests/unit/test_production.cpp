// SPDX-License-Identifier: GPL-3.0-or-later
// Production state: building animation frames, smoke and status icons (docs/re/production.md).

#include <algorithm>

#include "../test_framework.hpp"
#include "test_support.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/savegame.hpp"
#include "o1602/render/sprite_atlas.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

/// A bakery (HANDWERK with smoke RAUCHBAECK 48), a forester (PLANTAGE with LagAniFlg) and a market.
assets::BuildingTable make_buildings() {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n  Id: 20503\n  Gfx: 0\n  Size: 2, 2\n  AnimAnz: 8\n  AnimAdd: 4\n  AnimTime: 100\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 1\n    Rauchfignr: 48\n    Maxlager: 4\n    Maxnorohst: 5\n  EndObj\n"
        "  @Nummer: +1\n  Id: 22201\n  Gfx: 40\n  Size: 1, 1\n  AnimAnz: 4\n  AnimAdd: 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 2\n    Maxlager: 10\n    LagAniFlg: 1\n  EndObj\n"
        "  @Nummer: +1\n  Id: 20809\n  Gfx: 50\n  Size: 1, 1\n  AnimAnz: 3\n  AnimAdd: 1\n"
        "  Objekt: HAUS_PRODTYP\n    Kind: 7\n    Rauchfignr: 59\n  EndObj\n"
        "EndObj\n",
        types);
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

/// RAUCHBAECK (48) and NOAKTIV (46) like figuren.cod: no Blocknr, drawn from EFFEKTE.BSH.
assets::FigureTable make_figures() {
    const assets::CodPredefinedSymbol types[] = {{"FIGUR", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: FIGUR\n"
        "  Nummer: 46\n  Gfx: 10\n  Rotate: 0\n  Posoffs: -35, 35\n"
        "  Objekt: ANIM\n"
        "    Nummer: 0\n    Kind: 1\n    AnimOffs: 0\n    AnimAdd: 1\n    AnimAnz: 4\n    AnimSpeed: 50\n"
        "    Nummer: 1\n    Kind: 1\n    AnimOffs: 4\n    AnimAdd: 1\n    AnimAnz: 4\n    AnimSpeed: 50\n"
        "  EndObj\n"
        "  Nummer: 48\n  Gfx: 20\n  Rotate: 0\n  Posoffs: -20, 20\n  Fahnoffs: -0.5, 0.25, 2.0\n"
        "  Objekt: ANIM\n"
        "    Nummer: 0\n    Kind: 1\n    AnimOffs: 0\n    AnimAdd: 1\n    AnimAnz: 5\n    AnimSpeed: 80\n"
        "  EndObj\n"
        "EndObj\n",
        types);
    auto table = assets::FigureTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

}  // namespace

TEST_CASE("production: HAUS_PRODTYP fields for the production state") {
    const auto buildings = make_buildings();
    const assets::BuildingType& bakery = *buildings.by_id(20503);
    CHECK(bakery.max_stock == 4 * assets::kStockUnitsPerTon);
    CHECK(bakery.max_idle_cycles == 5);
    CHECK(!bakery.stock_animation);
    CHECK(sim::is_producer(bakery));
    CHECK(buildings.by_id(22201)->stock_animation);
    CHECK(!sim::is_producer(*buildings.by_id(20809)));  // market: kind 7
    CHECK(render::has_production_animation(*buildings.by_id(20809)));
}

TEST_CASE("production: animation frames follow the building's state (0x00489C90)") {
    const auto buildings = make_buildings();
    const assets::BuildingType& bakery = *buildings.by_id(20503);
    sim::Production p;
    p.anim_phase = 3;
    // Working: runs from its phase with the global frame; idle or switched off: stands still.
    p.working = true;
    CHECK(render::production_frame(bakery, 10, &p) == (3 + 10) % 8);
    p.working = false;
    CHECK(render::production_frame(bakery, 10, &p) == 3);
    p.working = true;
    p.enabled = false;
    CHECK(render::production_frame(bakery, 10, &p) == 3);
    // Without state the original uses the global frame alone.
    CHECK(render::production_frame(bakery, 10, nullptr) == 10 % 8);

    // LagAniFlg: the frame shows the fill level, (AnimAnz - 1) * stock / Maxlager, rounded.
    const assets::BuildingType& forester = *buildings.by_id(22201);
    p = sim::Production{};
    p.stock = 0;
    CHECK(render::production_frame(forester, 7, &p) == 0);
    p.stock = 5 * 32;  // half of 10 t: 1.5 -> 2
    CHECK(render::production_frame(forester, 7, &p) == 2);
    p.stock = 10 * 32;
    CHECK(render::production_frame(forester, 7, &p) == 3);
    p.stock = 40 * 32;  // above Maxlager: stays on the last frame
    CHECK(render::production_frame(forester, 7, &p) == 3);
    CHECK(render::production_frame(forester, 7, nullptr) == 0);

    // Market: the stalls fill with the progress of the cycle, (AnimAnz - 1) * progress / 512.
    const assets::BuildingType& market = *buildings.by_id(20809);
    p.progress = 0;
    CHECK(render::production_frame(market, 1, &p) == 0);
    p.progress = 256;
    CHECK(render::production_frame(market, 1, &p) == 1);
    p.progress = 514;  // seen in a savegame
    CHECK(render::production_frame(market, 1, &p) == 2);
}

TEST_CASE("production: status of a building and smoke") {
    const auto buildings = make_buildings();
    const assets::BuildingType& bakery = *buildings.by_id(20503);
    sim::Production p;
    CHECK(sim::production_status(p, bakery) == sim::ProductionStatus::Ok);
    p.idle_cycles = 5;
    CHECK(sim::production_status(p, bakery) == sim::ProductionStatus::NoRawGoods);
    p.enabled = false;
    CHECK(sim::production_status(p, bakery) == sim::ProductionStatus::Disabled);
    CHECK(!sim::smoke_visible(p));
    p.enabled = true;
    CHECK(!sim::smoke_visible(p));
    p.working = true;
    CHECK(sim::smoke_visible(p));
}

TEST_CASE("production: world lookup by island and origin tile") {
    sim::World world;
    world.set_production(sim::Production{2, 10, 11, true, true, 0, 0, 32, 100});
    world.set_production(sim::Production{2, 11, 10, true, false, 0, 0, 0, 0});
    REQUIRE(world.production(2, 10, 11) != nullptr);
    CHECK(world.production(2, 10, 11)->stock == 32);
    CHECK(!world.production(2, 11, 10)->working);
    CHECK(world.production(1, 10, 11) == nullptr);
    world.set_production(sim::Production{2, 10, 11, false, false, 0, 0, 0, 0});  // replaces
    CHECK(world.productions().size() == 2);
    CHECK(!world.production(2, 10, 11)->enabled);
}

TEST_CASE("production: smoke and status icons on buildings") {
    const auto buildings = make_buildings();
    const auto figures = make_figures();
    sim::World world;
    world.set_players({sim::Player{}, sim::Player{sim::PlayerRole::Computer, 1, 0, ""}});
    sim::WorldIsland island{assets::IslandHeader{}, sim::IslandMap(8, 8), 5, 5};
    REQUIRE(island.map.place(*buildings.by_id(20503), 1, 1, 0, 0, 0).ok());  // player 0
    REQUIRE(island.map.place(*buildings.by_id(20503), 4, 4, 1, 0, 1).ok());  // player 1, rotated
    REQUIRE(island.map.place(*buildings.by_id(20503), 4, 0, 0, 0).ok());     // no city: nothing
    REQUIRE(world.add_island(std::move(island)).ok());

    const auto all = render::collect_building_figures(world, buildings, figures);
    // Per bakery in a city: status icon and smoke.
    REQUIRE(all.size() == 4);
    CHECK(all[0].role == render::BuildingFigureRole::Status);
    CHECK(all[0].figure == 46);
    CHECK(all[0].x == 7.0);  // centre of the 2x2 footprint, NOAKTIV has no Fahnoffs
    CHECK(all[0].y == 7.0);
    CHECK(all[1].role == render::BuildingFigureRole::Smoke);
    CHECK(all[1].x == 6.5);   // 5 + 1 + 1 - 0.5
    CHECK(all[1].y == 7.25);  // 5 + 1 + 1 + 0.25
    CHECK(all[1].z == 2.0);
    CHECK(all[3].x == 9.75);  // rotation 1: x - Fahnoffs[1], y + Fahnoffs[0]
    CHECK(all[3].y == 9.5);
    CHECK(all[3].owner == 1);

    // Effects sheet with 40 sprites 6x8.
    const auto terrain = test::make_sheet({{4, 2}});
    const auto effekte = test::make_sheet(std::vector<std::pair<std::uint32_t, std::uint32_t>>(40, {6, 8}));
    const auto palette = test::make_palette();
    const assets::SpriteSheet* sheets[] = {&terrain, &effekte};
    auto atlas = render::SpriteAtlas::build(sheets, palette, 256);
    REQUIRE(atlas.ok());
    render::WorldLayers layers;
    layers.figures = &figures;
    layers.atlas = &*atlas;
    layers.sheet_base[static_cast<std::size_t>(assets::FigureSheet::Effekte)] = atlas->sheet_base(1);
    layers.local_player = 0;
    const render::WorldView view{render::Zoom::Large, 0};
    render::FigureSprites s;

    // Without production state nothing is shown.
    CHECK(!render::building_figure_sprites(all[1], world, buildings, view, layers, 0, s));
    world.set_production(sim::Production{0, 1, 1, true, false, 0, 0, 0, 0});
    world.set_production(sim::Production{0, 4, 4, true, false, 0, 5, 0, 0});
    // Idle bakery: no smoke; status OK: no icon.
    CHECK(!render::building_figure_sprites(all[1], world, buildings, view, layers, 0, s));
    CHECK(!render::building_figure_sprites(all[0], world, buildings, view, layers, 0, s));

    world.set_production(sim::Production{0, 1, 1, true, true, 0, 0, 0, 0});
    render::BuildingFigure smoke = all[1];
    smoke.phase = 0;
    REQUIRE(render::building_figure_sprites(smoke, world, buildings, view, layers, 170, s));
    REQUIRE(s.count == 1);
    CHECK(s.parts[0].sprite == atlas->sheet_base(1) + 20 + 2);  // frame 170 / 80
    CHECK(s.parts[0].mode == render::DrawCommand::Mode::Translucent);
    CHECK(s.parts[0].tint.a == render::kSmokeAlpha);
    // Point: x = (6.5 - 7.25 + 350) * 32 = 11176, y = 13.75 * 16 = 220; Posoffs cancel out,
    // bottom 2.0 * 32 above the point; depth y + 20.
    CHECK(s.parts[0].x == 11176 - 3);
    CHECK(s.parts[0].y == 220 - 64);
    CHECK(s.row == (220 + 20 - 1) / 16);

    // Status icon: only for the local player's buildings; anim 0 off, 1 short of raw material.
    world.set_production(sim::Production{0, 1, 1, false, false, 0, 0, 0, 0});
    render::BuildingFigure icon = all[0];
    icon.phase = 0;
    REQUIRE(render::building_figure_sprites(icon, world, buildings, view, layers, 0, s));
    CHECK(s.parts[0].sprite == atlas->sheet_base(1) + 10);
    CHECK(s.parts[0].mode == render::DrawCommand::Mode::Normal);
    world.set_production(sim::Production{0, 1, 1, true, false, 0, 7, 0, 0});
    REQUIRE(render::building_figure_sprites(icon, world, buildings, view, layers, 0, s));
    CHECK(s.parts[0].sprite == atlas->sheet_base(1) + 10 + 4);
    // Player 1's bakery is short of raw material too, but player 1 is not at this computer.
    CHECK(!render::building_figure_sprites(all[2], world, buildings, view, layers, 0, s));
    layers.local_player = sim::kNoPlayer;
    CHECK(!render::building_figure_sprites(icon, world, buildings, view, layers, 0, s));
}
