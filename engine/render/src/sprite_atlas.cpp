// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/sprite_atlas.hpp"

#include <algorithm>

#include "o1602/render/player_colors.hpp"

namespace o1602::render {

using core::ErrorCode;
using core::make_error;

core::Result<SpriteAtlas> SpriteAtlas::build(const assets::SpriteSheet& sheet, const assets::Palette& palette,
                                             std::uint32_t page_size) {
    const assets::SpriteSheet* sheets[] = {&sheet};
    return build(sheets, palette, page_size);
}

core::Result<SpriteAtlas> SpriteAtlas::build(std::span<const assets::SpriteSheet* const> sheets,
                                             const assets::Palette& palette, std::uint32_t page_size) {
    constexpr std::uint32_t kPad = 1;
    if (page_size == 0 || page_size > 0xFFFF) {
        return make_error(ErrorCode::OutOfRange, "invalid atlas page size");
    }
    SpriteAtlas atlas;
    atlas.m_page_size = page_size;

    struct Entry {
        std::uint32_t index;  ///< global sprite index
        const assets::SpriteSheet* sheet;
        std::uint32_t local;
    };
    std::vector<Entry> entries;
    std::uint32_t total = 0;
    for (const assets::SpriteSheet* sheet : sheets) {
        atlas.m_sheet_bases.push_back(total);
        for (std::uint32_t i = 0; i < sheet->size(); ++i) {
            entries.push_back(Entry{total + i, sheet, i});
        }
        total += static_cast<std::uint32_t>(sheet->size());
    }
    atlas.m_rects.resize(total);

    // Tallest first gives dense shelves; ties by width and index keep the order deterministic.
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        const auto& ia = a.sheet->info(a.local);
        const auto& ib = b.sheet->info(b.local);
        return ia.height != ib.height ? ia.height > ib.height : ia.width > ib.width;
    });

    std::uint32_t shelf_x = 0;
    std::uint32_t shelf_y = 0;
    std::uint32_t shelf_h = 0;
    const auto new_page = [&] {
        RgbaImage page;
        page.width = page_size;
        page.height = page_size;
        page.pixels.assign(std::size_t{page_size} * page_size * 4, 0);
        atlas.m_pages.push_back(std::move(page));
        shelf_x = shelf_y = shelf_h = 0;
    };

    for (const Entry& e : entries) {
        const auto& info = e.sheet->info(e.local);
        if (info.width == 0 || info.height == 0) {
            continue;
        }
        if (info.width + kPad > page_size || info.height + kPad > page_size) {
            return make_error(ErrorCode::OutOfRange,
                              "sprite " + std::to_string(e.index) + " does not fit into an atlas page");
        }
        auto image = e.sheet->decode(e.local);
        if (!image) {
            continue;  // corrupt sprite: drawn as missing
        }
        if (atlas.m_pages.empty()) {
            new_page();
        }
        if (shelf_x + info.width + kPad > page_size) {  // next shelf
            shelf_y += shelf_h;
            shelf_x = 0;
            shelf_h = 0;
        }
        if (shelf_y + info.height + kPad > page_size) {
            new_page();
        }
        RgbaImage& page = atlas.m_pages.back();
        for (std::uint32_t y = 0; y < image->height; ++y) {
            for (std::uint32_t x = 0; x < image->width; ++x) {
                const std::size_t src = std::size_t{y} * image->width + x;
                if (image->opaque[src] == 0) {
                    continue;
                }
                const assets::Rgba& c = palette[image->indices[src]];
                const std::size_t dst = ((std::size_t{shelf_y} + y) * page_size + shelf_x + x) * 4;
                page.pixels[dst] = c.r;
                page.pixels[dst + 1] = c.g;
                page.pixels[dst + 2] = c.b;
                page.pixels[dst + 3] = 255;
            }
        }
        atlas.m_rects[e.index] =
            AtlasRect{static_cast<std::uint16_t>(atlas.m_pages.size() - 1), static_cast<std::uint16_t>(shelf_x),
                      static_cast<std::uint16_t>(shelf_y), static_cast<std::uint16_t>(info.width),
                      static_cast<std::uint16_t>(info.height)};
        atlas.m_max_width = std::max(atlas.m_max_width, static_cast<std::int32_t>(info.width));
        atlas.m_max_height = std::max(atlas.m_max_height, static_cast<std::int32_t>(info.height));
        shelf_x += info.width + kPad;
        shelf_h = std::max(shelf_h, info.height + kPad);
    }
    return atlas;
}

