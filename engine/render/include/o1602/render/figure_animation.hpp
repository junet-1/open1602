// SPDX-License-Identifier: GPL-3.0-or-later
//
// Frame stepping of figure animations (ANIM objects of figuren.cod), as the original advances
// them once per displayed frame (0x0045D0B0): after AnimAnz frames of AnimSpeed ms an animation
// has run once; after AnimRept runs ENDLESS starts over, JUMPTO continues with AnimNr[0] and
// RANDOM with a random entry of AnimNr. The original uses rand(); Open1602 keeps a small
// generator per figure so a picture at a given time is reproducible.
#pragma once

#include <cstdint>

#include "o1602/assets/figure_table.hpp"

namespace o1602::render {

struct AnimState {
    std::uint8_t anim = 0;
    std::uint8_t frame = 0;
    std::uint8_t runs = 0;  ///< completed runs of the current animation
    std::uint32_t elapsed_ms = 0;
    std::uint32_t random = 1;  ///< generator state (never 0)
};

/// Starting state: animation `anim`, a frame and remaining time derived from `seed` so that
/// neighbouring figures do not move in lockstep.
[[nodiscard]] AnimState start_animation(const assets::FigureType& type, std::uint8_t anim, std::uint32_t seed) noexcept;

/// Advances by `dt_ms`. Figures without the animation keep their state.
void advance_animation(AnimState& state, const assets::FigureType& type, std::uint32_t dt_ms) noexcept;

}  // namespace o1602::render
