// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_settings_menu.hpp"

#include <string>
#include <string_view>

#include "o1602/ui/localization.hpp"

namespace o1602::app {

namespace {

using Id = ui::Form::Id;
using Tone = ui::Form::Tone;
using ui::Align;
using ui::Rect;

constexpr Id kItem = 1;  // + SetupItem
constexpr Id kOriginal = 50;
constexpr Id kDefaults = 51;
constexpr Id kBack = 52;
constexpr Id kStart = 53;

// The frame and the bottom row, as the multiplayer menu's.
constexpr Rect kScreen{16, 16, 992, 736};
constexpr Rect kHeading{40, 28, 760, 40};
constexpr std::int32_t kBottomY = 690;

constexpr std::int32_t kRowH = 40;
constexpr std::int32_t kLabelW = 236;
constexpr std::int32_t kValueW = 196;

struct Group {
    std::string_view heading;
    SetupItem first;
    SetupItem last;  ///< inclusive
    std::int32_t x;
    std::int32_t y;
};

// Economy, limits and disasters on the left, the world and the opponents on the right.
constexpr Group kGroups[] = {
    {"setup.group.economy", SetupItem::Refund, SetupItem::TraderPrices, 40, 84},
    {"setup.group.limits", SetupItem::Ships, SetupItem::GameSpeed, 40, 216},
    {"setup.group.disasters", SetupItem::Droughts, SetupItem::Volcanoes, 40, 388},
    {"setup.group.world", SetupItem::Map, SetupItem::Resources, 524, 84},
    {"setup.group.opponents", SetupItem::ClassicAi, SetupItem::ClassicAi, 524, 496},
};

}  // namespace

std::vector<ui::DrawItem> GameSettingsMenu::draw(const std::array<const ui::Font*, 3>& fonts) {
    layout();
    return m_form.draw(fonts);
}

GameSettingsMenu::Result GameSettingsMenu::mouse(std::int32_t x, std::int32_t y, bool down) {
    layout();
    const auto id = m_form.mouse(x, y, down);
    if (!id) {
        return Result::None;
    }
    if (*id >= kItem && *id < kItem + static_cast<Id>(kSetupItems)) {
        choose_next(m_setup, static_cast<SetupItem>(*id - kItem));
    } else if (*id == kOriginal) {
        m_setup = EndlessSetup::original();
    } else if (*id == kDefaults) {
        m_setup = EndlessSetup{};
    } else if (*id == kBack) {
        return Result::Back;
    } else if (*id == kStart) {
        return Result::Start;
    }
    return Result::None;
}

void GameSettingsMenu::layout() {
    ui::Form& f = m_form;
    f.clear();
    f.panel(kScreen);
    f.label(kHeading, ui::tr("setup.title"), Align::Left, Tone::Heading);
    for (const Group& g : kGroups) {
        const auto first = static_cast<std::int32_t>(g.first);
        const auto rows = static_cast<std::int32_t>(g.last) - first + 1;
        f.panel(Rect{g.x, g.y, kLabelW + kValueW + 28, 44 + rows * kRowH});
        f.label(Rect{g.x + 12, g.y + 6, kLabelW + kValueW, 32}, ui::tr(g.heading), Align::Left, Tone::Heading);
        for (std::int32_t i = 0; i < rows; ++i) {
            const auto item = static_cast<SetupItem>(first + i);
            const std::int32_t y = g.y + 40 + i * kRowH;
            const bool used = setup_item_used(m_setup, item);
            f.label(Rect{g.x + 12, y, kLabelW, kRowH - 4}, setup_label(item), Align::Left,
                    used ? Tone::Normal : Tone::Dim);
            f.button(kItem + first + i, Rect{g.x + 16 + kLabelW, y, kValueW, kRowH - 6},
                     setup_value(item, setup_choice(m_setup, item)), used);
        }
    }
    f.paragraph(Rect{40, 592, 944, 90}, ui::tr("setup.hint"), Tone::Dim);
    f.button(kBack, Rect{40, kBottomY, 180, 44}, ui::tr("setup.back"));
    f.button(kOriginal, Rect{260, kBottomY, 240, 44}, ui::tr("setup.original"), true,
             m_setup == EndlessSetup::original());
    f.button(kDefaults, Rect{520, kBottomY, 240, 44}, ui::tr("setup.defaults"), true, m_setup == EndlessSetup{});
    f.button(kStart, Rect{804, kBottomY, 180, 44}, ui::tr("setup.start"), true, true);
}

}  // namespace o1602::app
