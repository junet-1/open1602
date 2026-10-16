// SPDX-License-Identifier: GPL-3.0-or-later
//
// Units in any player colour. The original draws soldiers, riders, musketeers and gunners four
// times, once per player colour (red, blue, yellow, white). Banners with one of these colours use
// the drawn images; every other banner gets a template made from the four variants and tinted
// like the generic flag (flag_tint.hpp). The variants are drawn separately, not recoloured, so a
// tinted unit is similar to the originals but not identical (docs/re/figures.md).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "o1602/assets/palette.hpp"
#include "o1602/render/sprite_source.hpp"

namespace o1602::render {

/// The images of one unit family in its sheet: variant v of image k is `first + v * stride + k`.
struct UnitImageGroup {
    std::uint32_t first = 0;    ///< image 0 of variant 0
    std::uint32_t stride = 0;   ///< images per variant
    std::uint8_t variants = 0;  ///< 4 in the original
    std::uint8_t base = 0;      ///< variant the template is made from (3 = white in the original)
    std::uint32_t offset = 0;   ///< position of the family in a tinted block
};

/// Where the tinted units of each banner are in an atlas.
struct UnitTint {
    std::vector<UnitImageGroup> groups;
    std::uint32_t block_size = 0;  ///< images per banner (sum of the groups' strides)
    /// Per banner (BannerTable index): first atlas index of its tinted block, nothing for banners
    /// that use drawn variants or have no tint.
    std::vector<std::optional<std::uint32_t>> banner_base;

    /// Atlas index of image `image` (index in the sheet, of any variant) in the tinted block of
    /// `banner`; nothing if the banner has no block or the image belongs to no group.
    [[nodiscard]] std::optional<std::uint32_t> sprite(std::size_t banner, std::uint32_t image) const noexcept;
};

/// An RGBA8 image.
struct RgbaView {
    const std::uint8_t* pixels = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

/// Builds the tint template of one unit image from its colour variants (`out`: size of
/// variants[base]). Among the variants of the same size, a pixel *differs* if at least three
/// different colours occur there and the base variant's colour only once. A variant whose player
/// colour has a hue (`colors[v]`, not white) *shows its colour* at a pixel if the pixel's hue is
/// within 60 degrees of it and closer to it than to the other variants' colours. A differing pixel
/// where every such variant shows its colour is a seed; cloth is every seed plus every differing
/// pixel connected to one where at least one variant shows its colour. So sashes, trousers and
/// plumes become cloth; boots, weapons and skin stay, and so do the horses, which the original
/// also draws in different colours per variant, but not in the player colour (a grey horse under
/// the blue rider). Cloth gets the grey 0.48 * luminance + 5 as the flags;
/// other pure grey pixels are moved by one step in blue so that the tint leaves them alone
/// (template convention, docs/modding/flags.md).
void unit_template(std::span<const RgbaView> variants, std::span<const assets::Rgba> colors, std::size_t base,
                   std::vector<std::uint8_t>& out);

/// The unit images of all groups, tinted once per colour: colours.size() blocks of block_size
/// images, in group order.
class TintedUnitSource final : public SpriteSource {
public:
    /// `variant_colors`: player colour of each variant (the original: red, blue, yellow, white).
    TintedUnitSource(const SpriteSource& sheet, std::vector<UnitImageGroup> groups,
                     std::vector<assets::Rgba> variant_colors, std::vector<assets::Rgba> colors);
    [[nodiscard]] std::uint32_t count() const override;
    void size(std::uint32_t i, std::uint32_t& width, std::uint32_t& height) const override;
    bool pixels(std::uint32_t i, std::vector<std::uint8_t>& rgba) const override;

private:
    /// Group and image k of image i of a block.
    [[nodiscard]] const UnitImageGroup* locate(std::uint32_t i, std::uint32_t& k) const noexcept;

    const SpriteSource& m_sheet;
    std::vector<UnitImageGroup> m_groups;
    std::vector<assets::Rgba> m_variant_colors;
    std::vector<assets::Rgba> m_colors;
    std::uint32_t m_block = 0;
};

}  // namespace o1602::render
