// SPDX-License-Identifier: GPL-3.0-or-later
//
// Figure definitions from figuren.cod (FIGUR records): ships, soldiers, carriers, flags, ...
// Only the fields needed for drawing are decoded so far.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "o1602/assets/cod_parser.hpp"
#include "o1602/core/result.hpp"

namespace o1602::assets {

class GameDirectory;

/// Sprite sheets of a zoom level in the order the original loads them (file names at 0x0049C4C0 ..
/// 0x0049C6F4). Values 0..6 are the figure Blocknr; shadows always come from SCHATTEN.BSH.
enum class FigureSheet : std::uint8_t {
    Stadtfld = 0,
    Soldat = 1,
    Ship = 2,
    Traeger = 3,
    Maeher = 4,
    Tiere = 5,
    Effekte = 6,
    Numbers = 7,
    Schatten = 8,
    Fische = 9,
    Gaukler = 10,
};
inline constexpr std::size_t kFigureSheetCount = 11;

/// File name of a sheet (lower case; GameDirectory resolves case-insensitively).
[[nodiscard]] constexpr const char* figure_sheet_file(FigureSheet sheet) noexcept {
    constexpr const char* kFiles[kFigureSheetCount] = {"stadtfld.bsh", "soldat.bsh", "ship.bsh",    "traeger.bsh",
                                                       "maeher.bsh",   "tiere.bsh",  "effekte.bsh", "numbers.bsh",
                                                       "schatten.bsh", "fische.bsh", "gaukler.bsh"};
    return kFiles[static_cast<std::size_t>(sheet) % kFigureSheetCount];
}

/// Kind of an animation (ANIM Kind): what happens after it ran AnimRept times (0x0045D0B0).
enum class AnimKind : std::uint8_t {
    None = 0,
    Endless = 1,  ///< start over
    Random = 2,   ///< continue with a random entry of `next`
    JumpTo = 3,   ///< continue with next[0]
};

/// One ANIM object of a figure (0x1E bytes per animation in the original, figure record + 0x60).
struct FigureAnim {
    AnimKind kind = AnimKind::None;
    std::uint32_t offset = 0;        ///< AnimOffs: first sprite relative to the figure's Gfx
    std::uint32_t add = 1;           ///< AnimAdd: sprite step per frame
    std::uint32_t count = 1;         ///< AnimAnz: frames
    std::uint32_t repeat = 1;        ///< AnimRept: runs before `kind` applies
    std::uint32_t speed_ms = 100;    ///< AnimSpeed: duration of one frame
    std::uint32_t rotate = 0;        ///< sprite step per direction (Rotate, default: the figure's)
    std::vector<std::uint8_t> next;  ///< AnimNr
    bool defined = false;
};

struct FigureType {
    std::uint32_t number = 0;  ///< Nummer (index into the original table at 0x0061FCC0)
    std::int32_t kind = 0;
    std::uint32_t gfx = 0;  ///< first sprite in its sheet
    FigureSheet sheet = FigureSheet::Stadtfld;
    std::uint32_t rotate = 0;               ///< sprite distance per direction step
    std::array<std::int32_t, 2> posoffs{};  ///< [0]: vertical offset in pixels at full zoom (record +0x0C)
    /// Fahnoffs: flag position on ships/buildings. [1] distance along the ship axis and [2] height,
    /// both in tile units (height in half tile widths). Presentation only, so floating point.
    std::array<double, 3> fahnoffs{};
    std::uint32_t shadow = 0;       ///< Schatten: figure number of the shadow, 0 = none
    std::vector<FigureAnim> anims;  ///< indexed by ANIM Nummer
    bool defined = false;

    [[nodiscard]] const FigureAnim* anim(std::size_t index) const noexcept {
        return index < anims.size() && anims[index].defined ? &anims[index] : nullptr;
    }
};

/// Sprite of a figure within its sheet, as 0x0044F8F0 computes it:
/// Gfx + AnimOffs + AnimAdd * frame + ((direction + 2 * view rotation) & 7) * Rotate. Without the
/// animation (or for an undefined one) only Gfx + direction * the figure's Rotate.
[[nodiscard]] std::uint32_t figure_sprite(const FigureType& type, std::size_t anim, std::uint32_t frame,
                                          std::uint8_t direction, std::uint8_t view_rotation) noexcept;

/// A unit drawn once per player colour: the original keeps four consecutive figures, one each for
/// red, blue, yellow and white (SOLDAT1..4, KAVALERIE1..4, MUSKETIER1..4, KANONIER1..4).
struct UnitVariant {
    std::uint32_t family = 0;  ///< figure of variant 0 (red)
    std::uint8_t index = 0;    ///< variant of this figure, = original player colour 0..3
    std::uint8_t count = 0;    ///< variants of the family
};

class FigureTable {
public:
    /// First figures of the unit families of the original (figure numbers 1..16, 4 colours each).
    static constexpr std::array<std::uint32_t, 4> kUnitFamilies = {1, 5, 9, 13};
    static constexpr std::uint8_t kUnitVariants = 4;

    /// Ship flags by player colour (table at 0x00498F2C): FAHNE1..4, FAHNEPIRAT.
    static constexpr std::array<std::uint32_t, 5> kFlagFigures = {106, 107, 108, 109, 110};

    static core::Result<FigureTable> from_cod(const cod::Document& figuren);
    static core::Result<FigureTable> load(const GameDirectory& game);

    [[nodiscard]] const FigureType* get(std::uint32_t number) const noexcept {
        return number < m_types.size() && m_types[number].defined ? &m_types[number] : nullptr;
    }
    /// Flag figure of a player colour, or nullptr for colours without flag (natives, trader).
    [[nodiscard]] const FigureType* flag_for_color(std::uint8_t color) const noexcept {
        return color < kFlagFigures.size() ? get(kFlagFigures[color]) : nullptr;
    }
    /// Colour variant of a unit figure, nothing for figures drawn the same for everyone (e.g. the
    /// natives' spearmen SPEER1).
    [[nodiscard]] static std::optional<UnitVariant> unit_variant(std::uint32_t figure) noexcept {
        for (const std::uint32_t family : kUnitFamilies) {
            if (figure >= family && figure < family + kUnitVariants) {
                return UnitVariant{family, static_cast<std::uint8_t>(figure - family), kUnitVariants};
            }
        }
        return std::nullopt;
    }

private:
    std::vector<FigureType> m_types;
};

}  // namespace o1602::assets
