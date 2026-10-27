// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pictures of a content package's own: `graphics.toml` names PNG images, each a grid of sprites of
// one size, and gives them sprite numbers after the original's STADTFLD.BSH
// (docs/modding/graphics.md). A building a package added draws them (own_buildings.hpp, `gfx`).
// The numbers follow the packages' load order, so they are the same on every computer with the
// same packages; the islands' tiles keep them in savegames.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace o1602::assets {

/// The file of a package that names its images.
inline constexpr std::string_view kGraphicsFile = "graphics.toml";
/// The first sprite number of the packages' images: STADTFLD.BSH has 5964 sprites, so the
/// original's numbers stay free with room to spare.
inline constexpr std::uint32_t kFirstPackageSprite = 40000;
/// One past the last: a tile keeps its sprite in 16 bits, 0xFFFF is none.
inline constexpr std::uint32_t kEndPackageSprite = 65535;
/// The sprites of the largest zoom level are at most this wide and high; those of the next two
/// levels are halved from them or given in images of their own.
inline constexpr std::uint32_t kMaxPackageSpriteSide = 1024;

/// The zoom levels in the order of the original's folders: GFX, MGFX, SGFX.
inline constexpr std::size_t kPackageZoomLevels = 3;

/// An image of a package, in sprites of `width` x `height` pixels at the largest zoom level, row by
/// row from the top left.
struct PackageImage {
    std::string id;           ///< "<package>:<name>"
    std::uint32_t first = 0;  ///< sprite number of its first sprite
    std::uint32_t count = 0;  ///< its sprites
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    /// The PNG file per zoom level; empty for the smaller levels made from the larger one at half
    /// size. Its sprites there are half (a quarter) as wide and high, rounded up.
    std::array<std::filesystem::path, kPackageZoomLevels> files;

    friend bool operator==(const PackageImage&, const PackageImage&) = default;
};

/// Reads `graphics.toml` of `package` (in `folder`) and adds its images to `images` with the next
/// free sprite numbers. Reads the size of every PNG it names from the file's header. A table that
/// cannot be read is passed over with a warning; a file that is no data file adds nothing.
void apply_graphics_file(std::vector<PackageImage>& images, std::string_view package,
                         const std::filesystem::path& folder, std::string_view utf8,
                         std::vector<std::string>& warnings);

/// The image called `id` ("<package>:<name>"), nullptr if there is none.
[[nodiscard]] const PackageImage* find_package_image(std::span<const PackageImage> images,
                                                     std::string_view id) noexcept;

/// The size of a sprite of `image` at zoom level `zoom` (0 = largest).
[[nodiscard]] std::uint32_t package_sprite_width(const PackageImage& image, std::size_t zoom) noexcept;
[[nodiscard]] std::uint32_t package_sprite_height(const PackageImage& image, std::size_t zoom) noexcept;

}  // namespace o1602::assets
