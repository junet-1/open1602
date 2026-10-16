// SPDX-License-Identifier: GPL-3.0-or-later
//
// Draw list for a viewport of the world map (islands + open sea), same rules as the island
// renderer (docs/re/rendering.md). Coordinates are "world pixels" of the selected zoom level:
//   left edge of tile (vx, vy) = (vx - vy + view_height - 1) * tile_width / 2
//   bottom of tile row d = vx + vy = d * tile_height / 2 + tile_height - 1
// where (vx, vy) are world coordinates after view rotation.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/render/animation_clock.hpp"
#include "o1602/render/banner.hpp"
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
    /// Size of the world in tiles (a property of the map, ADR 0007). Functions that receive the
    /// world as well take its size from there.
    std::uint32_t world_width = sim::World::kOriginalWidth;
    std::uint32_t world_height = sim::World::kOriginalHeight;
};

/// `view` with the size of `world`.
[[nodiscard]] inline WorldView fit_view(WorldView view, const sim::World& world) noexcept {
    view.world_width = world.width();
    view.world_height = world.height();
    return view;
}

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

/// Flag on a warehouse (HAUS_PRODTYP Kind KONTOR), market (MARKT) or tower (WACHTURM) in the colour
/// of its owner.
/// The original spawns these as display figures for visible buildings every frame (0x00486DA0 ->
/// 0x004466A0, kind 0x1D); buildings do not move, so Open1602 collects them once per world.
struct BuildingFlag {
    double x = 0.0;  ///< world position in tiles (continuous): building centre + rotated Fahnoffs
    double y = 0.0;
    double z = 0.0;            ///< height: Fahnoffs[2] in half tile widths
    std::uint16_t figure = 0;  ///< flag figure (FAHNEMARKT, FAHNEKONTOR1..4)
    sim::PlayerId owner = sim::kNoPlayer;
    std::uint8_t phase = 0;  ///< animation offset so neighbouring flags do not wave in lockstep
};

[[nodiscard]] std::vector<BuildingFlag> collect_building_flags(const sim::World& world,
                                                               const assets::BuildingTable& buildings,
                                                               const assets::FigureTable& figures);

/// A figure standing or walking on land (animals now; carriers and soldiers later) as the
/// simulation or the viewer hands it to the renderer.
struct FigureInstance {
    double x = 0.0;  ///< world position in tiles (continuous; tile centre = x + 0.5)
    double y = 0.0;
    std::uint16_t figure = 0;  ///< FIGUR number
    std::uint8_t anim = 0;     ///< ANIM index and frame (AnimState)
    std::uint8_t frame = 0;
    std::uint8_t direction = 0;  ///< 0..7
};

/// Opacity of figure shadows (the original dithers every second pixel).
inline constexpr std::uint8_t kShadowAlpha = 128;

/// Everything besides the tiles that goes into the world draw list.
struct WorldLayers {
    SpriteExtent extent;
    /// Ships with their player flags. Needs the figure table and an atlas containing SHIP.BSH of the
    /// current zoom level at `ship_base` (sizes are taken from it, figures are centred).
    const assets::FigureTable* figures = nullptr;
    const SpriteAtlas* atlas = nullptr;
    std::uint32_t ship_base = 0;
    /// Colours and flags of the players' banners. Without it there are no flags and no ownership view.
    const BannerTable* banners = nullptr;
    /// Ownership view: tiles owned by a player get a diamond in the colour of the owner's banner.
    bool owner_tint = false;
    /// Warehouse, market and tower flags (collect_building_flags); drawn like ships (same sheet).
    const std::vector<BuildingFlag>* building_flags = nullptr;
    /// Figures on land with their shadows. Needs `figures` and the atlas bases of their sheets
    /// (`sheet_base`, e.g. from WorldGraphics::base); figures whose sheet is missing are skipped.
    const std::vector<FigureInstance>* land_figures = nullptr;
    std::array<std::optional<std::uint32_t>, assets::kFigureSheetCount> sheet_base{};
};

/// Up to two draw commands of one figure (shadow + body, hull + flag) and the tile row after
/// whose flat pass they are drawn.
struct FigureSprites {
    std::int32_t row = 0;
    std::array<DrawCommand, 2> parts{};
    std::uint8_t count = 0;
};

/// Shadow and body of a land figure (exposed for tests). Body: Gfx + animation (0x0044F8F0),
/// centred, bottom at y + Posoffs[0] * w/64; shadow: figure `Schatten` from SCHATTEN.BSH, centred
/// on y - Posoffs[1] * w/64 (cases 0x0C-0x0E of 0x0044FA90).
[[nodiscard]] bool land_figure_sprites(const FigureInstance& figure, const sim::World& world, const WorldView& view,
                                       const WorldLayers& layers, FigureSprites& out);

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

/// Sprite and row of a building flag (exposed for tests); the flag is returned in `out.hull`.
[[nodiscard]] bool building_flag_sprite(const BuildingFlag& flag, const sim::World& world, const WorldView& view,
                                        const WorldLayers& layers, std::uint64_t time_ms, ShipSprites& out);

/// Same, but fills `out` (cleared first) so a per-frame caller can reuse its allocation.
void build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings, const AnimationClock& clock,
                           const WorldView& view, const Viewport& viewport, const SpriteExtent& extent,
                           std::vector<DrawCommand>& out);

}  // namespace o1602::render
