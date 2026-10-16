// SPDX-License-Identifier: GPL-3.0-or-later
// Soldiers: savegame records, colour variants and tinted units (docs/re/figures.md).

#include <cstring>

#include "../test_framework.hpp"
#include "test_support.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/savegame.hpp"
#include "o1602/render/banner.hpp"
#include "o1602/render/flag_tint.hpp"
#include "o1602/render/sprite_atlas.hpp"
#include "o1602/render/unit_tint.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

TEST_CASE("units: SOLDAT3 records") {
    std::vector<std::uint8_t> rec(0x44, 0);
    rec[0] = 0x5C;  // x 604 half tiles = 302
    rec[1] = 0x02;
    rec[2] = 0x0C;  // y 268 = 134
    rec[3] = 0x01;
    rec[6] = 33;       // SPEER1
    rec[0x18] = 6;     // natives
    rec[0x19] = 1;     // ANIM 1
    rec[0x1B] = 0x0F;  // direction 7 (upper bits ignored)
    std::vector<std::uint8_t> file;
    test::put_chunk(file, "SOLDAT3", rec);
    auto chunks = assets::ChunkFile::from_bytes(file);
    REQUIRE(chunks.ok());
    auto state = assets::load_game_state(*chunks);
    REQUIRE(state.ok());
    REQUIRE(state->soldiers.size() == 1);
    const auto& s = state->soldiers[0];
    CHECK(s.x2 == 604);
    CHECK(s.y2 == 268);
    CHECK(s.figure == 33);
    CHECK(s.owner == 6);
    CHECK(s.anim == 1);
    CHECK(s.direction == 7);
}

TEST_CASE("units: colour variants of the original unit families") {
    const auto v = assets::FigureTable::unit_variant(7);  // KAVALERIE3 = yellow rider
    REQUIRE(v.has_value());
    CHECK(v->family == 5);
    CHECK(v->index == 2);
    CHECK(v->count == 4);
    CHECK(assets::FigureTable::unit_variant(1)->index == 0);
    CHECK(assets::FigureTable::unit_variant(16)->family == 13);
    CHECK(!assets::FigureTable::unit_variant(33).has_value());  // SPEER1: one image set for all
    CHECK(!assets::FigureTable::unit_variant(17).has_value());

    const auto banners = render::BannerTable::original();
    CHECK(banners.get(0)->unit_variant == std::optional<std::uint8_t>(0));
    CHECK(banners.get(3)->unit_variant == std::optional<std::uint8_t>(3));
    CHECK(!banners.get(4)->unit_variant.has_value());  // pirates
}

TEST_CASE("units: tint template from four drawn variants") {
    // Pixels: sash, boots (black only for yellow), horse (not in the player colours), steel grey.
    const std::uint8_t red[] = {200, 40, 10, 255, 137, 86, 13, 255, 137, 86, 13, 255, 128, 128, 128, 255};
    const std::uint8_t blue[] = {0, 0, 200, 255, 137, 86, 13, 255, 120, 120, 120, 255, 128, 128, 128, 255};
    const std::uint8_t yellow[] = {230, 200, 40, 255, 0, 4, 4, 255, 89, 42, 20, 255, 128, 128, 128, 255};
    const std::uint8_t white[] = {230, 230, 230, 255, 137, 86, 13, 255, 220, 220, 220, 255, 128, 128, 128, 255};
    const render::RgbaView views[] = {{red, 4, 1}, {blue, 4, 1}, {yellow, 4, 1}, {white, 4, 1}};
    const std::vector<assets::Rgba> colors(render::BannerTable::kDefaultColors.begin(),
                                           render::BannerTable::kDefaultColors.begin() + 4);
    std::vector<std::uint8_t> out;
    render::unit_template(views, colors, 3, out);
    REQUIRE(out.size() == 16);
    // Sash: cloth, grey 0.48 * 230 + 5 = 115.
    CHECK(out[0] == 115);
    CHECK(out[1] == 115);
    CHECK(out[2] == 115);
    // Boots: two colours only -> as in the white variant.
    CHECK(out[4] == 137);
    CHECK(out[6] == 13);
    // Horse: differs between the variants, but the blue rider's horse is grey -> not cloth; as a
    // pure grey it is moved out of the tint.
    CHECK(out[8] == 220);
    CHECK(out[10] == 221);
    // Steel: same everywhere, pure grey -> moved by one in blue.
    CHECK(out[12] == 128);
    CHECK(out[14] == 129);

    // Tinted green, the sash takes the colour, the rest stays.
    render::tint_rgba(out, assets::Rgba{30, 170, 40, 255});
    CHECK(out[1] > out[0]);
    CHECK(out[4] == 137);
}

