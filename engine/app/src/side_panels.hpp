// SPDX-License-Identifier: GPL-3.0-or-later
//
// The panels below the head of the side panel (docs/re/ui.md#modi): the build menu (BAU.GAD and
// its pop-up of building icons) and the player status of the info mode (INFO.GAD). They only
// show and ask; what a click means for the game goes back to the caller as an action.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "selection_panels.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/gad.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/render/sdl_ui_renderer.hpp"
#include "o1602/sim/mission.hpp"
#include "o1602/ui/build_menu.hpp"
#include "o1602/ui/panel.hpp"

namespace o1602::app {

/// The build menu (BuildMenu_OnCategoryPress 0x00432A70, BuildMenu_Refresh 0x00433520).
class BuildPanel {
public:
    /// What a click asks for.
    struct Action {
        enum class Kind : std::uint8_t { Pick, Demolish, Rotate, OrientToRoad };
        Kind kind = Kind::Pick;
        std::uint32_t building = 0;  ///< Pick: HAUS Id
        int step = 0;                ///< Rotate: -1 left, +1 right; OrientToRoad: 1 on, 0 off
    };
    /// Whether the local player may pick a building (sim::building_available).
    using Available = std::function<bool(std::uint32_t id)>;

    BuildPanel(assets::GadgetLayout bau, const ui::BuildMenuDefinition& menu, const assets::TextTable* texts);

    void set_scale(double scale);
    /// The mouse relative to the side panel. `consumed` tells whether the panel took it (an open
    /// pop-up takes every press on the panel).
    std::optional<Action> mouse(std::int32_t x, std::int32_t y, bool down, const Available& available, bool& consumed);
    [[nodiscard]] bool popup_open() const noexcept { return m_popup.has_value(); }
    void close_popup() noexcept { m_popup.reset(); }

    /// The building of the open category (nothing: demolish, or nothing available there). Picks
    /// another available one if the remembered one is not (the original keeps it, S-014).
    [[nodiscard]] std::optional<std::uint32_t> current(const Available& available);
    [[nodiscard]] bool demolish() const noexcept;
    /// Shows a building's category with it picked (a plantation's field, 0x004331B0).
    /// False if the menu has no such building.
    bool show(std::uint32_t id);

    /// Help text of the gadget under the mouse, nothing if none.
    [[nodiscard]] std::optional<std::string> help(const std::vector<std::string>& leiste) const;

    /// Title, costs, preview and production chain of the current building, then the panel and
    /// the pop-up. `rotation`: view plus building rotation (the preview turns with both).
    void draw(render::SdlUiRenderer& renderer, const assets::BuildingTable& buildings, const Available& available,
              std::uint8_t rotation, const ui::Rect& region, double scale);

private:
    void open_popup(const ui::BuildCategory& category, const Available& available, std::int32_t x, std::int32_t y);
    [[nodiscard]] std::string name_of(const ui::BuildCategory& category, std::size_t index) const;

    ui::Panel m_bau;
    assets::GadgetLayout m_layout;  ///< the file as loaded (offsets before moving groups)
    const ui::BuildMenuDefinition* m_menu;
    const assets::TextTable* m_texts;
    std::int32_t m_gfx_bauhaus = 81;
    std::int32_t m_gfx_ware = 192;
    std::int32_t m_category = 0;
    std::map<std::int32_t, std::size_t> m_picked;  ///< remembered position per category
    std::optional<ui::Panel> m_popup;
    std::int32_t m_popup_category = 0;
};

/// "Spielerstatus" of the info mode without a selection (INFO.GAD, InfoPanel_Refresh 0x00434420).
class InfoPanel {
public:
    InfoPanel(assets::GadgetLayout info, const assets::TextTable* texts);
    void set_scale(double scale) { m_panel.set_scale(scale); }
    /// The mouse; a click on a list button returns which list to open.
    std::optional<Selection::Kind> mouse(std::int32_t x, std::int32_t y, bool down);
    /// Whether "Bewertung" was clicked since the last call.
    [[nodiscard]] bool take_rating() noexcept { return std::exchange(m_rating_clicked, false); }
    /// "Punkte" (57027, Player_ComputeScore's total) and whether "Bewertung" can be pressed.
    void set_points(std::int64_t points, bool rating);
    [[nodiscard]] std::optional<std::string> help(const std::vector<std::string>& leiste) const;
    void draw(render::SdlUiRenderer& renderer, const sim::PlayerFigures& figures, std::uint8_t colour,
              const ui::Rect& region, double scale);

private:
    ui::Panel m_panel;
    std::vector<std::string> m_game;  ///< Text.cod [GAME]
    std::int32_t m_arms = 0;          ///< sprite of the coat of arms of colour 0
    std::int32_t m_balance_icon = 0;  ///< sprite of the balance icon (green; red is the next)
    bool m_rating_clicked = false;
};

}  // namespace o1602::app