void SpriteAtlas::drop_pixels() noexcept {
    for (RgbaImage& page : m_pages) {
        page.pixels = {};
    }
}

namespace {

/// Translucent tile diamond: width w, height w/2, bounding box left = x, bottom = y.
void fill_diamond(RgbaImage& target, std::int32_t x, std::int32_t y, std::int32_t w, assets::Rgba color,
                  std::uint8_t alpha) {
    const std::int32_t h = w / 2;
    const std::int32_t top = y - h;
    for (std::int32_t row = 0; row < h; ++row) {
        const std::int32_t ty = top + row;
        if (ty < 0 || ty >= static_cast<std::int32_t>(target.height)) {
            continue;
        }
        // Half width at this row: widest in the middle.
        const std::int32_t d = row < h / 2 ? row : h - 1 - row;
        const std::int32_t half = (d + 1) * 2;
        const std::int32_t cx = x + w / 2;
        const std::int32_t x0 = std::max(0, cx - half);
        const std::int32_t x1 = std::min(static_cast<std::int32_t>(target.width), cx + half);
        for (std::int32_t tx = x0; tx < x1; ++tx) {
            std::uint8_t* p =
                &target.pixels[(static_cast<std::size_t>(ty) * target.width + static_cast<std::size_t>(tx)) * 4];
            p[0] = static_cast<std::uint8_t>((p[0] * (255 - alpha) + color.r * alpha) / 255);
            p[1] = static_cast<std::uint8_t>((p[1] * (255 - alpha) + color.g * alpha) / 255);
            p[2] = static_cast<std::uint8_t>((p[2] * (255 - alpha) + color.b * alpha) / 255);
        }
    }
}

}  // namespace

void blit_draw_list(const std::vector<DrawCommand>& commands, const SpriteAtlas& atlas, RgbaImage& target,
                    std::int32_t origin_x, std::int32_t origin_y) {
    const auto tw = static_cast<std::int32_t>(target.width);
    const auto th = static_cast<std::int32_t>(target.height);
    const std::uint32_t ps = atlas.page_size();
    for (const DrawCommand& c : commands) {
        if (c.sprite == DrawCommand::kOverlayDiamond) {
            fill_diamond(target, c.x - origin_x, c.y - origin_y, c.tint_size,
                         player_rgb(static_cast<assets::PlayerColor>(c.tint)), kOverlayAlpha);
            continue;
        }
        const AtlasRect& r = atlas.rect(c.sprite);
        if (r.empty()) {
            continue;
        }
        const std::int32_t left = c.x - origin_x;
        const std::int32_t top = c.y - r.height - origin_y;
        const std::int32_t x0 = std::max(0, -left);
        const std::int32_t y0 = std::max(0, -top);
        const std::int32_t x1 = std::min<std::int32_t>(r.width, tw - left);
        const std::int32_t y1 = std::min<std::int32_t>(r.height, th - top);
        const std::vector<std::uint8_t>& page = atlas.pages()[r.page].pixels;
        if (page.empty()) {
            continue;  // pixels were dropped
        }
        for (std::int32_t y = y0; y < y1; ++y) {
            const std::uint8_t* src = &page[((std::size_t{r.y} + static_cast<std::size_t>(y)) * ps + r.x) * 4];
            std::uint8_t* dst = &target.pixels[(static_cast<std::size_t>(top + y) * target.width) * 4];
            for (std::int32_t x = x0; x < x1; ++x) {
                const std::uint8_t* s = src + static_cast<std::size_t>(x) * 4;
                if (s[3] == 0) {
                    continue;
                }
                std::copy_n(s, 4, dst + static_cast<std::size_t>(left + x) * 4);
            }
        }
    }
}

}  // namespace o1602::render
