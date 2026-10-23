// SPDX-License-Identifier: GPL-3.0-or-later
//
// The window "Warenbilanz" over the map, Open1602's own (docs/architecture/0012-goods-statistics.md):
// for every good of the player's cities, all of them or one island's, what is in store, what came
// in and went out per minute over a chosen time (1, 5, 15 or 60 game minutes) and how long the
// stock lasts at that balance. A click on a good shows where it came from and went to, and its
// last hour (or twelve hours) as a chart. Only the local player's cities: allies and trading
// partners never see each other's figures. The window only reads the world; the game goes on
// behind it.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/sim/world.hpp"
#include "o1602/ui/form.hpp"

namespace o1602::app {

/// The colours of the window: the form's and the palette colours of the charts.
struct GoodsStyle {
    ui::FormStyle form;
    std::uint8_t in = 0;     ///< green: goods coming in
    std::uint8_t out = 0;    ///< red: goods going out
    std::uint8_t stock = 0;  ///< the stock's line and the small charts
    std::uint8_t chart = 0;  ///< the ground of a chart
};

class GoodsWindow {
public:
    enum class Result : std::uint8_t { None, Close };
    /// The window's size in design units (it is centred over the map).
    static constexpr std::int32_t kWidth = 760;
    static constexpr std::int32_t kHeight = 580;
    /// The time the figures are averaged over, game minutes; the default is the second.
    static constexpr std::array<std::uint32_t, 4> kWindows = {1, 5, 15, 60};
    /// Rows of the table shown at once.
    static constexpr std::size_t kRows = 16;

    /// `ware_names`: Text.cod [WARE] by ware number; `ware_icon`: the sprite of ware 0 in TOOLS
    /// (GFXWARE).
    GoodsWindow(GoodsStyle style, std::vector<std::string> ware_names, std::int32_t ware_icon);

    void set_scale(double scale) { m_form.set_scale(scale); }
    [[nodiscard]] double scale() const noexcept { return m_form.scale(); }

    /// Opens on the table (the island and time chosen before stay).
    void open() noexcept {
        m_detail.reset();
        m_scroll = 0;
    }
    /// Escape: from a good's page back to the table; false on the table (the window closes).
    bool back() noexcept {
        if (!m_detail) {
            return false;
        }
        m_detail.reset();
        return true;
    }
    /// The mouse wheel over the window: notches, up positive.
    void wheel(std::int32_t notches) noexcept;

    /// Everything to draw for `player`'s cities, relative to the window's top left. It lays the
    /// window out; mouse() answers what was drawn last.
    [[nodiscard]] std::vector<ui::DrawItem> draw(const sim::World& world, const assets::BuildingTable& buildings,
                                                 sim::PlayerId player, const std::array<const ui::Font*, 3>& fonts);
    /// The mouse in pixels relative to the window's top left.
    Result mouse(std::int32_t x, std::int32_t y, bool down, ui::MouseButton button = ui::MouseButton::Left);

    // The view, for the tests.
    [[nodiscard]] std::uint32_t minutes() const noexcept { return kWindows[m_window]; }
    [[nodiscard]] std::optional<std::uint16_t> island() const noexcept { return m_island; }
    [[nodiscard]] std::optional<std::uint8_t> detail() const noexcept { return m_detail; }
    [[nodiscard]] const ui::Form& form() const noexcept { return m_form; }

    /// Tenths of a ton per minute as text, e.g. "12.5" ("12,5" in German): `units` 1/32 t over
    /// `minutes`. With `sign` a "+" before what is above zero.
    [[nodiscard]] static std::string rate_text(std::int64_t units, std::uint32_t minutes, bool sign = false);
    /// How long a stock lasts: "12 min", "3 h"; empty if it does not fall.
    [[nodiscard]] static std::string lasts_text(std::optional<std::uint32_t> minutes);

private:
    void layout_table(const sim::World& world, sim::PlayerId player, const std::array<const ui::Font*, 3>& fonts,
                      std::vector<ui::DrawItem>& extra);
    void layout_detail(const sim::World& world, const assets::BuildingTable& buildings, sim::PlayerId player,
                       std::uint8_t ware, const std::array<const ui::Font*, 3>& fonts,
                       std::vector<ui::DrawItem>& extra);
    void layout_controls(const sim::World& world, sim::PlayerId player);
    [[nodiscard]] std::string ware_name(std::uint8_t ware) const;

    GoodsStyle m_style;
    std::vector<std::string> m_ware_names;
    std::int32_t m_ware_icon = 0;
    ui::Form m_form;

    std::optional<std::uint16_t> m_island;  ///< nothing: all of the player's islands
    std::size_t m_window = 1;               ///< index into kWindows
    bool m_all_goods = false;               ///< also goods without stock and movement
    std::size_t m_scroll = 0;               ///< first row of the table shown
    std::size_t m_row_count = 0;            ///< rows of the table as last laid out
    std::optional<std::uint8_t> m_detail;   ///< the good whose page is shown
    bool m_long_chart = false;              ///< the page's chart over twelve hours instead of one
    std::vector<std::uint16_t> m_islands;   ///< the player's islands as last laid out
};

}  // namespace o1602::app
