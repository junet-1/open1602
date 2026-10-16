// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/world_renderer.hpp"

#include <algorithm>

namespace o1602::render {

namespace {

constexpr std::int32_t floor_div(std::int32_t a, std::int32_t b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

struct RotatedWorld {
    std::uint32_t width;  ///< size in view space
    std::uint32_t height;
    std::uint8_t rotation;

    explicit RotatedWorld(std::uint8_t r)
        : width((r & 1U) != 0 ? sim::World::kHeight : sim::World::kWidth),
          height((r & 1U) != 0 ? sim::World::kWidth : sim::World::kHeight),
          rotation(static_cast<std::uint8_t>(r & 3U)) {}

    /// View space -> world coordinates (same convention as the island renderer).
    void to_world(std::uint32_t vx, std::uint32_t vy, std::uint32_t& wx, std::uint32_t& wy) const {
        constexpr std::uint32_t W = sim::World::kWidth;
        constexpr std::uint32_t H = sim::World::kHeight;
        switch (rotation) {
            case 0:
                wx = vx;
                wy = vy;
                break;
            case 1:
                wx = vy;
                wy = H - 1 - vx;
                break;
            case 2:
                wx = W - 1 - vx;
                wy = H - 1 - vy;
                break;
            default:
                wx = W - 1 - vy;
                wy = vx;
                break;
        }
    }

    void to_view(std::uint32_t wx, std::uint32_t wy, std::uint32_t& vx, std::uint32_t& vy) const {
        constexpr std::uint32_t W = sim::World::kWidth;
        constexpr std::uint32_t H = sim::World::kHeight;
        switch (rotation) {
            case 0:
                vx = wx;
                vy = wy;
                break;
            case 1:
                vx = H - 1 - wy;
                vy = wx;
                break;
            case 2:
                vx = W - 1 - wx;
                vy = H - 1 - wy;
                break;
            default:
                vx = wy;
                vy = W - 1 - wx;
                break;
        }
    }
};

}  // namespace

void world_pixel_size(const WorldView& view, std::int32_t& width, std::int32_t& height) {
    const RotatedWorld rw(view.rotation);
    const auto diag = static_cast<std::int32_t>(rw.width + rw.height);
    width = diag * (tile_width(view.zoom) / 2);
    height = diag * (tile_height(view.zoom) / 2);
}

void world_tile_center(const WorldView& view, std::uint32_t x, std::uint32_t y, std::int32_t& px, std::int32_t& py) {
    const RotatedWorld rw(view.rotation);
    std::uint32_t vx = 0;
    std::uint32_t vy = 0;
    rw.to_view(x, y, vx, vy);
    const std::int32_t tw = tile_width(view.zoom);
    const std::int32_t th = tile_height(view.zoom);
    px = (static_cast<std::int32_t>(vx) - static_cast<std::int32_t>(vy) + static_cast<std::int32_t>(rw.height) - 1) *
             (tw / 2) +
         tw / 2;
    py = static_cast<std::int32_t>(vx + vy) * (th / 2) + th / 2;
}

std::uint32_t sea_sprite(const assets::BuildingType& sea, std::uint32_t frame, std::uint32_t x, std::uint32_t y) {
    std::uint32_t sprite = sea.gfx;
    if (sea.anim_count > 1) {
        sprite += ((frame + x * 3 + y) % sea.anim_count) * sea.anim_add;
    }
    return sprite;
}

std::vector<DrawCommand> build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings,
                                               const AnimationClock& clock, const WorldView& view,
                                               const Viewport& viewport, const SpriteExtent& extent) {
    std::vector<DrawCommand> commands;
    build_world_draw_list(world, buildings, clock, view, viewport, extent, commands);
    return commands;
}

void build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings, const AnimationClock& clock,
                           const WorldView& view, const Viewport& viewport, const SpriteExtent& extent,
                           std::vector<DrawCommand>& commands) {
    commands.clear();
    if (viewport.width <= 0 || viewport.height <= 0) {
        return;
    }
    const RotatedWorld rw(view.rotation);
    const std::int32_t tw = tile_width(view.zoom);
    const std::int32_t th = tile_height(view.zoom);
    const std::int32_t half_w = tw / 2;
    const std::int32_t half_h = th / 2;
    const auto view_h = static_cast<std::int32_t>(rw.height);
    const auto view_w = static_cast<std::int32_t>(rw.width);
    const std::int32_t rows = view_w + view_h - 1;
    const std::int32_t max_elevation = tw;  // Posoffs is at most 64 * tw / 64 (land uses 20)

    // Rows: the bottom of row d is d * half_h + th - 1; its sprites extend upwards by at most
    // max_height + max_elevation pixels.
    const std::int32_t d_begin = std::max(0, floor_div(viewport.top - (th - 1), half_h));
    const std::int32_t d_end = std::min(
        rows - 1, floor_div(viewport.top + viewport.height + extent.height + max_elevation - (th - 1), half_h));
    if (d_begin > d_end) {
        return;
    }
    commands.reserve(static_cast<std::size_t>((viewport.width + extent.width) / half_w + 2) *
                     static_cast<std::size_t>(d_end - d_begin + 1) * 5 / 8);

    const assets::BuildingType* sea = buildings.open_sea();
    const std::uint32_t sea_frame = sea != nullptr ? clock.frame(*sea) : 0;
    const std::int32_t sea_elevation = sea != nullptr ? sea->posoffs * tw / 64 : 0;

    const auto emit = [&](const sim::MapCell& cell, std::int32_t sx, std::int32_t row_bottom) {
        const assets::BuildingType* b = buildings.by_sprite(cell.sprite);
        if (b == nullptr) {
            commands.push_back(DrawCommand{cell.sprite, sx, row_bottom});
            return;
        }
        commands.push_back(DrawCommand{animated_sprite(cell, b, clock.frame(*b), view.rotation), sx,
                                       row_bottom - b->posoffs * tw / 64});
    };

    for (std::int32_t d = d_begin; d <= d_end; ++d) {
        const std::int32_t row_bottom = d * half_h + th - 1;
        // Left edge of tile vx in this row: X = (2 * vx - d + view_h - 1) * half_w.
        // Visible if X + max_width > left and X < left + width (conservative bounds).
        const std::int32_t k_lo = floor_div(viewport.left - extent.width, half_w) + d - view_h + 1;
        const std::int32_t k_hi = floor_div(viewport.left + viewport.width, half_w) + d - view_h + 1;
        const std::int32_t vx_lo = std::max({0, d - view_h + 1, -floor_div(-k_lo, 2)});
        const std::int32_t vx_hi = std::min({view_w - 1, d, floor_div(k_hi, 2)});
        for (int pass = 0; pass < 2; ++pass) {
            const bool high_pass = pass == 1;
            for (std::int32_t vx = vx_lo; vx <= vx_hi; ++vx) {
                const std::int32_t vy = d - vx;
                const std::int32_t sx = (vx - vy + view_h - 1) * half_w;
                std::uint32_t wx = 0;
                std::uint32_t wy = 0;
                rw.to_world(static_cast<std::uint32_t>(vx), static_cast<std::uint32_t>(vy), wx, wy);
                const sim::World::Cell& wc = world.cell(wx, wy);
                const sim::IslandMap* map = nullptr;
                const sim::MapCell* cell = nullptr;
                if (wc.island != sim::World::kSea) {
                    map = &world.islands()[wc.island].map;
                    cell = &map->top(wc.local_x, wc.local_y);
                }
                if (cell == nullptr || cell->empty()) {  // open sea, always flat
                    if (!high_pass && sea != nullptr) {
                        commands.push_back(
                            DrawCommand{sea_sprite(*sea, sea_frame, wx, wy), sx, row_bottom - sea_elevation});
                    }
                    continue;
                }
                const assets::BuildingType* b = buildings.by_sprite(cell->sprite);
                if ((b != nullptr && b->high) != high_pass) {
                    continue;
                }
                if (b != nullptr && b->draw_ground) {
                    const sim::MapCell& ground = map->ground(wc.local_x, wc.local_y);
                    if (!ground.empty()) {
                        emit(ground, sx, row_bottom);
                    }
                }
                emit(*cell, sx, row_bottom);
            }
        }
    }
}

}  // namespace o1602::render
