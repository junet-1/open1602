// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/banner.hpp"

namespace o1602::render {

// Fitted to the original flag images (red, blue, yellow) for the tint of flag_tint.hpp; white is
// the template itself.
const std::array<assets::Rgba, 12> BannerTable::kDefaultColors = {{
    {216, 51, 8, 255},     // red (original)
    {0, 0, 241, 255},      // blue (original)
    {255, 219, 57, 255},   // yellow (original)
    {255, 255, 255, 255},  // white (original)
    {30, 170, 40, 255},    // green
    {255, 128, 0, 255},    // orange
    {130, 40, 190, 255},   // purple
    {0, 190, 190, 255},    // teal
    {255, 110, 180, 255},  // pink
    {90, 170, 255, 255},   // light blue
    {120, 70, 30, 255},    // brown
    {170, 230, 60, 255},   // lime (no black: the pirates fly a black flag)
}};

BannerTable BannerTable::original(const assets::FigureTable* figures, const BannerOptions& options) {
    BannerTable table;
    const char* kPlayers[] = {"anno1602:red", "anno1602:blue", "anno1602:yellow", "anno1602:white"};
    for (std::uint8_t c = 0; c < 4; ++c) {
        Banner b{kPlayers[c], kDefaultColors[c], FlagStyle::Tinted, 0, 8, c};
        const assets::FigureType* flag = figures != nullptr ? figures->flag_for_color(c) : nullptr;
        if (options.original_player_flags && flag != nullptr && flag->sheet == assets::FigureSheet::Ship) {
            b.flag = FlagStyle::Sprites;
            b.flag_gfx = flag->gfx;
        }
        table.add(std::move(b));
    }
    Banner pirates{"anno1602:pirates", {40, 40, 40, 255}, FlagStyle::None, 0, 8, std::nullopt};
    if (const assets::FigureType* flag = figures != nullptr ? figures->flag_for_color(4) : nullptr;
        flag != nullptr && flag->sheet == assets::FigureSheet::Ship) {
        pirates.flag = FlagStyle::Sprites;
        pirates.flag_gfx = flag->gfx;
    }
    table.add(std::move(pirates));
    table.add(Banner{"anno1602:natives", {160, 104, 48, 255}, FlagStyle::None, 0, 8, std::nullopt});
    table.add(Banner{"anno1602:trader", {48, 160, 96, 255}, FlagStyle::None, 0, 8, std::nullopt});
    // Generic flag: the white flag FAHNE4.
    if (const assets::FigureType* white = figures != nullptr ? figures->flag_for_color(3) : nullptr;
        white != nullptr && white->sheet == assets::FigureSheet::Ship) {
        table.set_flag_template(FlagTemplate{white->gfx, 8, true});
    }
    return table;
}

static_assert(BannerTable::kDefaultColors.size() == sim::kMaxPlayers, "a colour of its own for every player");

void BannerTable::add_more_players() {
    if (m_banners.size() != sim::kFirstExtraBanner) {
        return;
    }
    struct More {
        const char* id;
        std::size_t colour;  ///< in kDefaultColors
    };
    static constexpr std::array<More, sim::kMaxPlayers - sim::kOriginalPlayers> kMore = {{
        {"open1602:orange", 5},
        {"open1602:purple", 6},
        {"open1602:teal", 7},
        {"open1602:pink", 8},
        {"open1602:light_blue", 9},
        {"open1602:green", 4},
        {"open1602:lime", 11},
        {"open1602:brown", 10},
    }};
    for (const More& m : kMore) {
        add_color(m.id, kDefaultColors[m.colour]);
    }
}

sim::BannerId BannerTable::add(Banner banner) {
    m_banners.push_back(std::move(banner));
    return static_cast<sim::BannerId>(m_banners.size() - 1);
}

sim::BannerId BannerTable::add_color(std::string id, assets::Rgba color) {
    // FNV-1a of the id: a stable "random" horse per banner (not std::hash, which differs between
    // standard libraries and would break multiplayer games across platforms).
    std::uint32_t hash = 2166136261U;
    for (const char c : id) {
        hash = (hash ^ static_cast<std::uint8_t>(c)) * 16777619U;
    }
    Banner b{std::move(id), color, FlagStyle::Tinted, 0, 8, std::nullopt};
    b.unit_mount = static_cast<std::uint8_t>(hash % 4U);
    return add(std::move(b));
}

}  // namespace o1602::render
