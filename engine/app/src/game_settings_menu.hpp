// SPDX-License-Identifier: GPL-3.0-or-later
//
// The start dialog of the endless game (ADR 0008): Open1602's own screen after the name and the
// banner, the original has none. The settings in groups (economy, limits, disasters, world), each
// a button with its value; a click chooses the next value. Presets for the original's rules and
// Open1602's defaults.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "endless_setup.hpp"

#include "o1602/ui/form.hpp"

namespace o1602::app {

class GameSettingsMenu {
public:
    enum class Result : std::uint8_t { None, Start, Back };

    explicit GameSettingsMenu(ui::FormStyle style = {}) : m_form(style) {}

    void set_style(const ui::FormStyle& style) { m_form.set_style(style); }
    void set_scale(double scale) { m_form.set_scale(scale); }
    void open(const EndlessSetup& setup) { m_setup = setup; }
    [[nodiscard]] const EndlessSetup& setup() const noexcept { return m_setup; }

    /// Everything to draw, relative to the 1024 x 768 design's top left.
    [[nodiscard]] std::vector<ui::DrawItem> draw(const std::array<const ui::Font*, 3>& fonts);
    Result mouse(std::int32_t x, std::int32_t y, bool down);

private:
    void layout();

    ui::Form m_form;
    EndlessSetup m_setup;
};

}  // namespace o1602::app
