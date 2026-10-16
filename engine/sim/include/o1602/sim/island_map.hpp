// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/core/result.hpp"

namespace o1602::sim {

/// One tile of an island. Mirrors the original grid word (island + 0xAF4, 32 bit per tile):
/// bits 0-12 base sprite, 13-14 rotation, 15-18 animation phase.
struct MapCell {
    static constexpr std::uint16_t kEmpty = 0xFFFF;

    std::uint16_t sprite = kEmpty;  ///< base sprite = building gfx + index of the sub-tile
    std::uint8_t rotation = 0;      ///< 0..3
    std::uint8_t anim_phase = 0;    ///< 0..15, per-tile animation offset

    [[nodiscard]] bool empty() const noexcept { return sprite == kEmpty; }
};

/// Position of a tile relative to the building it belongs to.
struct SubTile {
    const assets::BuildingType* building = nullptr;
    std::int32_t origin_x = 0;  ///< tile where the building was placed (top-left of its footprint)
    std::int32_t origin_y = 0;
};

/// Tile grid of one island with two layers: buildings/terrain on top and the ground below
/// (the original keeps the latter in a second grid at island + 0xAFC).
class IslandMap {
public:
    IslandMap(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] std::uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] std::uint32_t height() const noexcept { return m_height; }
    [[nodiscard]] const MapCell& top(std::uint32_t x, std::uint32_t y) const { return m_top[index(x, y)]; }
    [[nodiscard]] const MapCell& ground(std::uint32_t x, std::uint32_t y) const { return m_ground[index(x, y)]; }

    /// Places a building with its top-left footprint corner at (x, y), exactly like the original
    /// placement routine (0x00463B10): sub-tiles receive gfx, gfx+1, ... in a rotation dependent
    /// order. Previous content of the covered tiles moves to the ground layer (demolishing restores
    /// it from there, so former river tiles stay water - S-009).
    /// Fails (without changes) if the rotated footprint leaves the island.
    core::Status place(const assets::BuildingType& building, std::int32_t x, std::int32_t y, std::uint8_t rotation,
                       std::uint8_t anim_phase);

    /// Inverse of place() for one tile (original 0x00463250): building and placement origin.
    [[nodiscard]] SubTile sub_tile(const assets::BuildingTable& buildings, std::uint32_t x, std::uint32_t y) const;

    /// Builds the map from INSELHAUS entries (one entry per building at its origin).
    /// If `base` is given (the island template, see IslandHeader::template_path()), its tiles are
    /// placed first and the island's own entries on top - as the original loader does.
    /// Unknown ids and entries outside the island are skipped and reported in `warnings`.
    static IslandMap from_island(const assets::Island& island, const assets::BuildingTable& buildings,
                                 std::vector<std::string>* warnings = nullptr, const assets::Island* base = nullptr);

private:
    [[nodiscard]] std::size_t index(std::uint32_t x, std::uint32_t y) const { return std::size_t{y} * m_width + x; }

    std::uint32_t m_width;
    std::uint32_t m_height;
    std::vector<MapCell> m_top;
    std::vector<MapCell> m_ground;
};

/// Footprint after rotation (original 0x00463830): rotations 1 and 3 swap width and height.
inline void rotated_size(const assets::BuildingType& b, std::uint8_t rotation, std::uint32_t& w, std::uint32_t& h) {
    const bool swap = (rotation & 1U) != 0;
    w = swap ? b.size_y : b.size_x;
    h = swap ? b.size_x : b.size_y;
}

}  // namespace o1602::sim
