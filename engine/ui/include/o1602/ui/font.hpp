// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fonts of the original (TOOLGFX/*.ZEI, docs/formats/bsh.md#zei-schriften) and its text
// rules (docs/re/ui.md): glyph = character code - 32 (Windows-1252), advance = glyph width, the
// text's y is the bottom of the cell, two palette indices (1 body, 7 shadow) recoloured through one
// of ten colour tables. Pure logic: text is drawn into an indexed canvas.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/assets/sprite_sheet.hpp"
#include "o1602/core/result.hpp"

namespace o1602::ui {

/// Palette index for every palette index: what a glyph pixel becomes.
using Recolour = std::array<std::uint8_t, 256>;

/// The ten text colours of the original (0x00494870): body and shadow as RGB, matched to the
/// nearest palette colour.
struct TextColour {
    std::array<std::uint8_t, 3> body{};
    std::array<std::uint8_t, 3> shadow{};
};
inline constexpr std::size_t kTextColours = 10;
[[nodiscard]] const TextColour& text_colour(std::size_t index) noexcept;

/// Nearest palette colour by squared RGB distance, the first of equals (0x004031B0).
[[nodiscard]] std::uint8_t nearest_colour(const assets::Palette& palette, std::uint8_t r, std::uint8_t g,
                                          std::uint8_t b) noexcept;
/// The recolouring of text colour `index` (out of range: 0) for a palette.
[[nodiscard]] Recolour text_recolour(const assets::Palette& palette, std::size_t index);

class Font {
public:
    /// From a ZEI file of the game directory ("TOOLGFX/ZEI20V.ZEI").
    static core::Result<Font> load(const assets::GameDirectory& game, std::string_view file);
    static core::Result<Font> from_sheet(const assets::SpriteSheet& sheet);

    /// Height of the cell (the same for every glyph).
    [[nodiscard]] std::uint32_t height() const noexcept { return m_height; }
    /// Whether the font has a glyph for the byte (the large fonts have only codes 32..64).
    [[nodiscard]] bool has(unsigned char c) const noexcept { return c >= 0x20 && c - 0x20U < m_glyphs.size(); }
    /// Width of the text up to its first control character; bytes without a glyph count 0.
    [[nodiscard]] std::uint32_t width(std::string_view text) const noexcept;
    /// Draws the text with its cell's bottom at `bottom`, starting at x (left aligned). Pixels outside
    /// the canvas are left out; returns the width.
    std::uint32_t draw(assets::IndexedImage& canvas, std::string_view text, std::int32_t x, std::int32_t bottom,
                       const Recolour& colour) const;

private:
    struct Glyph {
        assets::IndexedImage image;  ///< with the pixels that overhang into the next glyph
        std::uint32_t advance = 0;
    };
    std::vector<Glyph> m_glyphs;
    std::uint32_t m_height = 0;
};

/// Draws text aligned left, centred on x or with its right edge at x (0x004014F0 / 0x00401530 /
/// 0x004015A0).
enum class Align : std::uint8_t { Left, Centre, Right };
std::uint32_t draw_text(assets::IndexedImage& canvas, const Font& font, std::string_view text, std::int32_t x,
                        std::int32_t bottom, const Recolour& colour, Align align);

/// Breaks text into lines of at most `max_width` pixels (Text_WrapLine 0x00401610): words are
/// separated by single spaces, a control character ends a line (several in a row count as one),
/// "--" in a word is a hyphen where it may break (removed if the word fits), a word wider than a line
/// is cut by width. From 10000 on nothing is wrapped. The original drops a line's first character
/// when it is a byte from 0x80 on after a hard break (U-017); Open1602 keeps it.
[[nodiscard]] std::vector<std::string> wrap_text(const Font& font, std::string_view text, std::uint32_t max_width);

}  // namespace o1602::ui
