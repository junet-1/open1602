// SPDX-License-Identifier: GPL-3.0-or-later
//
// Growth of fields, plantations, trees, meadows and fish shoals (docs/re/growth.md).
//
// Every crop is three consecutive records of haeuser.cod: growing (HAUS_PRODTYP Kind ROHSTWACHS),
// ripe (ROHSTOFF, with the ware a worker harvests) and, for fields, withered. A growing tile has
// a growth entry; 32 global clocks, one per growth class, tick every 40 s + 7 s * class, and each
// tick moves the entries of that class one stage on (the tile's animation phase shows the stage).
// After AnimAnz stages the tile becomes its ripe record. Harvesting turns it back into the
// growing record with a new entry (0x0047CA80, 0x0047C830).
#pragma once

#include <cstdint>

#include "o1602/assets/building_table.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

/// HAUS_PRODTYP Kind values of crops.
inline constexpr std::int32_t kProdKindRipe = 9;      ///< ROHSTOFF: ripe, can be harvested
inline constexpr std::int32_t kProdKindGrowing = 10;  ///< ROHSTWACHS: growing (or withered)

/// Duration of one growth stage of class `growth_class`.
[[nodiscard]] constexpr std::uint32_t growth_period_ms(std::uint32_t growth_class) noexcept {
    return 40'000U + 7'000U * growth_class;
}

/// The ripe record that follows a growing one, nullptr if `growing` is no crop.
[[nodiscard]] const assets::BuildingType* ripe_record(const assets::BuildingTable& buildings,
                                                      const assets::BuildingType& growing) noexcept;
/// The growing record before a ripe one, nullptr if `ripe` is no crop.
[[nodiscard]] const assets::BuildingType* growing_record(const assets::BuildingTable& buildings,
                                                         const assets::BuildingType& ripe) noexcept;
/// The ware a worker gets from a tile of this record once it is ripe (0 if none): the ware of a
/// ripe record, or of the ripe record that follows a growing one.
[[nodiscard]] std::uint8_t crop_ware(const assets::BuildingTable& buildings, const assets::BuildingType& b) noexcept;

/// Advances the growth clocks by `dt_ms` and grows the entries of every class that ticked.
/// Returns the number of tiles that became ripe.
std::size_t advance_growth(World& world, const assets::BuildingTable& buildings, std::uint32_t dt_ms);

/// Adds an entry for a growing tile without one (stage from its animation phase). Open1602 gives
/// every growing tile an entry; the original's list could overflow and leave tiles that never
/// grow (S-020).
void add_missing_growth(World& world, const assets::BuildingTable& buildings);

/// Harvests the ripe tile (x, y) of island `island` if it carries `ware`: it becomes the growing
/// record again and starts growing. False if there is nothing to harvest.
bool harvest_tile(World& world, const assets::BuildingTable& buildings, std::uint16_t island, std::uint16_t x,
                  std::uint16_t y, std::uint8_t ware);

}  // namespace o1602::sim
