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

constexpr std::int32_t kTop = 84;
constexpr std::int32_t kGap = 8;
constexpr std::int32_t kRowH = 36;
constexpr std::int32_t kLabelW = 236;
constexpr std::int32_t kValueW = 196;

struct Group {
    std::string_view heading;
    SetupItem first;
    SetupItem last;  ///< inclusive
    std::int32_t x;  ///< the column; the groups of a column follow each other from the top
};

// Economy, limits and disasters on the left, the world and the opponents on the right.
constexpr Group kGroups[] = {
    {"setup.group.economy", SetupItem::Refund, SetupItem::TraderPrices, 40},
    {"setup.group.limits", SetupItem::Ships, SetupItem::GameSpeed, 40},
    {"setup.group.disasters", SetupItem::Droughts, SetupItem::Volcanoes, 40},
    {"setup.group.world", SetupItem::Map, SetupItem::Resources, 524},
    {"setup.group.opponents", SetupItem::ClassicAi, SetupItem::ClassicAi, 524},
};

std::string_view title_key(SetupScope scope) {
    switch (scope) {
        case SetupScope::MultiplayerEndless:
            return "setup.title.multiplayer";
        case SetupScope::Scenario:
            return "setup.title.scenario";
        case SetupScope::Savegame:
            return "setup.title.savegame";
        case SetupScope::Endless:
            break;
    }
    return "setup.title";
}

std::string_view hint_key(SetupScope scope) {
    switch (scope) {
        case SetupScope::MultiplayerEndless:
            return "setup.hint.multiplayer";
        case SetupScope::Scenario:
            return "setup.hint.scenario";
        case SetupScope::Savegame:
            return "setup.hint.savegame";
        case SetupScope::Endless:
            break;
    }
    return "setup.hint";
}

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
    const bool editable = m_mode != Mode::View;
    if (*id >= kItem && *id < kItem + static_cast<Id>(kSetupItems)) {
        const auto item = static_cast<SetupItem>(*id - kItem);
        if (editable && setup_item_open(m_setup, m_scope, item)) {
            choose_next(m_setup, item);
        }
    } else if (*id == kOriginal && editable) {
        m_setup = keep_decided(EndlessSetup::original(m_scope));
    } else if (*id == kDefaults && editable) {
        m_setup = keep_decided(EndlessSetup::defaults(m_scope));
    } else if (*id == kBack) {
        return Result::Back;
    } else if (*id == kStart && editable) {
        return Result::Start;
    }
    return Result::None;
}

EndlessSetup GameSettingsMenu::keep_decided(EndlessSetup preset) const {
    if (m_scope == SetupScope::MultiplayerEndless) {
        // The players are the lobby's, whatever the preset says.
        preset = multiplayer_setup(preset, m_setup.world.players);
    }
    return preset;
}

void GameSettingsMenu::layout() {
    ui::Form& f = m_form;
    f.clear();
    f.panel(kScreen);
    f.label(kHeading, ui::tr(title_key(m_scope)), Align::Left, Tone::Heading);
    const bool editable = m_mode != Mode::View;
    std::int32_t left_y = kTop;
    std::int32_t right_y = kTop;
    for (const Group& g : kGroups) {
        std::vector<SetupItem> items;
        for (auto i = static_cast<std::int32_t>(g.first); i <= static_cast<std::int32_t>(g.last); ++i) {
            if (setup_item_offered(m_scope, static_cast<SetupItem>(i))) {
                items.push_back(static_cast<SetupItem>(i));
            }
        }
        if (items.empty()) {
            continue;
        }
        std::int32_t& y0 = g.x < kScreen.x + kScreen.w / 2 ? left_y : right_y;
        const auto rows = static_cast<std::int32_t>(items.size());
        f.panel(Rect{g.x, y0, kLabelW + kValueW + 28, 44 + rows * kRowH});
        f.label(Rect{g.x + 12, y0 + 6, kLabelW + kValueW, 32}, ui::tr(g.heading), Align::Left, Tone::Heading);
        for (std::int32_t i = 0; i < rows; ++i) {
            const SetupItem item = items[static_cast<std::size_t>(i)];
            const std::int32_t y = y0 + 40 + i * kRowH;
            const bool used = setup_item_used(m_setup, item);
            f.label(Rect{g.x + 12, y, kLabelW, kRowH - 4}, setup_label(item), Align::Left,
                    used ? Tone::Normal : Tone::Dim);
            const Rect value{g.x + 16 + kLabelW, y, kValueW, kRowH - 6};
            if (editable) {
                f.button(kItem + static_cast<Id>(item), value, setup_value(m_setup, item),
                         setup_item_open(m_setup, m_scope, item));
            } else {
                f.label(value, setup_value(m_setup, item), Align::Left, used ? Tone::Normal : Tone::Dim);
            }
        }
        y0 += 44 + rows * kRowH + kGap;
    }
    f.paragraph(Rect{40, 592, 944, 90}, ui::tr(hint_key(m_scope)), Tone::Dim);
    f.button(kBack, Rect{40, kBottomY, 180, 44}, ui::tr("setup.back"));
    if (!editable) {
        return;
    }
    f.button(kOriginal, Rect{260, kBottomY, 240, 44}, ui::tr("setup.original"), true,
             same_setup(m_setup, keep_decided(EndlessSetup::original(m_scope)), m_scope));
    f.button(kDefaults, Rect{520, kBottomY, 240, 44}, ui::tr("setup.defaults"), true,
             same_setup(m_setup, keep_decided(EndlessSetup::defaults(m_scope)), m_scope));
    f.button(kStart, Rect{804, kBottomY, 180, 44}, ui::tr(m_mode == Mode::Apply ? "setup.apply" : "setup.start"), true,
             true);
}

}  // namespace o1602::app
