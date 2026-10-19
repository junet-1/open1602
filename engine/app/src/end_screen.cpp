// SPDX-License-Identifier: GPL-3.0-or-later
#include "end_screen.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "o1602/ui/localization.hpp"

namespace o1602::app {

namespace {

// Fonts and text colours of the original (ui::font_file, ui::text_colour).
constexpr std::int32_t kFontNormal = 0;
constexpr std::int32_t kFontLarge = 2;  // digits only
constexpr std::int32_t kText = 4;
constexpr std::int32_t kHighlight = 5;
// Palette colours of the buttons, as the questions draw them (Hud::draw_question).
constexpr std::uint8_t kGold = 215;
constexpr std::uint8_t kDark = 28;

// The own layout over ENDE.GAD, design units (PROVISIONAL, see end_screen.hpp).
constexpr std::int32_t kCentre = 512;
constexpr std::int32_t kTitleY = 110;
constexpr std::int32_t kScoreY = 175;
constexpr std::int32_t kSubtitleY = 210;
constexpr std::int32_t kTableY = 260;
constexpr std::int32_t kRowHeight = 26;
constexpr std::size_t kMaxRows = 8;
constexpr std::int32_t kListY = 520;
constexpr std::size_t kListColumnRows = 6;
constexpr std::int32_t kButtonY = 690;
constexpr std::int32_t kButtonW = 200;
constexpr std::int32_t kButtonH = 34;

struct Column {
    std::int32_t x;
    ui::Align align;
    const char* heading;
};
constexpr std::array<Column, 6> kColumns = {{
    {150, ui::Align::Left, "end.column.player"},
    {520, ui::Align::Right, "end.column.inhabitants"},
    {610, ui::Align::Right, "end.column.cities"},
    {700, ui::Align::Right, "end.column.ships"},
    {800, ui::Align::Right, "end.column.money"},
    {900, ui::Align::Right, "end.column.points"},
}};

const std::string& outcome_title(sim::GameOutcome o) {
    switch (o) {
        case sim::GameOutcome::Won:
            return ui::tr("end.title.won");
        case sim::GameOutcome::Lost:
            return ui::tr("end.title.lost");
        case sim::GameOutcome::Left:
            break;
    }
    return ui::tr("end.title.left");
}

std::string player_state(const sim::EndScreenRow& r) {
    if (r.won) {
        return ui::tr("end.state.won");
    }
    if (r.status == sim::PlayerStatus::Defeated) {
        return ui::tr("end.state.defeated");
    }
    return {};
}

}  // namespace

std::string game_time_text(std::uint64_t time_ms) {
    const std::uint64_t minutes = time_ms / 60'000;
    const std::uint64_t m = minutes % 60;
    return std::to_string(minutes / 60) + (m < 10 ? ":0" : ":") + std::to_string(m);
}

EndScreen::EndScreen(std::optional<assets::GadgetLayout> ende) {
    if (ende) {
        m_panel.emplace(std::move(*ende), m_scale);
        // Its texts and numbers are not known yet (PROVISIONAL): only the pictures show.
        for (const assets::Gadget& g : m_panel->layout().gadgets) {
            if (g.text()) {
                m_panel->set_hidden(g.id, true);
            }
        }
    }
}

void EndScreen::open(sim::EndScreenSummary summary, std::vector<assets::HighScore> scores,
                     std::optional<std::size_t> rank) {
    m_summary = std::move(summary);
    m_scores = std::move(scores);
    m_rank = rank;
    m_pressed.reset();
    m_down = false;
    m_buttons.clear();
    // A won game may go on (the simulation checks no missions after the end); the others end.
    if (m_summary.outcome == sim::GameOutcome::Won) {
        m_buttons.push_back(
            {{kCentre - kButtonW - 20, kButtonY, kButtonW, kButtonH}, Choice::Continue, ui::tr("end.continue")});
        m_buttons.push_back({{kCentre + 20, kButtonY, kButtonW, kButtonH}, Choice::MainMenu, ui::tr("end.main_menu")});
    } else {
        m_buttons.push_back(
            {{kCentre - kButtonW / 2, kButtonY, kButtonW, kButtonH}, Choice::MainMenu, ui::tr("end.main_menu")});
    }
}

void EndScreen::set_scale(double scale) {
    m_scale = scale;
    if (m_panel) {
        m_panel->set_scale(scale);
    }
}

ui::Rect EndScreen::pixels(const ui::Rect& d) const noexcept {
    return {ui::to_pixels(d.x, m_scale), ui::to_pixels(d.y, m_scale), ui::to_pixels(d.w, m_scale),
            ui::to_pixels(d.h, m_scale)};
}

void EndScreen::draw(render::SdlUiRenderer& renderer, std::int32_t x, std::int32_t y) {
    if (m_panel) {
        renderer.draw(m_panel->draw(renderer.fonts()), x, y, m_scale);
    }
    std::vector<ui::DrawItem> items;
    const auto text = [&](std::string t, std::int32_t tx, std::int32_t ty, ui::Align align,
                          std::int32_t font = kFontNormal, std::int32_t colour = kText) {
        ui::DrawItem d;
        d.kind = ui::DrawItem::Kind::Text;
        d.block = font;
        d.colour = colour;
        d.align = align;
        d.x = ui::to_pixels(tx, m_scale);
        d.y = ui::to_pixels(ty, m_scale);
        d.text = std::move(t);
        items.push_back(std::move(d));
    };

    text(outcome_title(m_summary.outcome), kCentre, kTitleY, ui::Align::Centre);
    const auto local = std::find_if(m_summary.rows.begin(), m_summary.rows.end(),
                                    [&](const sim::EndScreenRow& r) { return r.player == m_summary.local; });
    if (local != m_summary.rows.end()) {
        text(std::to_string(local->score), kCentre, kScoreY, ui::Align::Centre, kFontLarge);
    }
    std::string subtitle = ui::tr("end.game_time", {game_time_text(m_summary.time_ms)});
    if (!m_summary.game.name.empty()) {
        subtitle = m_summary.game.name + "  -  " + subtitle;
    }
    text(std::move(subtitle), kCentre, kSubtitleY, ui::Align::Centre);

    for (const Column& c : kColumns) {
        text(ui::tr(c.heading), c.x, kTableY, c.align);
    }
    for (std::size_t i = 0; i < std::min(m_summary.rows.size(), kMaxRows); ++i) {
        const sim::EndScreenRow& r = m_summary.rows[i];
        const std::int32_t ry = kTableY + kRowHeight * static_cast<std::int32_t>(i + 1);
        const std::int32_t colour = r.player == m_summary.local ? kHighlight : kText;
        std::string name = r.name.empty() ? ui::tr("end.player", {std::to_string(r.player + 1)}) : r.name;
        if (const std::string state = player_state(r); !state.empty()) {
            name += " (" + state + ")";
        }
        const std::array<std::string, 6> values{name,
                                                std::to_string(r.inhabitants),
                                                std::to_string(r.cities),
                                                std::to_string(r.ships),
                                                std::to_string(r.money),
                                                std::to_string(r.score)};
        for (std::size_t k = 0; k < kColumns.size(); ++k) {
            text(values[k], kColumns[k].x, ry, kColumns[k].align, kFontNormal, colour);
        }
    }

    if (!m_scores.empty()) {
        text(ui::tr("end.high_scores"), kCentre, kListY, ui::Align::Centre);
        for (std::size_t i = 0; i < m_scores.size(); ++i) {
            if (m_scores[i].name.empty()) {
                continue;
            }
            const auto column = static_cast<std::int32_t>(i / kListColumnRows);
            const std::int32_t lx = 150 + column * 400;
            const std::int32_t ly = kListY + kRowHeight * static_cast<std::int32_t>(i % kListColumnRows + 1);
            const std::int32_t colour = m_rank && *m_rank == i ? kHighlight : kText;
            text(std::to_string(i + 1) + ".", lx, ly, ui::Align::Left, kFontNormal, colour);
            text(m_scores[i].name, lx + 40, ly, ui::Align::Left, kFontNormal, colour);
            text(std::to_string(m_scores[i].points), lx + 330, ly, ui::Align::Right, kFontNormal, colour);
        }
    }

    for (std::size_t i = 0; i < m_buttons.size(); ++i) {
        const Button& b = m_buttons[i];
        const ui::Rect r = pixels(b.rect);
        const std::int32_t edge = std::max(1, ui::to_pixels(2, m_scale));
        renderer.fill(ui::Rect{x + r.x, y + r.y, r.w, r.h}, kGold);
        renderer.fill(ui::Rect{x + r.x + edge, y + r.y + edge, r.w - 2 * edge, r.h - 2 * edge}, kDark);
        text(b.label, b.rect.x + b.rect.w / 2, b.rect.y + 26, ui::Align::Centre, kFontNormal,
             m_pressed && *m_pressed == i ? kHighlight : kText);
    }
    renderer.draw(items, x, y, m_scale);
}

std::optional<EndScreen::Choice> EndScreen::mouse(std::int32_t x, std::int32_t y, bool down) {
    std::optional<std::size_t> at;
    for (std::size_t i = 0; i < m_buttons.size(); ++i) {
        if (pixels(m_buttons[i].rect).contains(x, y)) {
            at = i;
        }
    }
    // Motion comes with down == false as well: only a release after a press counts, and only on
    // the button the press began on (as on the questions).
    if (down) {
        if (!m_down) {
            m_down = true;
            m_pressed = at;
        }
        return std::nullopt;
    }
    if (!std::exchange(m_down, false)) {
        return std::nullopt;
    }
    const std::optional<std::size_t> pressed = std::exchange(m_pressed, std::nullopt);
    if (pressed && at == pressed) {
        return m_buttons[*pressed].choice;
    }
    return std::nullopt;
}

}  // namespace o1602::app
