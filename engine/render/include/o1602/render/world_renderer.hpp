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
#include "o1602/render/unit_tint.hpp"
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

/// Figures the original spawns on buildings every frame (Buildings_SpawnDisplayFigures, 0x00486B60
/// -> 0x004466A0). Buildings do not move, so Open1602 collects them once per world; what depends on
/// the building's state (smoke, status icons) is decided when drawing.
enum class BuildingFigureRole : std::uint8_t {
    Flag,    ///< warehouse, market, tower (kind 0x1D): flag in the colour of the owner's banner
    Smoke,   ///< chimney of a workshop (kind 0x17): rises while the building works, translucent
    Status,  ///< icon above a production building of the local player (kind 0x16, NOAKTIV):
             ///< switched off or short of raw material
};

struct BuildingFigure {
    BuildingFigureRole role = BuildingFigureRole::Flag;
    double x = 0.0;  ///< world position in tiles (continuous): building centre + rotated Fahnoffs
    double y = 0.0;
    double z = 0.0;                        ///< height: Fahnoffs[2] in half tile widths
    std::uint16_t figure = 0;              ///< FIGUR number (FAHNEMARKT, RAUCHBAECK, NOAKTIV, ...)
    sim::PlayerId owner = sim::kNoPlayer;  ///< owner of the city the building belongs to
    std::uint8_t phase = 0;                ///< animation offset so neighbouring figures do not move in lockstep
    std::uint16_t island = 0;              ///< the building (index into World::islands(), origin tile) for its state
    std::uint16_t origin_x = 0;
    std::uint16_t origin_y = 0;
};

/// Flags (HAUS_PRODTYP Kind KONTOR, MARKT, WACHTURM), smoke (workshops with Rauchfignr) and status
/// icons (all production buildings, kinds 1..6) of buildings inside a city.
[[nodiscard]] std::vector<BuildingFigure> collect_building_figures(const sim::World& world,
                                                                   const assets::BuildingTable& buildings,
                                                                   const assets::FigureTable& figures);

/// A figure standing or walking on land (animals, soldiers; carriers later) as the simulation or
/// the viewer hands it to the renderer.
struct FigureInstance {
    double x = 0.0;  ///< world position in tiles (continuous; tile centre = x + 0.5)
    double y = 0.0;
    std::uint16_t figure = 0;  ///< FIGUR number
    std::uint8_t anim = 0;     ///< ANIM index and frame (AnimState)
    std::uint8_t frame = 0;
    std::uint8_t direction = 0;  ///< 0..7
    /// Owner: units drawn per colour (FigureTable::unit_variant) take the colour of the owner's
    /// banner - the drawn variant or the tinted template.
    sim::PlayerId owner = sim::kNoPlayer;
    /// Sheet of the images. The original picks it by the figure's draw kind (soldiers: SOLDAT.BSH),
    /// not by Blocknr, which many figures lack; nothing = Blocknr.
    std::optional<assets::FigureSheet> sheet;
};

/// Opacity of figure shadows (the original dithers every second pixel).
inline constexpr std::uint8_t kShadowAlpha = 128;
/// Opacity of chimney smoke (the original draws every second pixel, 0x004048E0).
inline constexpr std::uint8_t kSmokeAlpha = 128;

/// A tile marked with a translucent diamond, e.g. the red ground under a building that cannot be
/// built there.
struct TileMark {
    std::uint16_t island = 0;  ///< index into World::islands()
    std::uint16_t x = 0;       ///< island tile
    std::uint16_t y = 0;
    assets::Rgba color;  ///< alpha = opacity
};

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
    /// Atlas index of each banner's tinted flag (WorldGraphics::banner_flags).
    const std::vector<std::optional<std::uint32_t>>* banner_flags = nullptr;
    /// Ownership view: tiles owned by a player get a diamond in the colour of the owner's banner.
    bool owner_tint = false;
    /// Single marked tiles, drawn on top of everything.
    const std::vector<TileMark>* tile_marks = nullptr;
    /// Flags, smoke and status icons of buildings (collect_building_figures). Smoke and icons need
    /// the atlas bases of their sheets (`sheet_base`) and follow the buildings' production state.
    const std::vector<BuildingFigure>* building_figures = nullptr;
    /// Player whose buildings show status icons (the original: the local player). kNoPlayer: none.
    sim::PlayerId local_player = sim::kNoPlayer;
    /// Sets of figures on land (animals, soldiers, ...) with their shadows. Needs `figures` and the
    /// atlas bases of their sheets (`sheet_base`, e.g. from WorldGraphics::base); figures whose sheet
    /// is missing are skipped.
    std::vector<const std::vector<FigureInstance>*> land_figures;
    std::array<std::optional<std::uint32_t>, assets::kFigureSheetCount> sheet_base{};
    /// Tinted units for banners without drawn variants (WorldGraphics::unit_tint).
    const UnitTint* unit_tint = nullptr;
};

/// Up to two draw commands of one figure (shadow + body, hull + flag) and the tile row after
/// whose flat pass they are drawn.
struct FigureSprites {
    std::int32_t row = 0;
    std::array<DrawCommand, 2> parts{};
    std::uint8_t count = 0;
};

/// Shadow and body of a land figure (exposed for tests). The original keeps every figure at
/// Y = y + Posoffs[1] * w/64 (0x00451CC5), which is also its depth. Body: Gfx + animation
/// (0x0044F8F0), centred, bottom at Y + Posoffs[0] * w/64; shadow: figure `Schatten` from
/// SCHATTEN.BSH, centred on Y - Posoffs[1] * w/64 = y (cases 0x0C-0x0E of 0x0044FA90).
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

/// Sprites and row of a building figure at time `time_ms` (exposed for tests). False if it is not
/// shown: no graphics, no banner flag, smoke of an idle building, status OK, not the local player.
[[nodiscard]] bool building_figure_sprites(const BuildingFigure& figure, const sim::World& world,
                                           const assets::BuildingTable& buildings, const WorldView& view,
                                           const WorldLayers& layers, std::uint64_t time_ms, FigureSprites& out);

/// Same, but fills `out` (cleared first) so a per-frame caller can reuse its allocation.
void build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings, const AnimationClock& clock,
                           const WorldView& view, const Viewport& viewport, const SpriteExtent& extent,
                           std::vector<DrawCommand>& out);

}  // namespace o1602::render
