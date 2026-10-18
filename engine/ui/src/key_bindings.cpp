// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/ui/key_bindings.hpp"

#include <algorithm>
#include <utility>

namespace o1602::ui {

namespace {

constexpr std::pair<KeyAction, std::string_view> kNames[] = {
    {KeyAction::ScrollUp, "scroll_up"},
    {KeyAction::ScrollDown, "scroll_down"},
    {KeyAction::ScrollLeft, "scroll_left"},
    {KeyAction::ScrollRight, "scroll_right"},
    {KeyAction::RotateViewLeft, "rotate_view_left"},
    {KeyAction::RotateViewRight, "rotate_view_right"},
    {KeyAction::ZoomIn, "zoom_in"},
    {KeyAction::ZoomOut, "zoom_out"},
    {KeyAction::JumpToMessage, "jump_to_message"},
    {KeyAction::CentreOnSelection, "centre_on_selection"},
    {KeyAction::Pause, "pause"},
    {KeyAction::SpeedUp, "speed_up"},
    {KeyAction::SpeedDown, "speed_down"},
    {KeyAction::QuickSave, "quick_save"},
    {KeyAction::QuickLoad, "quick_load"},
    {KeyAction::GameMenu, "game_menu"},
    {KeyAction::Screenshot, "screenshot"},
    {KeyAction::Help, "help"},
    {KeyAction::Cancel, "cancel"},
    {KeyAction::ModeBuild, "mode_build"},
    {KeyAction::ModeMilitary, "mode_military"},
    {KeyAction::ModeInfo, "mode_info"},
    {KeyAction::ModeOptions, "mode_options"},
    {KeyAction::ShipList, "ship_list"},
    {KeyAction::CityList, "city_list"},
    {KeyAction::Diplomacy, "diplomacy"},
    {KeyAction::RotateBuilding, "rotate_building"},
    {KeyAction::PickBuilding, "pick_building"},
    {KeyAction::DemolishTool, "demolish_tool"},
    {KeyAction::NextBuilding, "next_building"},
    {KeyAction::GroupStore, "group_store"},
    {KeyAction::GroupAdd, "group_add"},
    {KeyAction::GroupRecall, "group_recall"},
    {KeyAction::NextIdleShip, "next_idle_ship"},
    {KeyAction::NextIdleUnit, "next_idle_unit"},
    {KeyAction::NextKindInSelection, "next_kind_in_selection"},
    {KeyAction::AttackMove, "attack_move"},
    {KeyAction::Stop, "stop"},
    {KeyAction::HoldPosition, "hold_position"},
    {KeyAction::Patrol, "patrol"},
    {KeyAction::Formation, "formation"},
    {KeyAction::Scatter, "scatter"},
    {KeyAction::WhiteFlag, "white_flag"},
    {KeyAction::Explore, "explore"},
    {KeyAction::Trade, "trade"},
    {KeyAction::EditRoute, "edit_route"},
    {KeyAction::Delete, "delete"},
    {KeyAction::PixelScaleUp, "pixel_scale_up"},
    {KeyAction::PixelScaleDown, "pixel_scale_down"},
    {KeyAction::ShowOwners, "show_owners"},
    {KeyAction::Quit, "quit"},
};

}  // namespace

void KeyBindings::bind(KeyContext context, KeyChord chord, KeyAction action, std::uint8_t param) {
    std::erase_if(m_bindings, [&](const KeyBinding& b) { return b.context == context && b.chord == chord; });
    m_bindings.push_back(KeyBinding{context, chord, action, param});
}

void KeyBindings::unbind(KeyAction action, std::uint8_t param) {
    std::erase_if(m_bindings, [&](const KeyBinding& b) { return b.action == action && b.param == param; });
}

std::optional<KeyBinding> KeyBindings::find(KeyContext context, const KeyChord& chord) const {
    std::optional<KeyBinding> general;
    for (const KeyBinding& b : m_bindings) {
        if (b.chord != chord) {
            continue;
        }
        if (b.context == context) {
            return b;
        }
        if (b.context == KeyContext::General) {
            general = b;
        }
    }
    return general;
}

std::vector<KeyChord> KeyBindings::keys(KeyAction action, std::uint8_t param) const {
    std::vector<KeyChord> out;
    for (const KeyBinding& b : m_bindings) {
        if (b.action == action && b.param == param) {
            out.push_back(b.chord);
        }
    }
    return out;
}

std::string_view KeyBindings::name(KeyAction action) noexcept {
    for (const auto& [a, n] : kNames) {
        if (a == action) {
            return n;
        }
    }
    return {};
}

std::optional<KeyAction> KeyBindings::action_named(std::string_view name) noexcept {
    for (const auto& [a, n] : kNames) {
        if (n == name) {
            return a;
        }
    }
    return std::nullopt;
}

}  // namespace o1602::ui
