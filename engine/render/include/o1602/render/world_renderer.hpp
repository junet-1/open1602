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
#include "o1602/assets/figure_table.hpp"
#include "o1602/render/animation_clock.hpp"
#include "o1602/render/island_renderer.hpp"
#include "o1602/render/sprite_atlas.hpp"
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

/// Everything besides the tiles that goes into the world draw list.
struct WorldLayers {
    SpriteExtent extent;
    /// Ships with their player flags. Needs the figure table and an atlas containing SHIP.BSH of the
    /// current zoom level at `ship_base` (sizes are taken from it, figures are centred).
    const assets::FigureTable* figures = nullptr;
    const SpriteAtlas* atlas = nullptr;
    std::uint32_t ship_base = 0;
    /// Ownership view: tiles owned by a player get DrawCommand::tint = the owner's colour.
    bool owner_tint = false;
};

/// Draw list including ships (drawn after the flat pass of their tile row, like the figures in
/// the original, 0x0044FA90) and the ownership tint.
void build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings, const AnimationClock& clock,
                           const WorldView& view, const Viewport& viewport, const WorldLayers& layers,
                           std::vector<DrawCommand>& out);

/// Sprite, position and flag of one ship (exposed for tests).
struct ShipSprites {
    std::int32_t row = 0;  ///< tile row (view space) after whose flat pass the ship is drawn
    DrawCommand hull;
    DrawCommand flag;
    bool has_flag = false;
};
[[nodiscard]] bool ship_sprites(const sim::Ship& ship, const sim::World& world, const WorldView& view,
                                const WorldLayers& layers, std::uint64_t time_ms, ShipSprites& out);

/// Same, but fills `out` (cleared first) so a per-frame caller can reuse its allocation.
void build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings, const AnimationClock& clock,
                           const WorldView& view, const Viewport& viewport, const SpriteExtent& extent,
                           std::vector<DrawCommand>& out);

}  // namespace o1602::render
