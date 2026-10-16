// SPDX-License-Identifier: GPL-3.0-or-later
//
// Banners: how a player is recognised on screen - display colour (ownership view, minimap, UI)
// and the waving flag on ships and buildings. Players refer to a banner by sim::BannerId; the
// table is open-ended so mods can add colours and flags (ADR 0007, section 4).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/sim/player.hpp"

namespace o1602::render {

struct Banner {
    std::string id;  ///< stable name, e.g. "anno1602:red"
    assets::Rgba color;
    /// First of `flag_frames` flag images in the ship sheet (ships and buildings share them, as in
    /// the original); nothing for banners without a flag (natives, free trader). Banners from mods
    /// will name their own sheet here.
    std::optional<std::uint32_t> flag_gfx;
    std::uint32_t flag_frames = 8;
};

/// Opacity of the ownership overlay diamonds.
inline constexpr std::uint8_t kOverlayAlpha = 110;

class BannerTable {
public:
    /// The seven colours of the original. With `figures`, the flags are taken from the flag
    /// figures (table 0x00498F2C: FAHNE1-4, FAHNEPIRAT -> SHIP.BSH 192 + 8 * colour).
    [[nodiscard]] static BannerTable original(const assets::FigureTable* figures = nullptr);

    [[nodiscard]] const Banner* get(sim::BannerId id) const noexcept {
        return id < m_banners.size() ? &m_banners[id] : nullptr;
    }
    [[nodiscard]] std::size_t size() const noexcept { return m_banners.size(); }
    sim::BannerId add(Banner banner);

private:
    std::vector<Banner> m_banners;
};

}  // namespace o1602::render
