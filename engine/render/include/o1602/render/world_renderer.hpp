// SPDX-License-Identifier: GPL-3.0-or-later
//
// Draw list for a viewport of the world map (islands + open sea), same rules as the island
// renderer (docs/re/rendering.md). Coordinates are "world pixels" of the selected zoom level:
//   left edge of tile (vx, vy) = (vx - vy + view_height - 1) * tile_width / 2
//   bottom of tile row d = vx + vy = d * tile_height / 2 + tile_height - 1
// where (vx, vy) are world coordinates after view rotation.
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/render/animation_clock.hpp"
#include "o1602/render/island_renderer.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::render {

struct Viewport {
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct WorldView {
    Zoom zoom = Zoom::Large;
    std::uint8_t rotation = 0;  ///< 0..3
};

/// Size of the whole world in world pixels (without the overhang of tall sprites above row 0).
void world_pixel_size(const WorldView& view, std::int32_t& width, std::int32_t& height);

/// World pixel position (centre of the tile diamond) of a world tile, for scrolling to it.
void world_tile_center(const WorldView& view, std::uint32_t x, std::uint32_t y, std::int32_t& px, std::int32_t& py);

/// Sprite of an open sea tile: ripple pattern (frame + 3x + y) mod AnimAnz (0x00489C90).
[[nodiscard]] std::uint32_t sea_sprite(const assets::BuildingType& sea, std::uint32_t frame, std::uint32_t x,
                                       std::uint32_t y);

/// Largest sprite of the sheet; tiles outside the viewport whose sprites may still reach into it
/// (tall buildings in rows below, wide sprites in neighbouring columns) are included.
struct SpriteExtent {
    std::int32_t width = 64;
    std::int32_t height = 64;
};

/// All sprites that may intersect the viewport, in painter's order: rows back to front, per row
/// first the flat pass (open sea, ground, normal buildings), then the Highflg pass.
[[nodiscard]] std::vector<DrawCommand> build_world_draw_list(const sim::World& world,
                                                             const assets::BuildingTable& buildings,
                                                             const AnimationClock& clock, const WorldView& view,
                                                             const Viewport& viewport, const SpriteExtent& extent);

/// Same, but fills `out` (cleared first) so a per-frame caller can reuse its allocation.
void build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings, const AnimationClock& clock,
                           const WorldView& view, const Viewport& viewport, const SpriteExtent& extent,
                           std::vector<DrawCommand>& out);

}  // namespace o1602::render
