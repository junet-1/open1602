// SPDX-License-Identifier: GPL-3.0-or-later
//
// Figure definitions from figuren.cod (FIGUR records): ships, soldiers, carriers, flags, ...
// Only the fields needed for drawing are decoded so far.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "o1602/assets/cod_parser.hpp"
#include "o1602/core/result.hpp"

namespace o1602::assets {

class GameDirectory;

/// Sprite sheet of a figure (Blocknr). Order of the sheets loaded at startup (0x00494870).
enum class FigureSheet : std::uint8_t {
    Stadtfld = 0,
    Soldat = 1,
    Ship = 2,
    Traeger = 3,
    Maeher = 4,
    Tiere = 5,
    Effekte = 6,
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
    bool defined = false;
};

class FigureTable {
public:
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

private:
    std::vector<FigureType> m_types;
};

}  // namespace o1602::assets
