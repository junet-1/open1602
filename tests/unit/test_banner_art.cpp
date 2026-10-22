// SPDX-License-Identifier: GPL-3.0-or-later
// Interface art in any player colour (render/banner.hpp BannerArt, render/variant_tint.hpp): the
// drawn variant for the original's colours, the white one recoloured for every other banner.

#include <array>
#include <cstdint>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/render/banner.hpp"
#include "o1602/render/flag_tint.hpp"
#include "o1602/render/variant_tint.hpp"

using namespace o1602;

namespace {

/// A solid RGBA image.
std::vector<std::uint8_t> solid(std::uint32_t w, std::uint32_t h, assets::Rgba c) {
    std::vector<std::uint8_t> rgba;
    for (std::uint32_t i = 0; i < w * h; ++i) {
        rgba.insert(rgba.end(), {c.r, c.g, c.b, c.a});
    }
    return rgba;
}

void put(std::vector<std::uint8_t>& rgba, std::uint32_t w, std::uint32_t x, std::uint32_t y, assets::Rgba c) {
    const std::size_t i = (std::size_t{y} * w + x) * 4;
    rgba[i] = c.r;
    rgba[i + 1] = c.g;
    rgba[i + 2] = c.b;
    rgba[i + 3] = c.a;
}

assets::Rgba at(const std::vector<std::uint8_t>& rgba, std::uint32_t w, std::uint32_t x, std::uint32_t y) {
    const std::size_t i = (std::size_t{y} * w + x) * 4;
    return {rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]};
}

}  // namespace

TEST_CASE("banner art: the original colours show their drawn variant, the others the white one tinted") {
    auto banners = render::BannerTable::original();
    banners.add_more_players();
    render::BannerArt art = render::BannerArt::at(548);
    for (sim::BannerId b = 0; b < 4; ++b) {
        CHECK(render::banner_art(art, banners.get(b)) == (render::BannerArtSprite{548 + b, std::nullopt}));
    }
    // Players five on, and natives and the trader (no art of their own): white, in their colour.
    for (const sim::BannerId b :
         {sim::BannerId{5}, sim::BannerId{6}, sim::kFirstExtraBanner, static_cast<sim::BannerId>(banners.size() - 1)}) {
        const render::Banner* banner = banners.get(b);
        REQUIRE(banner != nullptr);
        CHECK(render::banner_art(art, banner) == (render::BannerArtSprite{551, banner->color}));
    }
    // No banner: the white art as drawn.
    CHECK(render::banner_art(art, nullptr) == (render::BannerArtSprite{551, std::nullopt}));

    // A sprite of one banner's own: the pirates' skull among the military message icons.
    art.own = {{"anno1602:pirates", 4}};
    CHECK(render::banner_art(art, banners.get(4)) == (render::BannerArtSprite{552, std::nullopt}));
    // Variants further apart.
    render::BannerArt spread = render::BannerArt::at(100);
    spread.stride = 12;
    CHECK(render::banner_art(spread, banners.get(2)).gfx == 124);
    CHECK(render::banner_art(spread, banners.get(sim::kFirstExtraBanner)).gfx == 136);
}

TEST_CASE("banner art: tinting takes the colour where all variants differ and the white one is grey") {
    // 6 x 6: a light grey field in the white variant, solid other colours in the others; a motif
    // (2 x 2 orange at 1,1) the same in all; a dark outline pixel and a lone differing pixel.
    constexpr std::uint32_t w = 6;
    constexpr std::uint32_t h = 6;
    const assets::Rgba grey{200, 200, 200, 255};
    const assets::Rgba motif{230, 90, 20, 255};
    std::array<std::vector<std::uint8_t>, 4> v{solid(w, h, {216, 51, 8, 255}), solid(w, h, {0, 0, 241, 255}),
                                               solid(w, h, {255, 219, 57, 255}), solid(w, h, grey)};
    for (auto& img : v) {
        for (std::uint32_t y = 1; y < 3; ++y) {
            for (std::uint32_t x = 1; x < 3; ++x) {
                put(img, w, x, y, motif);
            }
        }
    }
    put(v[3], w, 5, 0, {30, 30, 30, 255});     // outline: too dark
    put(v[0], w, 5, 5, {200, 200, 200, 255});  // the red variant agrees with the white one here
    const std::array<render::RgbaView, 4> views{
        render::RgbaView{v[0].data(), w, h}, render::RgbaView{v[1].data(), w, h}, render::RgbaView{v[2].data(), w, h},
        render::RgbaView{v[3].data(), w, h}};
    const assets::Rgba green{0, 230, 90, 255};
    std::vector<std::uint8_t> out;
    render::tint_variant(views, 3, green, out);
    REQUIRE(out.size() == std::size_t{w} * h * 4);
    const assets::Rgba cloth = render::tint_shade(101, green);  // 0.48 * 200 + 5
    CHECK(at(out, w, 4, 3) == cloth);
    CHECK(at(out, w, 0, 5) == cloth);
    CHECK(at(out, w, 1, 1) == motif);                            // the same in all variants
    CHECK(at(out, w, 5, 0) == (assets::Rgba{30, 30, 30, 255}));  // outline
    CHECK(at(out, w, 5, 5) == grey);                             // not all variants differ

    // A pixel of cloth alone (fewer than three cloth neighbours) is dithering, not cloth.
    std::array<std::vector<std::uint8_t>, 2> lone{solid(w, h, motif), solid(w, h, motif)};
    put(lone[0], w, 3, 3, {10, 10, 200, 255});
    put(lone[1], w, 3, 3, grey);
    put(lone[0], w, 4, 3, {10, 10, 200, 255});
    put(lone[1], w, 4, 3, grey);
    const std::array<render::RgbaView, 2> lone_views{render::RgbaView{lone[0].data(), w, h},
                                                     render::RgbaView{lone[1].data(), w, h}};
    render::tint_variant(lone_views, 1, green, out);
    CHECK(at(out, w, 3, 3) == grey);

    // Variants of another size are left out; with none left the base is as drawn.
    const std::vector<std::uint8_t> small = solid(2, 2, {0, 0, 0, 255});
    const std::array<render::RgbaView, 2> other_size{render::RgbaView{small.data(), 2, 2}, views[3]};
    render::tint_variant(other_size, 1, green, out);
    CHECK(out == v[3]);
    render::tint_variant(other_size, 5, green, out);
    CHECK(out.empty());
}
