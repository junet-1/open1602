// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/ui/build_menu.hpp"

#include <algorithm>

#include "o1602/ui/panel.hpp"

namespace o1602::ui {

std::size_t BuildCategory::size() const noexcept {
    std::size_t n = 0;
    for (const auto& row : rows) {
        n += row.size();
    }
    return n;
}

std::optional<std::uint32_t> BuildCategory::at(std::size_t index) const noexcept {
    for (const auto& row : rows) {
        if (index < row.size()) {
            return row[index];
        }
        index -= row.size();
    }
    return std::nullopt;
}

std::optional<std::size_t> BuildCategory::index_of(std::uint32_t id) const noexcept {
    std::size_t i = 0;
    for (const auto& row : rows) {
        for (const std::uint32_t b : row) {
            if (b == id) {
                return i;
            }
            ++i;
        }
    }
    return std::nullopt;
}

const BuildCategory* BuildMenuDefinition::category(std::int32_t button) const noexcept {
    for (const BuildCategory& c : categories) {
        if (c.button == button) {
            return &c;
        }
    }
    return nullptr;
}

const BuildCategory* BuildMenuDefinition::category_of(std::uint32_t id) const noexcept {
    for (const BuildCategory& c : categories) {
        if (c.index_of(id)) {
            return &c;
        }
    }
    return nullptr;
}

const BuildMenuDefinition& original_build_menu() {
    static const BuildMenuDefinition menu = [] {
        BuildMenuDefinition m;
        m.categories = {
            {31001, false, "MILITAR", 5, {{22521, 21404, 21405}, {22501, 20304, 20305, 20303}, {20311, 20313, 20315}}},
            {31002, false, "STRASSE", 15, {{20411, 20401, 21601, 21603}, {20421, 20422, 20423, 20424}}},
            {31003, false, "BERGWERK", 23, {{22407, 22401, 22403, 22405}}},
            {31004, true, "ABRISS", 0, {}},
            {31005,
             false,
             "HANDWERK",
             27,
             {{20507, 20511, 20501, 20503},
              {20525, 20519, 20505, 20531},
              {20513, 20509, 20521, 20515},
              {20533, 20517, 20529, 20527}}},
            {31006,
             false,
             "FARM",
             44,
             {{22003, 22001, 21303},
              {21503, 21301, 21515, 21513},
              {21507, 21335, 21501, 21341},
              {21517, 21343, 21511, 21339},
              {21505, 21331, 21509, 21337}}},
            {31007, false, "HAFEN", 63, {{22101, 22103, 22105, 22107}, {21075, 21073, 22115, 22111}}},
            {31008,
             false,
             "DIVERS",
             71,
             {{20605, 20809, 20803, 20805},
              {20801, 20807, 20817, 20821},
              {20819, 20813, 20815, 20811},
              {20831, 20841, 20851, 20853}}},
        };
        return m;
    }();
    return menu;
}

namespace {

assets::Gadget piece(std::int32_t id, assets::GadgetKind kind, std::int32_t gfx, std::int32_t x, std::int32_t y,
                     std::int32_t w, std::int32_t h) {
    assets::Gadget g;
    g.id = id;
    g.kind = kind;
    g.gfx = gfx;
    g.pos = {x, y};
    g.size = {w, h};
    g.no_select = true;
    return g;
}

}  // namespace

assets::GadgetLayout build_popup(const BuildCategory& category, const std::vector<bool>& available,
                                 const assets::Gadget& button, std::int32_t gfx_bauhaus,
                                 std::int32_t locked_icon_offset) {
    // Rows with something to pick; none at all: the last row, all "?" (0x00432D82).
    struct Row {
        std::size_t first = 0;  ///< position of its first building in the list
        std::size_t count = 0;
    };
    std::vector<Row> shown;
    std::size_t first = 0;
    for (const auto& row : category.rows) {
        bool any = false;
        for (std::size_t k = 0; k < row.size(); ++k) {
            any = any || (first + k < available.size() && available[first + k]);
        }
        if (any) {
            shown.push_back(Row{first, row.size()});
        }
        first += row.size();
    }
    if (shown.empty() && !category.rows.empty()) {
        shown.push_back(Row{first - category.rows.back().size(), category.rows.back().size()});
    }

    using assets::GadgetKind;
    assets::GadgetLayout layout;
    auto& g = layout.gadgets;
    // Behind everything, the whole panel: a press there closes the pop-up.
    assets::Gadget backdrop = piece(kPopupBackdrop, GadgetKind::Area, 0, 0, 0, kDesignWidth, kDesignHeight);
    backdrop.no_select = false;
    backdrop.menu = true;
    g.push_back(backdrop);

    constexpr std::int32_t kX = 8;
    constexpr std::int32_t kWidth = 241;
    constexpr std::int32_t kShadow = 4;
    const std::int32_t bottom = button.pos[1];
    const auto rows = static_cast<std::int32_t>(shown.size());
    const std::int32_t top = bottom - kPopupRowStep * rows;
    const auto row_y = [&](std::size_t k) { return bottom - kPopupRowStep * (static_cast<std::int32_t>(k) + 1); };
    // The frame: a strip above, the rows, a strip below; first their shadows.
    for (const std::int32_t d : {kShadow, 0}) {
        const GadgetKind kind = d != 0 ? GadgetKind::GraphicRaster : GadgetKind::Graphic;
        g.push_back(piece(d != 0 ? 32952 : 32953, kind, gfx_bauhaus + 0, kX + d, top - 5 + d, kWidth, 5));
        for (std::size_t k = 0; k < shown.size(); ++k) {
            const auto r = static_cast<std::int32_t>(k);
            g.push_back(piece((d != 0 ? 32921 : 32901) + r, kind, gfx_bauhaus + 1, kX + d, row_y(k) + d, kWidth, 60));
        }
        g.push_back(piece(d != 0 ? 32954 : 32955, kind, gfx_bauhaus + 2, kX + d, bottom + 1 + d, kWidth, 4));
    }
    // Icons, then the frames to click over them.
    std::vector<assets::Gadget> slots;
    for (std::size_t k = 0; k < shown.size(); ++k) {
        for (std::size_t j = 0; j < shown[k].count; ++j) {
            const std::size_t i = shown[k].first + j;
            const bool ok = i < available.size() && available[i];
            const auto ii = static_cast<std::int32_t>(i);
            const std::int32_t x = kX + 8 + kPopupIconStep * static_cast<std::int32_t>(j);
            const std::int32_t y = row_y(k) + 5;
            const std::int32_t icon = gfx_bauhaus + (ok ? category.icon_offset + ii : locked_icon_offset);
            g.push_back(piece(kPopupIcon + ii, GadgetKind::Graphic, icon, x, y, 53, 50));
            assets::Gadget slot = piece(kPopupSlot + ii, GadgetKind::Graphic, gfx_bauhaus + 3, x, y, 53, 50);
            slot.press_offset = 1;
            slot.select_on_move = true;
            slot.no_select = !ok;
            slots.push_back(slot);
        }
    }
    const auto base = static_cast<std::int32_t>(g.size());
    for (auto& slot : slots) {
        slot.radio = true;
        slot.base = base;
        g.push_back(slot);
    }
    return layout;
}

}  // namespace o1602::ui
