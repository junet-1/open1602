// SPDX-License-Identifier: GPL-3.0-or-later
//
// Banners: how a player is recognised on screen - display colour (ownership view, minimap, UI)
// and the waving flag on ships and buildings. Players refer to a banner by sim::BannerId; the
// table is open-ended (ADR 0007, section 4).
//
// Player flags are one generic flag tinted in the banner colour (flag_tint.hpp), so any colour
// works without new graphics. Banners with their own motif (the pirates' skull) keep sprites.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/sim/player.hpp"

namespace o1602::render {

enum class FlagStyle : std::uint8_t {
    None,     ///< no flag (natives, free trader)
    Tinted,   ///< the generic flag in `color`
    Sprites,  ///< own images (`flag_gfx` in the ship sheet), e.g. the pirates' skull
};

struct Banner {
    std::string id;      ///< stable name, e.g. "anno1602:red"
    assets::Rgba color;  ///< display colour and flag tint
    FlagStyle flag = FlagStyle::Tinted;
    std::uint32_t flag_gfx = 0;  ///< FlagStyle::Sprites: first image in the ship sheet
    std::uint32_t flag_frames = 8;
    /// Units drawn per colour (soldiers, riders, ...) use the original images of this colour
    /// (0 red, 1 blue, 2 yellow, 3 white); without it they are tinted in `color` (unit_tint.hpp).
    std::optional<std::uint8_t> unit_variant;
    /// Without unit_variant: which of four horses (0 brown, 1 grey, 2 dark brown, 3 white,
    /// UnitTintColor::mount) the riders get. add_color picks it from the banner's id, so it stays the same in
    /// every session and on every computer of a multiplayer game.
    std::uint8_t unit_mount = 3;
};

/// Opacity of the ownership overlay diamonds.
inline constexpr std::uint8_t kOverlayAlpha = 110;

/// The generic flag: `frames` images in the ship sheet starting at `gfx`. The base game takes the
/// white flag (FAHNE4) and turns it into a tint template (flag_tint.hpp).
struct FlagTemplate {
    std::uint32_t gfx = 0;
    std::uint32_t frames = 8;
    bool from_original = true;  ///< convert an original coloured flag (false: already a template)
};

struct BannerOptions {
    /// Use the original red, blue, yellow and white flag images instead of the tinted generic
    /// flag (pixel-faithful to the original, including its 256-colour dithering).
    bool original_player_flags = false;
};

class BannerTable {
public:
    /// The seven banners of the original. Red, blue, yellow and white use the tinted generic flag
    /// in the colours measured from the original flags; pirates keep FAHNEPIRAT; natives and the
    /// free trader have none. `figures` supplies the flag images (table 0x00498F2C).
    [[nodiscard]] static BannerTable original(const assets::FigureTable* figures = nullptr,
                                              const BannerOptions& options = {});

    /// Colours offered for new banners: the four of the original and eight more that stay
    /// distinguishable on land, water and the minimap (the menu also allows any RGB colour).
    static const std::array<assets::Rgba, 12> kDefaultColors;

    [[nodiscard]] const Banner* get(sim::BannerId id) const noexcept {
        return id < m_banners.size() ? &m_banners[id] : nullptr;
    }
    [[nodiscard]] std::size_t size() const noexcept { return m_banners.size(); }
    [[nodiscard]] const std::vector<Banner>& banners() const noexcept { return m_banners; }
    sim::BannerId add(Banner banner);
    /// The banners of the fifth player on (sim::kFirstExtraBanner), after the original's seven:
    /// the eight colours after the original's four, those that stand out best on the minimap's
    /// land and are furthest from the natives' brown first.
    void add_more_players();
    /// A banner with the generic flag in `color`; its riders get a horse picked from `id`.
    sim::BannerId add_color(std::string id, assets::Rgba color);

    [[nodiscard]] const std::optional<FlagTemplate>& flag_template() const noexcept { return m_template; }
    void set_flag_template(FlagTemplate t) { m_template = t; }

private:
    std::vector<Banner> m_banners;
    std::optional<FlagTemplate> m_template;
};

}  // namespace o1602::render
