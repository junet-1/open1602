// SPDX-License-Identifier: GPL-3.0-or-later
// Figure animations (ANIM), land figures with shadows, half-size sheets (docs/re/figures.md).

#include <algorithm>

#include "../test_framework.hpp"
#include "test_support.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/render/animal_figures.hpp"
#include "o1602/render/figure_animation.hpp"
#include "o1602/render/sprite_atlas.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

/// A deer like HIRSCH (89) with its shadow SCHATTENLANG (117), small numbers for the sheets.
assets::FigureTable make_deer() {
    const assets::CodPredefinedSymbol types[] = {{"FIGUR", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: FIGUR\n"
        "  Nummer: 89\n  Gfx: 4\n  Blocknr: 5\n  Rotate: 8\n  Posoffs: 3, 9\n  Schatten: 117\n"
        "  Objekt: ANIM\n"
        "    Nummer: 0\n    Kind: 1\n    AnimOffs: 0\n    AnimAdd: 1\n    AnimAnz: 8\n    AnimSpeed: 135\n"
        "    Nummer: 1\n    Kind: 2\n    AnimOffs: 112\n    AnimAdd: 1\n    AnimAnz: 6\n    AnimSpeed: 130\n"
        "    AnimNr: 1, 1, 3, 1\n    AnimRept: 2\n    Rotate: 6\n"
        "    Nummer: 3\n    Kind: 3\n    AnimOffs: 64\n    AnimAdd: 1\n    AnimAnz: 3\n    AnimSpeed: 130\n"
        "    AnimNr: 1\n    Rotate: 6\n"
        "  EndObj\n"
        "  Nummer: 117\n  Gfx: 8\n  Blocknr: 6\n  Rotate: 8\n"
        "  Objekt: ANIM\n"
        "    Nummer: 0\n    Kind: 1\n    AnimOffs: 0\n    AnimAdd: 1\n    AnimAnz: 8\n    AnimSpeed: 135\n"
        "  EndObj\n"
        "EndObj\n",
        types);
    auto table = assets::FigureTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

}  // namespace

TEST_CASE("figures: ANIM objects and the sprite formula of 0x0044F8F0") {
    const auto figures = make_deer();
    const assets::FigureType* deer = figures.get(89);
    REQUIRE(deer != nullptr);
    CHECK(deer->shadow == 117);
    CHECK(deer->sheet == assets::FigureSheet::Tiere);
    REQUIRE(deer->anim(0) != nullptr);
    CHECK(deer->anim(0)->rotate == 8);  // no Rotate: the figure's
    REQUIRE(deer->anim(1) != nullptr);
    const assets::FigureAnim& graze = *deer->anim(1);
    CHECK(graze.kind == assets::AnimKind::Random);
    CHECK(graze.offset == 112);
    CHECK(graze.count == 6);
    CHECK(graze.repeat == 2);
    CHECK(graze.speed_ms == 130);
    CHECK(graze.rotate == 6);
    CHECK((graze.next == std::vector<std::uint8_t>{1, 1, 3, 1}));
    CHECK(deer->anim(2) == nullptr);
    CHECK(deer->anim(0)->repeat == 1);

    // Gfx + AnimOffs + AnimAdd * frame + ((dir + 2 * rotation) & 7) * Rotate
    CHECK(assets::figure_sprite(*deer, 1, 2, 3, 0) == 4 + 112 + 2 + 3 * 6);
    CHECK(assets::figure_sprite(*deer, 1, 2, 3, 3) == 4 + 112 + 2 + 1 * 6);  // (3 + 6) & 7 = 1
    CHECK(assets::figure_sprite(*deer, 0, 5, 7, 0) == 4 + 5 + 7 * 8);
    // An animation the figure lacks: Gfx + direction * Rotate (shadows of grazing animals).
    const assets::FigureType* shadow = figures.get(117);
    REQUIRE(shadow != nullptr);
    CHECK(assets::figure_sprite(*shadow, 1, 4, 2, 0) == 8 + 2 * 8);
}

TEST_CASE("figures: animation stepping like 0x0045D0B0") {
    const auto figures = make_deer();
    const assets::FigureType& deer = *figures.get(89);

    render::AnimState s;
    s.anim = 0;  // ENDLESS, 8 frames of 135 ms
    render::advance_animation(s, deer, 134);
    CHECK(s.frame == 0);
    render::advance_animation(s, deer, 1);
    CHECK(s.frame == 1);
    render::advance_animation(s, deer, 135 * 7);
    CHECK(s.frame == 0);  // wrapped around
    CHECK(s.anim == 0);

    // JUMPTO: after one run (3 frames) continue with AnimNr[0] = 1.
    s = render::AnimState{};
    s.anim = 3;
    render::advance_animation(s, deer, 130 * 3);
    CHECK(s.anim == 1);
    CHECK(s.frame == 0);

    // RANDOM with AnimRept 2: two runs of 6 frames, then one of AnimNr (1 or 3).
    s = render::AnimState{};
    s.anim = 1;
    s.random = 12345;
    render::advance_animation(s, deer, 130 * 6);
    CHECK(s.anim == 1);
    CHECK(s.runs == 1);
    render::advance_animation(s, deer, 130 * 6);
    CHECK((s.anim == 1 || s.anim == 3));
    CHECK(s.runs == 0);

    // Same seed, same course.
    render::AnimState a = render::start_animation(deer, 1, 42);
    render::AnimState b = render::start_animation(deer, 1, 42);
    for (int i = 0; i < 100; ++i) {
        render::advance_animation(a, deer, 97);
        render::advance_animation(b, deer, 97);
    }
    CHECK(a.anim == b.anim);
    CHECK(a.frame == b.frame);
    CHECK(render::start_animation(deer, 1, 1).frame < 6);
}

TEST_CASE("figures: land figure with shadow, position and depth") {
    const auto figures = make_deer();
    // Atlas: sheet 0 terrain, sheet 1 "tiere" (200 sprites 10x12), sheet 2 "schatten" (80 sprites 14x6).
    const auto terrain = test::make_sheet({{4, 2}});
    const auto tiere = test::make_sheet(std::vector<std::pair<std::uint32_t, std::uint32_t>>(200, {10, 12}));
    const auto schatten = test::make_sheet(std::vector<std::pair<std::uint32_t, std::uint32_t>>(80, {14, 6}));
    const auto palette = test::make_palette();
    const assets::SpriteSheet* sheets[] = {&terrain, &tiere, &schatten};
    auto atlas = render::SpriteAtlas::build(sheets, palette, 512);
    REQUIRE(atlas.ok());

    render::WorldLayers layers;
    layers.figures = &figures;
    layers.atlas = &*atlas;
    layers.sheet_base[static_cast<std::size_t>(assets::FigureSheet::Tiere)] = atlas->sheet_base(1);
    layers.sheet_base[static_cast<std::size_t>(assets::FigureSheet::Schatten)] = atlas->sheet_base(2);

    sim::World world;
    const render::FigureInstance deer{10.5, 20.5, 89, 1, 2, 3};
    render::FigureSprites f;
    REQUIRE(render::land_figure_sprites(deer, world, render::WorldView{render::Zoom::Large, 0}, layers, f));
    REQUIRE(f.count == 2);
    // Point: x = (10.5 - 20.5 + 350) * 32 = 10880, y = 31 * 16 = 496.
    const render::DrawCommand& shadow = f.parts[0];
    CHECK(shadow.mode == render::DrawCommand::Mode::Shadow);
    CHECK(shadow.tint.a == render::kShadowAlpha);
    CHECK(shadow.sprite == atlas->sheet_base(2) + 8 + 3 * 8);  // no ANIM 1: direction only
    CHECK(shadow.x == 10880 - 7);
    CHECK(shadow.y == 496 - 9 - 3 + 6);  // centred 9 px (Posoffs[1]) above the point
    const render::DrawCommand& body = f.parts[1];
    CHECK(body.mode == render::DrawCommand::Mode::Normal);
    CHECK(body.sprite == atlas->sheet_base(1) + 4 + 112 + 2 + 3 * 6);
    CHECK(body.x == 10880 - 5);
    CHECK(body.y == 496 + 3);  // bottom 3 px (Posoffs[0]) below the point
    CHECK(f.row == (496 + 9 - 1) / 16);

    // Without its sheet (e.g. SGFX without derived graphics) the figure is not drawn.
    layers.sheet_base[static_cast<std::size_t>(assets::FigureSheet::Tiere)].reset();
    CHECK(!render::land_figure_sprites(deer, world, render::WorldView{render::Zoom::Large, 0}, layers, f));
}

TEST_CASE("figures: half-size sheets for zoom levels without graphics") {
    // 4 x 2 image: left 2x2 block fully opaque red 100/200, right block with one opaque pixel.
    std::vector<std::uint8_t> src(4 * 2 * 4, 0);
    const auto set = [&](std::uint32_t x, std::uint32_t y, std::uint8_t r) {
        const std::size_t p = (y * 4 + x) * 4;
        src[p] = r;
        src[p + 3] = 255;
    };
    set(0, 0, 100);
    set(1, 0, 200);
    set(0, 1, 100);
    set(1, 1, 200);
    set(3, 1, 50);
    std::vector<std::uint8_t> dst;
    std::uint32_t w = 0;
    std::uint32_t h = 0;
    render::halve_rgba(src, 4, 2, dst, w, h);
    CHECK(w == 2);
    CHECK(h == 1);
    CHECK(dst[0] == 150);  // average of the opaque pixels
    CHECK(dst[3] == 255);
    CHECK(dst[7] == 0);  // one opaque pixel of four is not enough

    // Odd sizes round up; the half-size source reports the reduced size.
    const auto sheet = test::make_sheet({{5, 3}});
    const auto palette = test::make_palette();
    const render::HalfSizeSource half(sheet, palette);
    std::uint32_t sw = 0;
    std::uint32_t sh = 0;
    half.size(0, sw, sh);
    CHECK(sw == 3);
    CHECK(sh == 2);
    std::vector<std::uint8_t> pixels;
    REQUIRE(half.pixels(0, pixels));
    CHECK(pixels.size() == 3U * 2U * 4U);
}

TEST_CASE("figures: shadows are drawn as translucent black shapes") {
    const auto sheet = test::make_sheet({{2, 1}}, 200);  // colour (200, 0, 0)
    const auto palette = test::make_palette();
    auto atlas = render::SpriteAtlas::build(sheet, palette, 64);
    REQUIRE(atlas.ok());
    render::RgbaImage target;
    target.width = 4;
    target.height = 1;
    target.pixels.assign(4 * 4, 100);
    render::DrawCommand shadow{0, 0, 1};
    shadow.mode = render::DrawCommand::Mode::Shadow;
    shadow.tint = assets::Rgba{0, 0, 0, 128};
    render::DrawCommand body{0, 2, 1};
    render::blit_draw_list({shadow, body}, *atlas, target, 0, 0);
    CHECK(target.pixels[0] == 100 * 127 / 255);  // darkened, not the sprite colour
    CHECK(target.pixels[1] == 100 * 127 / 255);
    CHECK(target.pixels[8] == 200);  // normal sprite
}
