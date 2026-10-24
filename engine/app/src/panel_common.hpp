// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the windows of the selections share (selection_panels.cpp, ship_panel.cpp).
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "selection_panels.hpp"

#include "o1602/sim/game_data.hpp"

namespace o1602::app {

namespace panel_detail {

inline constexpr std::uint8_t kFirstTradeWare = sim::ware::kFirstTrade;
inline constexpr std::uint8_t kLastTradeWare = sim::ware::kLastTrade;
/// An empty hold or stock slot shows the ware "Leer" (GFXWARE + 0 = GFXWARELEER + 4: sand). The
/// four pictures from GFXWARELEER are frames (green, yellow, black, wide yellow): the yellow one
/// marks the chosen ware and is no picture of its own.
inline constexpr std::int32_t kEmptyHold = 4;

/// A help line (LEISTE section of Text.cod).
inline std::string help_line(const PanelContext& c, std::size_t line) {
    return c.text("LEISTE", line);
}

/// The city of the tile of a selection, nothing if there is none.
inline const sim::City* city_at(const Selection& s, const PanelContext& c) {
    if (s.island >= c.world().islands().size()) {
        return nullptr;
    }
    const sim::IslandMap& map = c.world().islands()[s.island].map;
    return map.contains(s.x, s.y) ? c.world().city(s.island, map.top(s.x, s.y).city) : nullptr;
}

/// LAGER: the stock of a city, also the ware picker of the Kontor lists and the route editor
/// (StockPanel_Refresh 0x00434810).
class StockList {
public:
    /// Minutes the goods trend is averaged over (the window "Warenbilanz" opens on the same).
    static constexpr std::uint32_t kTrendMinutes = 5;

    explicit StockList(assets::GadgetLayout layout);
    [[nodiscard]] ui::Panel& panel() noexcept { return m_panel; }
    /// As a picker, the 24th place shows the empty frame that chooses no ware. With `trend`, under
    /// each good what came in more than went out per minute over the last kTrendMinutes (green), or
    /// the other way round (red); nothing while it is less than 0.05 t (DisplaySettings::goods_trend).
    void refresh(const sim::City& city, bool picker, bool trend = false);
    /// The trend of a good as shown: "+1.4", "-0.3"; empty if it is not shown.
    [[nodiscard]] static std::string trend_text(const sim::City& city, std::uint8_t ware);
    /// The ware of a frame: 2..24, 0 for the empty frame, nothing for other gadgets.
    [[nodiscard]] std::optional<std::uint8_t> ware_of(std::int32_t id) const;

private:
    ui::Panel m_panel;
    std::int32_t m_gfx_ware = 192;
    std::int32_t m_gfx_empty = 189;
    std::int32_t m_corner = 634;
};

/// HANDEL: what a foreign Kontor wants (up to its limit) and offers (above what it keeps), also
/// the ware picker of the route editor at a foreign city (ForeignKontorPanel_Refresh).
class TradeLists {
public:
    static constexpr std::int32_t kDemandFrame = 53051;  ///< + row, 11 rows
    static constexpr std::int32_t kOfferFrame = 53091;   ///< + row, 8 rows
    explicit TradeLists(assets::GadgetLayout layout);
    [[nodiscard]] ui::Panel& panel() noexcept { return m_panel; }
    void refresh(const sim::City& city, const PanelContext& c);
    /// The ware of a row as last refreshed: (ware, the city buys it), nothing if none.
    [[nodiscard]] std::optional<std::pair<std::uint8_t, bool>> ware_of(std::int32_t id) const;
    [[nodiscard]] std::optional<std::string> help(std::int32_t id, const sim::City& city, const PanelContext& c) const;
    /// What the city still buys or sells of a ware, 0 below 1 t.
    [[nodiscard]] static std::uint32_t amount(const sim::City& city, std::uint8_t w, bool buy);

private:
    void fill(const sim::City& city, bool buy, std::int32_t icon, std::int32_t bar, std::int32_t frame,
              std::int32_t gadgets, std::int32_t places);
    ui::Panel m_panel;
    std::int32_t m_gfx_ware = 192;
    std::int32_t m_gfx_empty = 189;
    std::vector<std::uint8_t> m_buy;
    std::vector<std::uint8_t> m_sell;
};

}  // namespace panel_detail

/// The layouts of the ship's window and its pages.
struct ShipLayouts {
    assets::GadgetLayout ship;
    assets::GadgetLayout transfer;
    assets::GadgetLayout trade;
    assets::GadgetLayout route;
    assets::GadgetLayout lager;
    assets::GadgetLayout handel;
    assets::GadgetLayout pirat;
    assets::GadgetLayout piratord;
    /// MUSIK.GAD for its list arrows (the pages of the route editor); without it, four stops.
    std::optional<assets::GadgetLayout> musik;
};
/// The ship's window with its pages (ship_panel.cpp).
/// SHIPLIST (`ships`) or STADTLST: the lists of the player status (list_panels.cpp).
std::unique_ptr<SelectionPanel> make_list_panel(assets::GadgetLayout layout, bool ships);
/// VERTRAG with its tribute page TRIBUT (diplomacy_panel.cpp).
std::unique_ptr<SelectionPanel> make_diplomacy_panel(assets::GadgetLayout vertrag, assets::GadgetLayout tribut);
std::unique_ptr<SelectionPanel> make_ship_panel(ShipLayouts layouts);

/// The windows of the military mode (unit_panels.cpp): SKSHIP, SKSOLDAT, MKSHIP, MKSOLDAT.
std::unique_ptr<SelectionPanel> make_ship_unit_panel(assets::GadgetLayout layout);
std::unique_ptr<SelectionPanel> make_soldier_unit_panel(assets::GadgetLayout layout);
std::unique_ptr<SelectionPanel> make_group_panel(assets::GadgetLayout layout, bool ships);

}  // namespace o1602::app
