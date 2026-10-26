// SPDX-License-Identifier: GPL-3.0-or-later
//
// Readable names of the original's buildings and figures, Open1602's own: a content package may
// write ["anno1602:bakery"] instead of ["anno1602:20503"] (docs/modding/values.md). The buildings
// are those of the build menu, named after Text.cod; the figures are named after figuren.cod's
// symbols.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace o1602::assets {

/// A name for `count` numbers from `first`: one building's Id, or a figure in each of the four
/// player colours (SOLDAT1 to SOLDAT4 are "swordsman").
struct OriginalName {
    std::string_view name;
    std::uint32_t first = 0;
    std::uint32_t count = 1;
};

/// The names of haeuser.cod's buildings, by HAUS Id.
[[nodiscard]] std::span<const OriginalName> original_building_names() noexcept;
/// The names of figuren.cod's figures, by FIGUR number.
[[nodiscard]] std::span<const OriginalName> original_figure_names() noexcept;
/// The entry of `names` called `name`, nullptr if none is.
[[nodiscard]] const OriginalName* find_original_name(std::span<const OriginalName> names,
                                                     std::string_view name) noexcept;

}  // namespace o1602::assets
