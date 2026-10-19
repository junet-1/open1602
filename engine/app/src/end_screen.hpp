// SPDX-License-Identifier: GPL-3.0-or-later
//
// The end screen after a game (docs/re/ui.md "Endbildschirm"; ENDE.GAD): how the game ended, the
// players' figures and points, and the scenario's high score list with the new line.
//
// PROVISIONAL LAYOUT: what each gadget of ENDE.GAD shows and how the original fills it is not
// analysed yet. Until then the screen draws the layout as its background (its pictures, no
// texts) and lays out its own lines over it, in the original's fonts and colours, with
// Open1602's texts (data/locale "end.*"). Once the gadgets are known, refresh() fills them and the
// own lines go.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/gad.hpp"
#include "o1602/assets/scenario.hpp"
#include "o1602/render/sdl_ui_renderer.hpp"
#include "o1602/sim/score.hpp"
#include "o1602/ui/panel.hpp"

namespace o1602::app {

class EndScreen {
public:
    /// What the player chose.
    enum class Choice : std::uint8_t {
        MainMenu,  ///< back to the main menu
        Continue,  ///< keep playing the won game (Open1602; the original ends it)
    };

    /// `ende`: the layout ENDE.GAD, nothing if the installation lacks it (the screen is then black
    /// behind its lines).
    explicit EndScreen(std::optional<assets::GadgetLayout> ende);

    /// Shows a finished game. `scores` is the scenario's list after the game's line went in at
    /// `rank` (nothing: not in the list, or no list).
    void open(sim::EndScreenSummary summary, std::vector<assets::HighScore> scores, std::optional<std::size_t> rank);
    void set_scale(double scale);

    /// Draws over the whole 1024 x 768 design, its top left at (x, y).
    void draw(render::SdlUiRenderer& renderer, std::int32_t x, std::int32_t y);
    /// The mouse relative to the design's top left, in pixels: a click on a button chooses.
    std::optional<Choice> mouse(std::int32_t x, std::int32_t y, bool down);
    /// Escape and Enter: the main menu.
    [[nodiscard]] static Choice back() noexcept { return Choice::MainMenu; }

    [[nodiscard]] const sim::EndScreenSummary& summary() const noexcept { return m_summary; }

private:
    struct Button {
        ui::Rect rect;  ///< design units
        Choice choice = Choice::MainMenu;
        std::string label;
    };
    [[nodiscard]] ui::Rect pixels(const ui::Rect& design) const noexcept;

    std::optional<ui::Panel> m_panel;
    double m_scale = 1.0;
    sim::EndScreenSummary m_summary;
    std::vector<assets::HighScore> m_scores;
    std::optional<std::size_t> m_rank;
    std::vector<Button> m_buttons;
    std::optional<std::size_t> m_pressed;
    bool m_down = false;
};

/// "h:mm" of a game time.
[[nodiscard]] std::string game_time_text(std::uint64_t time_ms);

}  // namespace o1602::app
