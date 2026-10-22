// SPDX-License-Identifier: GPL-3.0-or-later
// The canvas of the free zoom (engine/render/sdl/sdl_map_canvas.cpp): sprites drawn at a whole
// scale into a texture, which is shrunk onto a software renderer's output and read back.

#include <array>
#include <vector>

#include "../test_framework.hpp"
#include "../unit/test_support.hpp"
#include "sdl_support.hpp"

#include "o1602/render/sdl_map_canvas.hpp"
#include "o1602/render/sdl_sprite_renderer.hpp"
#include "o1602/render/sprite_atlas.hpp"

using namespace o1602;

namespace {

const assets::Rgba kBackground{0, 0, 200, 255};
const assets::Rgba kBlack{0, 0, 0, 255};
const assets::Rgba kRed50{50, 0, 0, 255};  // test::make_palette: colour i is (i, 0, 0)

render::SpriteAtlas block_atlas() {
    const auto sheet = test::make_sheet({{8, 8}}, 50);
    const std::array<const assets::SpriteSheet*, 1> sheets{&sheet};
    auto atlas = render::SpriteAtlas::build(sheets, test::make_palette(), 64);
    REQUIRE(atlas.ok());
    return std::move(atlas).value();
}

render::DrawCommand block(std::int32_t x, std::int32_t y) {
    render::DrawCommand c;
    c.sprite = 0;
    c.x = x;
    c.y = y;
    return c;
}

}  // namespace

TEST_CASE("sdl map canvas: a direct plan draws straight onto the screen") {
    test::Canvas canvas(32, 32);
    render::SdlMapCanvas map(canvas.renderer());
    canvas.clear(kBackground);
    render::RenderPlan plan;  // direct
    CHECK(map.begin(plan));
    CHECK(!map.used());
    map.end();  // nothing to do
    canvas.read();
    CHECK(canvas.at(5, 5) == kBackground);
}

TEST_CASE("sdl map canvas: a scale of 1.5 is drawn at 2 and shrunk by 0.75") {
    test::Canvas canvas(64, 64);
    const auto atlas = block_atlas();
    render::SdlSpriteRenderer sprites(canvas.renderer());
    REQUIRE(sprites.upload(atlas).ok());
    render::SdlMapCanvas map(canvas.renderer());
    canvas.clear(kBackground);
    render::RenderPlan plan;
    plan.direct = false;
    plan.scale = 2;
    plan.width = 40;  // 30 screen pixels
    plan.height = 40;
    plan.dst_scale = 0.75;
    REQUIRE(map.begin(plan));
    CHECK(map.used());
    // The 8 x 8 block with its bottom left at world pixel (4, 16): texture pixels 8..24 x 16..32,
    // screen pixels 6..18 x 12..24.
    CHECK(sprites.draw({block(4, 16)}, plan.origin_x, plan.origin_y, plan.scale) == 1);
    map.end();
    canvas.read();
    for (const auto& [x, y] : {std::pair{7, 13}, std::pair{12, 18}, std::pair{17, 23}}) {
        CHECK(test::near(canvas.at(x, y), kRed50));
    }
    CHECK(canvas.at(3, 3) == kBlack);  // the canvas, cleared
    CHECK(canvas.at(20, 20) == kBlack);
    CHECK(canvas.at(40, 40) == kBackground);  // beyond the canvas: the screen as it was
    CHECK(canvas.at(31, 10) == kBackground);
}

TEST_CASE("sdl map canvas: the canvas moves by its offset and grows with the plan") {
    test::Canvas canvas(64, 64);
    const auto atlas = block_atlas();
    render::SdlSpriteRenderer sprites(canvas.renderer());
    REQUIRE(sprites.upload(atlas).ok());
    render::SdlMapCanvas map(canvas.renderer());
    render::RenderPlan plan;
    plan.direct = false;
    plan.scale = 1;
    plan.width = 10;
    plan.height = 10;
    plan.dst_scale = 1.0;
    REQUIRE(map.begin(plan));
    map.end();
    // Larger the next frame, and shifted by whole screen pixels: the block lands there.
    plan.width = 60;
    plan.height = 60;
    plan.dst_x = 10.0;
    plan.dst_y = 4.0;
    canvas.clear(kBackground);
    REQUIRE(map.begin(plan));
    CHECK(sprites.draw({block(0, 8)}, 0, 0, 1) == 1);
    map.end();
    canvas.read();
    CHECK(canvas.at(10, 4) == kRed50);
    CHECK(canvas.at(17, 11) == kRed50);
    CHECK(canvas.at(18, 12) == kBlack);
    CHECK(canvas.at(9, 4) == kBackground);
}
