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
    std::uint32_t record = 0;    ///< Nummer: position in haeuser.cod (the original's table index)
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
    /// Einhoffs (record +0x18, float): height of the building in half tile widths. Status icons and
    /// other symbols hover at this height above the building's centre (0x00446776).
    double icon_height = 0.0;
    bool high = false;         ///< Highflg: drawn in the second (high) pass of a row
    bool draw_ground = false;  ///< Grundflg: draw the ground tile below first
    /// Tuerflg (record +0x6A bit 0): the door is the middle tile of one edge instead of the centre
    /// tile (0x004634C0); figures leave and enter there.
    bool door_on_edge = false;
    /// Wegspeed (record +0x58): how fast figures walk on this tile, per figure speed type (FIGUR
    /// Speedtyp: 0 carriers, 1 soldiers, 2 carts and riders, 3 other). Stored like the original as
    /// value * 32 / 100 (32 = 100 % = full speed, larger is slower), at most 126 (0x00462834).
    std::array<std::uint8_t, 4> walk_speed{32, 32, 32, 32};
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
    /// HAUS_PRODTYP/Ware, Rohstoff, Workstoff: goods produced and consumed (ware numbers of
    /// haeuser.cod, record +0x21..+0x23; 0 = none).
    std::uint8_t ware = 0;
    std::uint8_t raw_ware = 0;
    std::uint8_t raw2_ware = 0;
    /// HAUS_PRODTYP/Prodmenge, Rohmenge, Workmenge: goods per cycle in 1/32 t (record +0x24,
    /// +0x26, +0x28; the original multiplies by 32 when loading).
    std::uint32_t produce_amount = 0;
    std::uint32_t raw_amount = 0;
    std::uint32_t raw2_amount = 0;
    /// HAUS_PRODTYP/Interval: game seconds per cycle at full rate (record +0x3A).
    std::uint32_t interval = 0;
    /// HAUS_PRODTYP/Figurnr, Figuranz: carriers or workers of the building (record +0x3C, +0x3E).
    std::uint16_t worker_figure = 0;
    std::uint8_t worker_count = 0;
    /// HAUS_PRODTYP/Radius (record +0x20): catchment radius in tiles. Carriers, carts and workers
    /// never leave this circle around the building.
    std::uint8_t radius = 0;
    /// HAUS_PRODTYP/Maxnorohst: cycles without raw material before the building reports a shortage
    /// (record +0x36).
    std::uint32_t max_idle_cycles = 0;
    /// HAUS_PRODTYP/LagAniFlg (record +0x47 bit 2): the animation frame shows how full the
    /// building's store is instead of running (forester, stonemason).
    bool stock_animation = false;
    /// HAUS_PRODTYP/Anicontflg (record +0x47 bit 0): when the building stops, its animation stays
    /// at the current frame instead of jumping back to the first (grain mill, 0x004638C0).
    bool continuous_animation = false;
    /// HAUS_PRODTYP/Doerrflg (record +0x47 bit 3): the withered state of a field or plantation.
    bool withered = false;
    /// HAUS_PRODTYP/Erzbergnr (record +0x3F): 0 deep mine (ERZBERG_GROSS), 1 small mine
    /// (ERZBERG_KLEIN). Only mines with 0 dig on in a big vein below 120 t.
    std::uint8_t ore_mountain = 0;
    /// HAUS_PRODTYP/BGruppe (record +0x2E): class of inhabitants of a house (WOHNUNG), index into
    /// BuildingTable::resident_classes().
    std::uint8_t resident_class = 0;
    /// HAUS_BAUKOST: what building costs (goods in 1/32 t, record +0x4C..+0x54).
    struct BuildCost {
        std::uint32_t tools = 0;    ///< Werkzeug
        std::uint32_t wood = 0;     ///< Holz
        std::uint32_t bricks = 0;   ///< Ziegel
        std::uint32_t cannons = 0;  ///< Kanon
        std::int32_t money = 0;     ///< Money
    };
    BuildCost build_cost;
    /// HAUS_PRODTYP/Kosten: running costs in gold per minute while switched on (first value,
    /// record +0x2C) and switched off (second value, +0x2A).
    std::uint32_t running_cost = 0;
    std::uint32_t idle_cost = 0;
    /// Ruinenr (record +0x66): what remains when the building is destroyed (RUINE_*, 0xFF = nothing).
    std::uint8_t ruin = 0xFF;
    /// Growing records (HAUS_PRODTYP Kind ROHSTWACHS): growth class 0..31. One growth stage lasts
    /// 40 s + 7 s * class; the original converts Interval / AnimAnz into the class after loading
    /// (0x00462F1D).
    std::uint8_t growth_class = 0;

    /// Number of base sprites (one per covered tile).
    [[nodiscard]] std::uint32_t tile_count() const noexcept { return size_x * size_y; }
};

