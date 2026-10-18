// SPDX-License-Identifier: GPL-3.0-or-later
//
// The windows of what the player clicked on the map (docs/re/ui.md#auswahlfenster): production
// buildings, public buildings, houses, the own Kontor, shipyards and castles, ships. Each window
// reads the game every frame and turns clicks into commands; it changes nothing itself.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/render/sdl_ui_renderer.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/ui/panel.hpp"
#include "o1602/ui/unit_selection.hpp"

namespace o1602::app {

/// What the player selected on the map.
struct Selection {
    enum class Kind : std::uint8_t { None, Building, Ship };
    Kind kind = Kind::None;
    std::uint16_t island = 0;  ///< Building: its origin on the island
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint32_t ship = 0;  ///< Ship: Ship::id
    /// Building: the same building was clicked again (a Kontor then shows its stock, 0x0043AA00).
    bool again = false;
    friend bool operator==(const Selection&, const Selection&) = default;
};

/// What the windows read.
struct PanelContext {
    const sim::Simulation& simulation;
    const assets::BuildingTable& buildings;
    const assets::FigureTable& figures;
    const assets::TextTable* texts;
    sim::PlayerId local = sim::kNoPlayer;
    /// The selected ships and land units (the military mode's windows change it: filters,
    /// formations).
    ui::UnitSelection* units = nullptr;
    [[nodiscard]] const sim::World& world() const noexcept { return simulation.world(); }
    /// A line of a Text.cod section, empty if there is none.
    [[nodiscard]] std::string text(std::string_view section, std::size_t line) const;
};

/// One window: one or more layouts (sub-pages) and what fills them.
class SelectionPanel {
public:
    SelectionPanel() = default;
    virtual ~SelectionPanel() = default;
    SelectionPanel(const SelectionPanel&) = delete;
    SelectionPanel& operator=(const SelectionPanel&) = delete;

    virtual void set_scale(double scale) = 0;
    /// The layout shown now.
    virtual ui::Panel& panel() = 0;
    /// Fills the gadgets from the game for this frame.
    virtual void refresh(const Selection& selection, const PanelContext& context) = 0;
    /// A press, click or slide; commands go to `out`.
    virtual void clicked(const ui::PanelEvent& event, const Selection& selection, const PanelContext& context,
                         std::vector<sim::Command>& out) = 0;
    /// Help text of the gadget under the mouse.
    [[nodiscard]] virtual std::optional<std::string> help(std::int32_t id, const Selection& selection,
                                                          const PanelContext& context) const = 0;
    /// Whether the window waits for a click on the map (a unit's destination, a route stop).
    [[nodiscard]] virtual bool picking() const { return false; }
    /// The click on the map while picking, world tile.
    virtual void picked(std::uint32_t /*x*/, std::uint32_t /*y*/, const Selection& /*selection*/,
                        const PanelContext& /*context*/, std::vector<sim::Command>& /*out*/) {}
    /// Escape: back one page or out of picking; false if there is nothing to go back from.
    virtual bool back() { return false; }
    /// What the window asks of the interface beyond commands (once).
    struct Request {
        enum class Kind : std::uint8_t { FoundKontor };
        Kind kind = Kind::FoundKontor;
        std::uint32_t ship = 0;  ///< FoundKontor: the ship whose cargo pays
    };
    [[nodiscard]] virtual std::optional<Request> take_request() { return std::nullopt; }
};

/// A window of one layout.
class SimplePanel : public SelectionPanel {
public:
    explicit SimplePanel(assets::GadgetLayout layout) : m_panel(std::move(layout), 1.0) {}
    void set_scale(double scale) override { m_panel.set_scale(scale); }
    ui::Panel& panel() override { return m_panel; }

protected:
    ui::Panel m_panel;
};

/// All windows of the selections, loaded once.
class SelectionPanels {
public:
    SelectionPanels(const assets::GameDirectory& game);

    /// The window for a selection, nothing if it has none (the info mode then shows the player
    /// status).
    [[nodiscard]] SelectionPanel* panel_for(const Selection& selection, const PanelContext& context);
    /// The window of the military mode for the selected units (Panel_UpdateSelection 0x0043AA00):
    /// one ship, one land unit, a group of ships or of land units; nothing without a selection.
    [[nodiscard]] SelectionPanel* military_panel_for(const PanelContext& context);
    void set_scale(double scale);

private:
    std::map<std::string, std::unique_ptr<SelectionPanel>> m_panels;
};

}  // namespace o1602::app
