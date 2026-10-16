// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "o1602/assets/cod_parser.hpp"
#include "o1602/core/result.hpp"

namespace o1602::assets {

class GameDirectory;

/// Typed view of one HAUS record from haeuser.cod (buildings and terrain).
/// Field meanings follow the original record layout (1602.exe g_haeuser @ 0x00609160, 0x88 bytes);
/// see docs/re/haus-record.md. Only fields with known meaning are exposed.
struct BuildingType {
    std::uint32_t id = 0;        ///< Id (20000..23500); island tiles store id - 20000
    std::int32_t kind = 0;       ///< Kind (BODEN=11, MEER=19, GEBAEUDE=14, ...)
    std::int32_t prod_kind = 0;  ///< Kind of the nested HAUS_PRODTYP block (selects animation logic)
    std::uint32_t gfx = 0;       ///< first sprite index in STADTFLD.BSH
    std::uint32_t size_x = 1;    ///< footprint in tiles (unrotated)
    std::uint32_t size_y = 1;
    std::uint32_t rotate = 0;      ///< sprite distance between two rotation steps (0 = not rotatable)
    std::uint32_t anim_count = 0;  ///< AnimAnz: animation frames (animated if > 1)
    std::uint32_t anim_add = 0;    ///< AnimAdd: sprite distance between two frames
    std::int32_t anim_time = 0;    ///< AnimTime: frame duration (TIMENEVER = 0 = static)
    std::uint32_t anim_frame = 0;  ///< AnimFrame: initial global frame
    std::int32_t posoffs = 0;      ///< Posoffs: elevation in pixels at full zoom (land 20, sea 0)
    bool high = false;             ///< Highflg: drawn in the second (high) pass of a row
    bool draw_ground = false;      ///< Grundflg: draw the ground tile below first
    std::uint32_t build_gfx = 0;   ///< Baugfx: icon in the build menu
    std::int32_t kreuz_base = 0;   ///< KreuzBase: related base record (e.g. road crossings)
    std::uint16_t rand_count = 0;  ///< RandAnz: number of random variants
    std::uint16_t rand_add = 0;    ///< RandAdd: sprite distance between variants
    /// HAUS_PRODTYP/Rauchfignr: figures shown on the building (record +0x41): smoke of workshops,
    /// flags of warehouses (KONTOR) and markets (MARKT), the market jugglers. 0 = none.
    std::array<std::uint16_t, 2> attached_figures{};
    /// HAUS_PRODTYP/Maxlager: goods the building holds until a carrier collects them, in 1/32 t
    /// (the original multiplies the cod value by 32 when loading, record +0x30).
    std::uint32_t max_stock = 0;
    /// HAUS_PRODTYP/Maxnorohst: cycles without raw material before the building reports a shortage
    /// (record +0x36).
    std::uint32_t max_idle_cycles = 0;
    /// HAUS_PRODTYP/LagAniFlg (record +0x47 bit 2): the animation frame shows how full the
    /// building's store is instead of running (forester, stonemason).
    bool stock_animation = false;

    /// Number of base sprites (one per covered tile).
    [[nodiscard]] std::uint32_t tile_count() const noexcept { return size_x * size_y; }
};

/// All building types of haeuser.cod with lookups by id and by sprite index
/// (the original keeps the latter in a table at 0x00619B60).
class BuildingTable {
public:
    static constexpr std::uint32_t kIdOffset = 20000;

    /// Builds the table from an interpreted haeuser.cod (records with Id 0 are unused slots).
    static core::Result<BuildingTable> from_cod(const cod::Document& haeuser);
    /// Reads and interprets haeuser.cod of an installation.
    static core::Result<BuildingTable> load(const GameDirectory& game);

    [[nodiscard]] const std::vector<BuildingType>& types() const noexcept { return m_types; }
    [[nodiscard]] const BuildingType* by_id(std::uint32_t id) const noexcept;
    /// Building whose base sprite range [gfx, gfx + tile_count) contains `sprite`.
    [[nodiscard]] const BuildingType* by_sprite(std::uint32_t sprite) const noexcept;

    /// Record drawn for open sea: the first record with Kind MEER (19) and production kind 9,
    /// as chosen by the original after loading (0x00462D50 -> g_seaHaus). Id 21201 in the original data.
    [[nodiscard]] const BuildingType* open_sea() const noexcept {
        return m_open_sea < 0 ? nullptr : &m_types[static_cast<std::size_t>(m_open_sea)];
    }

    /// Index of a type in types() (stable; used for per-type state such as animation clocks).
    [[nodiscard]] std::size_t index_of(const BuildingType& type) const noexcept {
        return static_cast<std::size_t>(&type - m_types.data());
    }

private:
    std::vector<BuildingType> m_types;
    std::vector<std::int32_t> m_by_id;      ///< index by (id - kIdOffset), -1 = none
    std::vector<std::int32_t> m_by_sprite;  ///< index by sprite, -1 = none
    std::int32_t m_open_sea = -1;           ///< index (not a pointer, so copies of the table stay valid)
};

}  // namespace o1602::assets
