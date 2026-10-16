// SPDX-License-Identifier: GPL-3.0-or-later
//
// Isometric island rendering following the original renderer (1602.exe, row routine 0x00489C90).
// Specification: docs/re/rendering.md.
//
// Rendering is split into two steps so the same logic can drive the software rasterizer used by
// tools and tests and, later, the GPU renderer:
//   1. build_draw_list(): which sprite goes where, in painter's order
//   2. rasterize():       blit the sprites into an RGBA image
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/assets/sprite_sheet.hpp"
#include "o1602/sim/island_map.hpp"

namespace o1602::render {

/// The three graphic sets of the original: GFX (tile 64 px wide), MGFX (32), SGFX (16).
enum class Zoom : std::uint8_t { Large = 0, Medium = 1, Small = 2 };

[[nodiscard]] constexpr std::int32_t tile_width(Zoom zoom) noexcept {
    return 64 >> static_cast<int>(zoom);
}
[[nodiscard]] constexpr std::int32_t tile_height(Zoom zoom) noexcept {
    return tile_width(zoom) / 2;
}

struct RenderOptions {
    Zoom zoom = Zoom::Large;
    /// Global animation step added to every animated tile (the original advances a frame counter
    /// per building type every AnimTime; for still images a single global step is sufficient).
    std::uint32_t anim_step = 0;
    /// View rotation 0..3. All four are visually consistent; not yet compared with the running game.
    std::uint8_t view_rotation = 0;
};

/// One sprite to draw. (x, y) is the left/bottom anchor of the sprite, as in the original blit
/// routine (0x0048A140), relative to the island's top tile corner.
struct DrawCommand {
    std::uint32_t sprite = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
};

/// Sprite index for a map cell (base sprite + animation frame + rotation step).
[[nodiscard]] std::uint32_t sprite_for_cell(const sim::MapCell& cell, const assets::BuildingType* building,
                                            const RenderOptions& options);

/// Painter's order: screen rows of tiles from back to front; per row first all flat tiles,
/// then all tiles with Highflg (the original draws figures between the two passes).
[[nodiscard]] std::vector<DrawCommand> build_draw_list(const sim::IslandMap& map,
                                                       const assets::BuildingTable& buildings,
                                                       const RenderOptions& options);

struct RgbaImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;  ///< RGBA8, row major
};

/// Draws the commands into a tightly fitting image. Sprites that do not exist in the sheet are
/// skipped and counted in `missing_sprites`.
[[nodiscard]] RgbaImage rasterize(const std::vector<DrawCommand>& commands, const assets::SpriteSheet& sheet,
                                  const assets::Palette& palette, assets::Rgba background = {0, 0, 0, 0},
                                  std::size_t* missing_sprites = nullptr);

}  // namespace o1602::render
