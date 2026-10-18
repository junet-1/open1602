// SPDX-License-Identifier: GPL-3.0-or-later
//
// The build menu (docs/re/ui.md#baumenü): eight category buttons in BAU.GAD, and per category a
// pop-up of building icons in rows that the code builds itself (BuildMenu_OnCategoryPress
// 0x00432A70). Which buildings belong to which category is a table in the original's code, not
// data; here it is data (BuildMenuDefinition), with the original's lists as the default, so that
// content packs can add buildings and categories (ADR 0007).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/gad.hpp"

namespace o1602::ui {

/// One category: its button in BAU.GAD, the Text.cod section that names its buildings (by their
/// position in the list), the TOOLS sprite of its first icon (as an offset from GFXBAUHAUS; the
/// icons follow one after the other) and its buildings (HAUS Ids) in the pop-up's rows.
struct BuildCategory {
    std::int32_t button = 0;
    bool demolish = false;  ///< no list: the demolish tool
    std::string names;
    std::int32_t icon_offset = 0;
    std::vector<std::vector<std::uint32_t>> rows;

    /// Number of buildings, and the building at a position of the whole list.
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> at(std::size_t index) const noexcept;
    /// Position of a building in the list.
    [[nodiscard]] std::optional<std::size_t> index_of(std::uint32_t id) const noexcept;
};

struct BuildMenuDefinition {
    std::vector<BuildCategory> categories;
    std::int32_t default_category = 31006;  ///< selected at the start of a game (Farmen, 0x0042BAD0)
    std::int32_t locked_icon_offset = 43;   ///< the grey "?" of a building not yet available

    [[nodiscard]] const BuildCategory* category(std::int32_t button) const noexcept;
    /// The category a building is listed in, nothing if none.
    [[nodiscard]] const BuildCategory* category_of(std::uint32_t id) const noexcept;
};

/// The original's categories (lists 0x00498440.., row sizes 0x004985A0..).
[[nodiscard]] const BuildMenuDefinition& original_build_menu();

// Gadget Ids of the pop-up (the original's numbers).
inline constexpr std::int32_t kPopupSlot = 32001;           ///< + position in the list: the frame to click
inline constexpr std::int32_t kPopupIcon = 32101;           ///< + position: the icon
inline constexpr std::int32_t kPopupBackdrop = 1000000000;  ///< the rest of the panel: closes the pop-up
/// Distance of two rows and of two icons in a row, in design units.
inline constexpr std::int32_t kPopupRowStep = 59;
inline constexpr std::int32_t kPopupIconStep = 57;

/// The pop-up of a category as a layout of the side panel. Only rows with an available building
/// are shown (if there is none, the last row, all "?"), the first row next to the category button
/// and the others above it. `available[i]`: whether the building at position i may be picked.
/// `gfx_bauhaus` is TOOLS.INC's GFXBAUHAUS (the frame pieces are its first five sprites).
[[nodiscard]] assets::GadgetLayout build_popup(const BuildCategory& category, const std::vector<bool>& available,
                                               const assets::Gadget& button, std::int32_t gfx_bauhaus,
                                               std::int32_t locked_icon_offset);

}  // namespace o1602::ui
