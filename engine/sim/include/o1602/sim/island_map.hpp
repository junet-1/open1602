// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/core/result.hpp"
#include "o1602/sim/player.hpp"

namespace o1602::sim {

/// One tile of an island. Mirrors the original grid word (island + 0xAF4, 32 bit per tile):
/// bits 0-12 base sprite, 13-14 rotation, 15-18 animation phase; plus the owner (the original's
/// INSELHAUS attribute bits 22-25, translated to a PlayerId when loading).
struct MapCell {
    static constexpr std::uint16_t kEmpty = 0xFFFF;
    static constexpr std::uint8_t kNoCity = 0xFF;

    std::uint16_t sprite = kEmpty;  ///< base sprite = building gfx + index of the sub-tile
    std::uint8_t rotation = 0;      ///< 0..3
    std::uint8_t anim_phase = 0;    ///< 0..15, per-tile animation offset
    PlayerId owner = kNoPlayer;     ///< index into World::players()
    std::uint8_t city = kNoCity;    ///< city on this island (World::city()), attribute bits 14-16

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
    [[nodiscard]] bool contains(std::int32_t x, std::int32_t y) const noexcept {
        return x >= 0 && y >= 0 && static_cast<std::uint32_t>(x) < m_width && static_cast<std::uint32_t>(y) < m_height;
    }

    /// Changes the picture of a tile in place (a field ripening or harvested: the original swaps
    /// the sprite bits of the grid word, 0x0047CA80 / 0x0047C830).
    void set_sprite(std::uint32_t x, std::uint32_t y, std::uint16_t sprite) { m_top[index(x, y)].sprite = sprite; }
    void set_rotation(std::uint32_t x, std::uint32_t y, std::uint8_t rotation) {
        m_top[index(x, y)].rotation = static_cast<std::uint8_t>(rotation & 3U);
    }
    void set_anim_phase(std::uint32_t x, std::uint32_t y, std::uint8_t phase) {
        m_top[index(x, y)].anim_phase = static_cast<std::uint8_t>(phase & 0x0FU);
    }

    /// Places a building with its top-left footprint corner at (x, y), exactly like the original
    /// placement routine (0x00463B10): sub-tiles receive gfx, gfx+1, ... in a rotation dependent
    /// order. Previous content of the covered tiles moves to the ground layer (demolishing restores
    /// it from there, so former river tiles stay water - S-009).
    /// Fails (without changes) if the rotated footprint leaves the island.
    core::Status place(const assets::BuildingType& building, std::int32_t x, std::int32_t y, std::uint8_t rotation,
                       std::uint8_t anim_phase, PlayerId owner = kNoPlayer, std::uint8_t city = MapCell::kNoCity);

    /// A building the player builds (0x00463B10): like place(), but the ground layer stays as it is
    /// (it keeps the island's original terrain, which demolishing restores).
    core::Status build(const assets::BuildingType& building, std::int32_t x, std::int32_t y, std::uint8_t rotation,
                       std::uint8_t anim_phase, PlayerId owner, std::uint8_t city);

    /// Takes the current picture as the original terrain of every tile (the ground layer the
    /// original keeps at island + 0xAFC).
    void capture_ground() { m_ground = m_top; }

    /// Demolishing: the tile shows its ground again (sprite, rotation, phase); city and owner stay.
    void restore_ground(std::uint32_t x, std::uint32_t y) {
        MapCell& t = m_top[index(x, y)];
        const MapCell& g = m_ground[index(x, y)];
        t.sprite = g.sprite;
        t.rotation = g.rotation;
        t.anim_phase = g.anim_phase;
    }

    /// Territory: the city (and its owner) a tile belongs to.
    void set_city(std::uint32_t x, std::uint32_t y, std::uint8_t city, PlayerId owner) {
        m_top[index(x, y)].city = city;
        m_top[index(x, y)].owner = owner;
    }

    /// Replaces the building whose footprint lies at (x, y) by another one of the same size in
    /// place (a house changing class, ruins): owner, city and the ground below stay (0x00463EF0).
    core::Status replace(const assets::BuildingType& building, std::int32_t x, std::int32_t y, std::uint8_t rotation,
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
    enum class Mode : std::uint8_t { Place, Replace, Build };
    core::Status write(const assets::BuildingType& building, std::int32_t x, std::int32_t y, std::uint8_t rotation,
                       std::uint8_t anim_phase, PlayerId owner, std::uint8_t city, Mode mode);
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
