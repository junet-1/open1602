// SPDX-License-Identifier: GPL-3.0-or-later
//
// State of the production buildings (workshops, farms, mines, hunters, fishers, markets). Until the
// economy simulation exists the state comes from the savegame (PRODLIST2) and stays as loaded; the
// renderer already uses it for building animations, smoke and status icons (docs/re/production.md).
#pragma once

#include <cstdint>

#include "o1602/assets/building_table.hpp"

namespace o1602::sim {

/// HAUS_PRODTYP Kind values with production state (the original keeps a PRODLIST entry for them).
enum class ProductionKind : std::int32_t {
    Handwerk = 1,   ///< workshops (bakery, smelter, ...)
    Plantage = 2,   ///< farms and plantations
    Bergwerk = 3,   ///< mines
    Weidetier = 4,  ///< cattle and sheep farms
    Jagdhaus = 5,   ///< hunters
    Fischerei = 6,  ///< fishers
    Markt = 7,      ///< market place
};

/// Workshops, farms, mines, hunters and fishers: kinds 1..6. These have smoke and status icons.
[[nodiscard]] constexpr bool is_producer(const assets::BuildingType& b) noexcept {
    return b.prod_kind >= static_cast<std::int32_t>(ProductionKind::Handwerk) &&
           b.prod_kind <= static_cast<std::int32_t>(ProductionKind::Fischerei);
}

struct Production {
    std::uint16_t island = 0;  ///< index into World::islands()
    std::uint16_t x = 0;       ///< origin tile of the building on the island
    std::uint16_t y = 0;
    bool enabled = true;           ///< switched on by the player
    bool working = false;          ///< has raw material and is producing
    std::uint8_t anim_phase = 0;   ///< animation offset of the building (0..15)
    std::uint8_t idle_cycles = 0;  ///< cycles without raw material (0..15)
    std::uint16_t stock = 0;       ///< produced goods waiting for a carrier, 1/32 t
    std::uint16_t progress = 0;    ///< progress of the current cycle, kProgressDone = finished

    static constexpr std::uint16_t kProgressDone = 512;
};

/// What the icon above a building of the local player shows (0x0047CDD0).
enum class ProductionStatus : std::uint8_t {
    Ok,          ///< no icon
    Disabled,    ///< switched off by the player
    NoRawGoods,  ///< idle for at least Maxnorohst cycles
};

[[nodiscard]] constexpr ProductionStatus production_status(const Production& p,
                                                           const assets::BuildingType& b) noexcept {
    if (!p.enabled) {
        return ProductionStatus::Disabled;
    }
    return p.idle_cycles >= b.max_idle_cycles ? ProductionStatus::NoRawGoods : ProductionStatus::Ok;
}

/// Smoke rises from the chimney while the building works (0x0047CF30).
[[nodiscard]] constexpr bool smoke_visible(const Production& p) noexcept {
    return p.enabled && p.working;
}

}  // namespace o1602::sim