TEST_CASE("units: soldiers in the colour of their owner's banner") {
    const assets::CodPredefinedSymbol types[] = {{"FIGUR", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: FIGUR\n"
        "  Nummer: 1\n  Gfx: 0\n  Rotate: 1\n  Posoffs: 0, 3\n"
        "  Nummer: 2\n  Gfx: 10\n  Rotate: 1\n  Posoffs: 0, 3\n"
        "  Nummer: 3\n  Gfx: 20\n  Rotate: 1\n  Posoffs: 0, 3\n"
        "  Nummer: 4\n  Gfx: 30\n  Rotate: 1\n  Posoffs: 0, 3\n"
        "EndObj\n",
        types);
    auto figures = assets::FigureTable::from_cod(doc);
    REQUIRE(figures.ok());

    // Atlas: terrain, soldier sheet (40 images 4x6) and a tinted block of 10 images for banner 7.
    const auto terrain = test::make_sheet({{4, 2}});
    const auto soldat = test::make_sheet(std::vector<std::pair<std::uint32_t, std::uint32_t>>(40, {4, 6}));
    const auto tinted = test::make_sheet(std::vector<std::pair<std::uint32_t, std::uint32_t>>(10, {4, 6}));
    const auto palette = test::make_palette();
    const assets::SpriteSheet* sheets[] = {&terrain, &soldat, &tinted};
    auto atlas = render::SpriteAtlas::build(sheets, palette, 256);
    REQUIRE(atlas.ok());

    auto banners = render::BannerTable::original();
    const sim::BannerId green = banners.add_color("test:green", render::BannerTable::kDefaultColors[4]);
    render::UnitTint tint;
    tint.groups.push_back(render::UnitImageGroup{0, 10, 4, 3, 0});
    tint.block_size = 10;
    tint.banner_base.resize(banners.size());
    tint.banner_base[green] = atlas->sheet_base(2);
    CHECK(tint.sprite(green, 25) == std::optional<std::uint32_t>(atlas->sheet_base(2) + 5));  // any variant
    CHECK(!tint.sprite(1, 5).has_value());
    CHECK(!tint.sprite(green, 40).has_value());

    sim::World world;
    world.set_players(
        {sim::Player{sim::PlayerRole::Human, 2, 0, ""}, sim::Player{sim::PlayerRole::Computer, green, 0, ""}});
    render::WorldLayers layers;
    layers.figures = &*figures;
    layers.atlas = &*atlas;
    layers.banners = &banners;
    layers.unit_tint = &tint;
    layers.sheet_base[static_cast<std::size_t>(assets::FigureSheet::Soldat)] = atlas->sheet_base(1);

    render::FigureInstance soldier;
    soldier.x = 10.0;
    soldier.y = 20.0;
    soldier.figure = 1;  // family SOLDAT1..4
    soldier.direction = 3;
    soldier.owner = 0;  // yellow banner: drawn variant 2
    soldier.sheet = assets::FigureSheet::Soldat;
    render::FigureSprites f;
    REQUIRE(render::land_figure_sprites(soldier, world, render::WorldView{}, layers, f));
    CHECK(f.parts[f.count - 1].sprite == atlas->sheet_base(1) + 20 + 3);
    soldier.owner = 1;  // green: tinted template
    REQUIRE(render::land_figure_sprites(soldier, world, render::WorldView{}, layers, f));
    CHECK(f.parts[f.count - 1].sprite == atlas->sheet_base(2) + 3);
    soldier.owner = sim::kNoPlayer;  // nobody: the stored figure
    REQUIRE(render::land_figure_sprites(soldier, world, render::WorldView{}, layers, f));
    CHECK(f.parts[f.count - 1].sprite == atlas->sheet_base(1) + 3);
}
