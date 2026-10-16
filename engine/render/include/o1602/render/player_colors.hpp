// SPDX-License-Identifier: GPL-3.0-or-later
//
// Display colours of the player colours (assets::PlayerColor). In the original the colours only
// appear as sprite variants (flags, soldiers); these RGB values are taken from the flag sprites
// and are used where the engine needs a plain colour (ownership overlay, minimap, UI).
#pragma once

#include <array>
#include <cstdint>

#include "o1602/assets/palette.hpp"
#include "o1602/assets/scenario.hpp"

namespace o1602::render {

[[nodiscard]] constexpr assets::Rgba player_rgb(assets::PlayerColor color) noexcept {
    constexpr std::array<assets::Rgba, assets::kPlayerColorCount> kColors = {{
        {200, 32, 24, 255},    // red
        {32, 56, 216, 255},    // blue
        {232, 192, 24, 255},   // yellow
        {232, 232, 232, 255},  // white
        {40, 40, 40, 255},     // pirates (black flag)
        {160, 104, 48, 255},   // natives (no flag)
        {48, 160, 96, 255},    // free trader (no flag)
    }};
    return kColors[static_cast<std::size_t>(color) % kColors.size()];
}

/// Opacity of the ownership overlay diamonds.
inline constexpr std::uint8_t kOverlayAlpha = 110;

}  // namespace o1602::render
