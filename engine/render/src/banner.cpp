// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/banner.hpp"

#include <array>

namespace o1602::render {

BannerTable BannerTable::original(const assets::FigureTable* figures) {
    struct Entry {
        const char* id;
        assets::Rgba color;  // taken from the flag sprites
    };
    constexpr std::array<Entry, assets::kPlayerColorCount> kOriginal = {{
        {"anno1602:red", {200, 32, 24, 255}},
        {"anno1602:blue", {32, 56, 216, 255}},
        {"anno1602:yellow", {232, 192, 24, 255}},
        {"anno1602:white", {232, 232, 232, 255}},
        {"anno1602:pirates", {40, 40, 40, 255}},
        {"anno1602:natives", {160, 104, 48, 255}},
        {"anno1602:trader", {48, 160, 96, 255}},
    }};
    BannerTable table;
    for (std::size_t c = 0; c < kOriginal.size(); ++c) {
        Banner b{kOriginal[c].id, kOriginal[c].color, std::nullopt, 8};
        if (figures != nullptr) {
            const assets::FigureType* flag = figures->flag_for_color(static_cast<std::uint8_t>(c));
            if (flag != nullptr && flag->sheet == assets::FigureSheet::Ship) {
                b.flag_gfx = flag->gfx;
            }
        }
        table.add(std::move(b));
    }
    return table;
}

sim::BannerId BannerTable::add(Banner banner) {
    m_banners.push_back(std::move(banner));
    return static_cast<sim::BannerId>(m_banners.size() - 1);
}

}  // namespace o1602::render
