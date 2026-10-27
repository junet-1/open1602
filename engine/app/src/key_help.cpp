// SPDX-License-Identifier: GPL-3.0-or-later
#include "key_help.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "o1602/ui/localization.hpp"

namespace o1602::app {

namespace {

using ui::KeyAction;

/// Pages and their sections, text keys of the localization.
constexpr std::array<const char*, kKeyHelpPages> kPageTitles = {
    "keys.page.map_game", "keys.page.windows", "keys.page.building", "keys.page.units", "keys.page.view"};
constexpr std::array<std::array<const char*, 2>, kKeyHelpPages> kSectionHeadings = {{
    {"keys.heading.map", "keys.heading.game"},
    {"keys.heading.windows", nullptr},
    {"keys.heading.building", nullptr},
    {"keys.heading.groups", "keys.heading.units"},
    {"keys.heading.view", nullptr},
}};

}  // namespace

KeyHelpPlace key_help_place(KeyAction action) noexcept {
    switch (action) {
        case KeyAction::ScrollUp:
        case KeyAction::ScrollDown:
        case KeyAction::ScrollLeft:
        case KeyAction::ScrollRight:
        case KeyAction::RotateViewLeft:
        case KeyAction::RotateViewRight:
        case KeyAction::ZoomIn:
        case KeyAction::ZoomOut:
        case KeyAction::JumpToMessage:
        case KeyAction::CentreOnSelection:
        case KeyAction::NextCity:
            return {0, 0};
        case KeyAction::Pause:
        case KeyAction::PauseVideo:
        case KeyAction::SpeedUp:
        case KeyAction::SpeedDown:
        case KeyAction::QuickSave:
        case KeyAction::QuickLoad:
        case KeyAction::GameMenu:
        case KeyAction::Screenshot:
        case KeyAction::Help:
        case KeyAction::Cancel:
        case KeyAction::Quit:
            return {0, 1};
        case KeyAction::ModeBuild:
        case KeyAction::ModeMilitary:
        case KeyAction::ModeInfo:
        case KeyAction::ModeOptions:
        case KeyAction::ShipList:
        case KeyAction::CityList:
        case KeyAction::Diplomacy:
        case KeyAction::GoodsStatistics:
        case KeyAction::Chat:
            return {1, 0};
        case KeyAction::RotateBuilding:
        case KeyAction::RotateBuildingBack:
        case KeyAction::PickBuilding:
        case KeyAction::DemolishTool:
        case KeyAction::NextBuilding:
            return {2, 0};
        case KeyAction::GroupStore:
        case KeyAction::GroupAdd:
        case KeyAction::GroupRecall:
        case KeyAction::NextIdleShip:
        case KeyAction::NextIdleUnit:
        case KeyAction::SelectInfantry:
        case KeyAction::SelectMusketeers:
        case KeyAction::SelectCavalry:
        case KeyAction::SelectCannoneers:
            return {3, 0};
        case KeyAction::NextKindInSelection:
        case KeyAction::AttackMove:
        case KeyAction::Stop:
        case KeyAction::HoldPosition:
        case KeyAction::Patrol:
        case KeyAction::Formation:
        case KeyAction::Scatter:
        case KeyAction::WhiteFlag:
        case KeyAction::Explore:
        case KeyAction::Trade:
        case KeyAction::EditRoute:
        case KeyAction::Delete:
            return {3, 1};
        case KeyAction::PixelScaleUp:
        case KeyAction::PixelScaleDown:
        case KeyAction::ShowOwners:
        case KeyAction::ShowFps:
        case KeyAction::ShowCoordinates:
        case KeyAction::Fullscreen:
            return {4, 0};
    }
    return {0, 1};
}

std::array<KeyHelpPage, kKeyHelpPages> key_help_pages(const std::vector<KeyHelpEntry>& entries,
                                                      const std::function<std::string(ui::KeyAction)>& label) {
    // One line per action, its keys joined in the order they come.
    std::vector<std::pair<KeyAction, std::string>> actions;
    for (const KeyHelpEntry& e : entries) {
        const auto it =
            std::find_if(actions.begin(), actions.end(), [&](const auto& a) { return a.first == e.action; });
        if (it == actions.end()) {
            actions.emplace_back(e.action, e.keys);
            continue;
        }
        // Each key once ("Entf" in two contexts); a key's whole name, not a part of another's.
        std::size_t start = 0;
        bool known = false;
        while (start <= it->second.size()) {
            const std::size_t end = std::min(it->second.find(", ", start), it->second.size());
            known |= it->second.compare(start, end - start, e.keys) == 0;
            start = end + 2;
        }
        if (!known) {
            it->second += ", " + e.keys;
        }
    }
    std::stable_sort(actions.begin(), actions.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::array<KeyHelpPage, kKeyHelpPages> pages;
    std::array<std::array<std::vector<KeyHelpLine>, 2>, kKeyHelpPages> lines;
    for (const auto& [action, keys] : actions) {
        const KeyHelpPlace p = key_help_place(action);
        lines[p.page][p.section].push_back(KeyHelpLine{keys, label(action)});
    }
    for (std::size_t p = 0; p < kKeyHelpPages; ++p) {
        pages[p].title = ui::tr(kPageTitles[p]);
        for (std::size_t s = 0; s < 2; ++s) {
            if (kSectionHeadings[p][s] != nullptr && !lines[p][s].empty()) {
                pages[p].sections.push_back(KeyHelpSection{ui::tr(kSectionHeadings[p][s]), std::move(lines[p][s])});
            }
        }
    }
    return pages;
}

std::size_t key_help_rows(const KeyHelpPage& page) noexcept {
    if (page.sections.empty()) {
        return 0;
    }
    if (page.sections.size() == 1) {
        return 1 + (page.sections[0].lines.size() + 1) / 2;
    }
    std::size_t longest = 0;
    for (const KeyHelpSection& s : page.sections) {
        longest = std::max(longest, s.lines.size());
    }
    return 1 + longest;
}

KeyHelpCell key_help_cell(const KeyHelpPage& page, std::size_t section, std::size_t line) noexcept {
    if (line == SIZE_MAX) {
        return {section, 0};
    }
    if (page.sections.size() == 1) {
        const std::size_t rows = (page.sections[0].lines.size() + 1) / 2;
        return {line / rows, 1 + line % rows};
    }
    return {section, 1 + line};
}

}  // namespace o1602::app
