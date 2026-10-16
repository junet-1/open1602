// SPDX-License-Identifier: GPL-3.0-or-later
//
// Houses: who lives where, and how houses grow, rise to the next class, fall back or are
// abandoned (docs/re/houses.md).
//
// Every house is visited about every 15.5 s (16 timer groups, 15 s + 64 ms * group). Its growth
// rate follows the city's food supply, the satisfaction of its class and the public buildings in
// reach: a fully supplied house grows by 8-12 % per visit, a neglected one shrinks. A full house
// queues to try out the next class; if the people are content, the services of the next class are
// there and the warehouse has the building material, it becomes a house of that class. A house
// that loses its people falls back a class, gives its people to the neighbours or turns into a
// ruin.
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

/// Bits of House::services (house + 8 in the original): public buildings in reach.
namespace house_service {
inline constexpr std::uint16_t kDisasterMask = 0x0003;  ///< 1 plague, 2 fire
inline constexpr std::uint16_t kKapelle = 1U << 2;
inline constexpr std::uint16_t kKirche = 1U << 3;
inline constexpr std::uint16_t kWirtshaus = 1U << 4;
inline constexpr std::uint16_t kSchule = 1U << 5;
inline constexpr std::uint16_t kBadehaus = 1U << 6;
inline constexpr std::uint16_t kTheater = 1U << 7;
inline constexpr std::uint16_t kHochschule = 1U << 8;
inline constexpr std::uint16_t kKlinik = 1U << 9;
inline constexpr std::uint16_t kKontor = 1U << 10;
inline constexpr std::uint16_t kGalgen = 1U << 11;
inline constexpr std::uint16_t kBrunnen = 1U << 12;
}  // namespace house_service

/// Period of house timer group `group`.
[[nodiscard]] constexpr std::uint32_t house_group_period_ms(std::uint32_t group) noexcept {
    return 15'000U + 64U * group;
}

struct HouseEvent {
    enum class Kind : std::uint8_t {
        Upgraded,    ///< became a house of the next class
        Downgraded,  ///< fell back a class
        Abandoned,   ///< nobody lives there any more: ruins
    };
    Kind kind = Kind::Upgraded;
    std::uint16_t island = 0;
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint8_t resident_class = 0;  ///< class after the change
};

/// Whether a house has the public buildings class `resident_class` needs (0x0047BFA0): pioneers a
/// market or Kontor; settlers a market and a chapel or church; citizens also a tavern and a
/// school; merchants a church and a bath house; aristocrats a college and a theatre.
[[nodiscard]] bool has_services(const House& house, std::uint8_t resident_class) noexcept;

/// Recomputes which markets, Kontors and public buildings reach each house of island `island`
/// (0x00482120): buildings of the same city whose Radius covers the house from their centre.
void update_services(World& world, const assets::BuildingTable& buildings, std::uint16_t island);
/// The same for every island.
void update_services(World& world, const assets::BuildingTable& buildings);

/// Growth rate of a house (0x0047B410): positive grows by rate * 6/128 of its inhabitants per
/// visit, negative shrinks by |rate| * 4/128, 0 stays.
[[nodiscard]] std::int32_t growth_rate(const City& city, const House& house) noexcept;

/// Advances the house timer groups by `dt_ms` and visits the houses of every group that ticked.
void advance_houses(World& world, const assets::BuildingTable& buildings, std::uint32_t dt_ms,
                    std::vector<HouseEvent>* events);

}  // namespace o1602::sim
