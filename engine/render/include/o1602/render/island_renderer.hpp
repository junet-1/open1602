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

/// Terrain/building sprite sheet of a zoom level, relative to the game directory.
[[nodiscard]] constexpr const char* terrain_sheet_path(Zoom zoom) noexcept {
    constexpr const char* kSheets[] = {"gfx/stadtfld.bsh", "mgfx/stadtfld.bsh", "sgfx/stadtfld.bsh"};
    return kSheets[static_cast<int>(zoom) % 3];
}

/// Ship sheet (figures with Blocknr 2: ships, flags, bow waves) of a zoom level.
[[nodiscard]] constexpr const char* ship_sheet_path(Zoom zoom) noexcept {
    constexpr const char* kSheets[] = {"gfx/ship.bsh", "mgfx/ship.bsh", "sgfx/ship.bsh"};
    return kSheets[static_cast<int>(zoom) % 3];
}

/// Palette of the terrain sheets, relative to the game directory.
inline constexpr const char* kTerrainPalettePath = "toolgfx/stadtfld.col";

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
    /// Not a sprite but a translucent tile diamond (tile width x tile height, given by `tint_size`)
    /// in the colour `tint` (alpha = opacity), e.g. for the ownership view. (x, y) is its
    /// left / bottom corner box.
    static constexpr std::uint32_t kOverlayDiamond = 0xFFFFFFFF;

    /// How a sprite is drawn.
    enum class Mode : std::uint8_t {
        Normal,
        /// Only the sprite's shape, filled with `tint` (alpha = opacity): figure shadows. The
        /// original dithers them (every second pixel black, 0x00401D10); a translucent fill
        /// looks the same on today's displays.
        Shadow,
    };

    std::uint32_t sprite = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    assets::Rgba tint{0, 0, 0, 0};  ///< colour of an overlay diamond or shadow
    std::uint8_t tint_size = 0;     ///< tile width of an overlay diamond (64 / 32 / 16)
    Mode mode = Mode::Normal;
};

/// Sprite index for a map cell with an explicit global animation frame of its building type
/// (base sprite + animation frame + rotation step). `building` may be null (sprite unchanged).
[[nodiscard]] std::uint32_t animated_sprite(const sim::MapCell& cell, const assets::BuildingType* building,
                                            std::uint32_t global_frame, std::uint8_t view_rotation);

/// Sprite index for a map cell, global frame = AnimFrame + options.anim_step.
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