/// A class of inhabitants (BGRUPPE in haeuser.cod: pioneers, settlers, citizens, merchants,
/// aristocrats; any number, ADR 0007). Values are converted like the original (0x00460750) so that
/// the economy keeps its balance (docs/re/consumption.md).
struct ResidentClass {
    /// What the class consumes per inhabitant and 10 s, in 1/8192 t:
    /// trunc(value * 8192) / 600 of `Ware: W, value` (value = t per 100 inhabitants and minute).
    struct Need {
        std::uint8_t ware = 0;
        std::uint32_t amount = 0;
    };
    std::uint32_t share = 0;          ///< Prozent * 128 / 100: supply the class asks for
    std::uint32_t max_per_house = 0;  ///< Maxwohn
    std::uint32_t tax = 0;            ///< trunc(Steuer * 16 / 3)
    std::uint32_t hiscore = 0;        ///< Hiscore
    std::vector<Need> needs;          ///< besides food, in file order
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
    /// Building by its Nummer in haeuser.cod. Crops are three records in a row: growing, ripe
    /// and withered (docs/re/growth.md).
    [[nodiscard]] const BuildingType* by_record(std::uint32_t record) const noexcept;

    /// Record drawn for open sea: the first record with Kind MEER (19) and production kind 9,
    /// as chosen by the original after loading (0x00462D50 -> g_seaHaus). Id 21201 in the original data.
    [[nodiscard]] const BuildingType* open_sea() const noexcept {
        return m_open_sea < 0 ? nullptr : &m_types[static_cast<std::size_t>(m_open_sea)];
    }

    /// Classes of inhabitants by BGRUPPE Nummer.
    [[nodiscard]] const std::vector<ResidentClass>& resident_classes() const noexcept { return m_classes; }
    /// Food every inhabitant eats per 10 s in 1/8192 t (global `Nahrung`: trunc(1.3 * 8192) / 600 =
    /// 17) and its ware (NAHRUNG, 14).
    [[nodiscard]] std::uint32_t food_need() const noexcept { return m_food_need; }
    [[nodiscard]] std::uint8_t food_ware() const noexcept { return m_food_ware; }
    /// Building materials (WERKZEUG, HOLZ, ZIEGEL, KANONEN of haeuser.cod).
    struct Materials {
        std::uint8_t tools = 22;
        std::uint8_t wood = 23;
        std::uint8_t bricks = 24;
        std::uint8_t cannons = 13;
    };
    [[nodiscard]] const Materials& materials() const noexcept { return m_materials; }
    /// First house record of a class of inhabitants (table order; 0x00462D50), nullptr if none.
    [[nodiscard]] const BuildingType* first_house(std::size_t resident_class) const noexcept;
    /// Ruin record for Ruinenr `ruin` (table 0x00619B20), the beach variant on beach ground;
    /// nullptr if the original has no record for it.
    [[nodiscard]] const BuildingType* ruin_record(std::uint8_t ruin, bool beach) const noexcept;

    /// Index of a type in types() (stable; used for per-type state such as animation clocks).
    [[nodiscard]] std::size_t index_of(const BuildingType& type) const noexcept {
        return static_cast<std::size_t>(&type - m_types.data());
    }

private:
    std::vector<BuildingType> m_types;
    std::vector<std::int32_t> m_by_id;      ///< index by (id - kIdOffset), -1 = none
    std::vector<std::int32_t> m_by_sprite;  ///< index by sprite, -1 = none
    std::vector<std::int32_t> m_by_record;  ///< index by Nummer, -1 = none
    std::int32_t m_open_sea = -1;
    std::vector<ResidentClass> m_classes;
    std::uint32_t m_food_need = 0;
    std::uint8_t m_food_ware = 14;
    Materials m_materials;  ///< index (not a pointer, so copies of the table stay valid)
};

}  // namespace o1602::assets
