// SPDX-License-Identifier: GPL-3.0-or-later
//
// Goods on the move between the buildings of a city (docs/re/logistics.md):
//
// - Carriers: a workshop that runs short of raw material sends a carrier (0x0044AB60). He walks
//   over roads and open ground to the nearest building of his city that has the ware - a producer
//   of it, a market or the warehouse - takes up to what he can carry and brings it home.
// - Market carts: every market and warehouse sends carts (up to Figuranz, 0x0044AD50). A cart
//   drives on roads to the fullest producer of the city in its circle and brings the goods to the
//   city's warehouse. Finished goods are always collected; raw material and intermediate goods
//   only from a full store, the rest is left to the carriers of the next workshop.
//
// Goods are reserved when a figure sets out, so two figures never count on the same ton. A
// figure walks only inside the circle of its home building (pathfinding.hpp). Everything runs
// in fixed ticks with integer positions (ADR 0004); figures are not saved, like in the original.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/sim/pathfinding.hpp"
#include "o1602/sim/pool.hpp"
#include "o1602/sim/production_cycle.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

/// A building on an island as the logistics sees it.
struct BuildingAt {
    const assets::BuildingType* type = nullptr;
    TilePos origin;
    std::uint8_t rotation = 0;
    std::uint32_t width = 1;  ///< rotated footprint
    std::uint32_t height = 1;
    std::uint8_t city = MapCell::kNoCity;
};

/// The building covering `tile` of island `island`, if any.
[[nodiscard]] std::optional<BuildingAt> building_at(const World& world, const assets::BuildingTable& buildings,
                                                    std::uint16_t island, TilePos tile);

/// Door of a building (0x004634C0): its centre tile, or with Tuerflg the middle tile of one edge,
/// which edge depending on the rotation.
[[nodiscard]] TilePos door_tile(const BuildingAt& building) noexcept;
/// Centre tile: origin + (size - 1) / 2.
[[nodiscard]] TilePos centre_tile(const BuildingAt& building) noexcept;

enum class WalkerKind : std::uint8_t {
    Carrier,  ///< fetches raw material for a workshop (figure kind 8)
    Cart,     ///< brings goods of producers to the warehouse (figure kind 0x0B)
};

/// A figure on its way.
struct Walker {
    WalkerKind kind = WalkerKind::Carrier;
    std::uint16_t island = 0;             ///< index into World::islands()
    std::uint16_t figure = 0;             ///< FIGUR number (TRAEGER, KARREN, ...)
    std::uint8_t city = 0;                ///< city on the island
    std::uint32_t home = 0;               ///< index into World::productions()
    TilePos home_tile;                    ///< where it delivers: the door (carriers) or the centre (carts)
    std::optional<std::uint32_t> source;  ///< producer it fetches from (index into productions)
    TilePos source_origin;                ///< origin of the building it fetches from (producer or warehouse)
    bool from_warehouse = false;          ///< fetches from a market or warehouse (city stock)
    std::uint8_t ware = 0;
    std::uint16_t reserved = 0;  ///< reserved at the source, not picked up yet
    std::uint16_t load = 0;      ///< carried, 1/32 t
    std::uint16_t expected = 0;  ///< carts: room reserved in the warehouse (City::incoming)
    bool returning = false;

    // Movement: the figure walks from `tile` towards path[step]; `progress` of `length` (units of
    // 1/65536 tile) is done, at `speed` units per tick.
    TilePos tile;
    std::vector<TilePos> path;
    std::uint32_t step = 0;
    std::uint32_t progress = 0;
    std::uint32_t length = 0;
    std::uint32_t speed = 0;
    std::uint8_t direction = 0;
    std::uint8_t walk_speed = 32;  ///< Wegspeed of the tile the step started on (animation pace)

    [[nodiscard]] bool moving() const noexcept { return length != 0; }
    /// ANIM of the figure: 1 walking empty, 0 carrying (TRAEGER, KARREN).
    [[nodiscard]] std::uint8_t anim() const noexcept { return load != 0 || returning ? 0 : 1; }
};

/// Island-tile position of a walker including the part of its step (tile centre = x + 0.5).
struct WalkerPosition {
    double x = 0.0;
    double y = 0.0;
};
[[nodiscard]] WalkerPosition walker_position(const Walker& walker) noexcept;

struct LogisticsStats {
    std::uint64_t carrier_trips = 0;    ///< carriers sent out
    std::uint64_t cart_trips = 0;       ///< carts sent out
    std::uint64_t raw_delivered = 0;    ///< raw material brought to workshops, 1/32 t
    std::uint64_t goods_collected = 0;  ///< goods brought to warehouses, 1/32 t
};

class Logistics {
public:
    /// Units of a step: 1 tile = 65536.
    static constexpr std::uint32_t kTileUnits = 65536;
    /// Length of a diagonal step (sqrt 2 tiles).
    static constexpr std::uint32_t kDiagonalUnits = 92682;
    /// Game time per tick (Simulation::kTickMs).
    static constexpr std::uint32_t kTickMs = 50;
    /// The original's carrier (TRAEGER), sent for the second raw material of non-workshops.
    static constexpr std::uint16_t kCarrierFigure = 80;

    Logistics(World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures);

    /// Recomputes the warehouse room of every city (after loading or building).
    void update_storage();

    /// Reacts to a production event: NeedsRawGoods sends a carrier or a cart.
    void handle(const ProductionEvent& event);

    /// One tick of game time: every figure walks; at the end of its path it picks up or delivers.
    void tick();

    [[nodiscard]] const Pool<Walker>& walkers() const noexcept { return m_walkers; }
    [[nodiscard]] const LogisticsStats& stats() const noexcept { return m_stats; }

    /// Tiles per tick of a figure on a tile of Wegspeed class `walk_speed` (units of 1/65536).
    [[nodiscard]] static std::uint32_t step_speed(const assets::FigureType& figure, std::uint8_t walk_speed) noexcept;

private:
    void send_carrier(std::uint32_t home, std::uint8_t ware, std::uint16_t figure);
    void send_cart(std::uint32_t home);
    /// Marks the tiles of the city's buildings with HAUS_PRODTYP kind 1..last_kind as goals
    /// (inside the current search area).
    void mark_goals(const IslandMap& map, std::uint8_t city, std::int32_t last_kind, std::uint8_t speed_type);
    /// Starts the walker if `path` is not empty; false if there is nothing to walk.
    bool launch(Walker walker);
    void arrive(Handle handle, Walker& walker);
    void pick_up(Walker& walker);
    void deliver(Walker& walker);
    bool plan_home(Walker& walker);
    /// Removes the walker and gives back what it reserved.
    void finish(Handle handle, Walker& walker);
    void begin_step(Walker& walker);

    [[nodiscard]] std::uint8_t& jobs(std::uint32_t production);
    [[nodiscard]] std::optional<BuildingAt> building_of(std::uint32_t production) const;
    /// Reserves up to `max` of `ware` at the building `at` (a producer or a market/warehouse);
    /// nothing if less than `min` is free (0x0047D810).
    std::uint16_t reserve(const BuildingAt& at, std::uint16_t island, std::uint8_t ware, std::uint16_t min,
                          std::uint16_t max, bool& from_warehouse, std::optional<std::uint32_t>& production);

    World* m_world;
    const assets::BuildingTable* m_buildings;
    const assets::FigureTable* m_figures;
    Pool<Walker> m_walkers;
    std::vector<std::uint8_t> m_jobs;  ///< walkers out per production building
    PathFinder m_finder;
    LogisticsStats m_stats;
};

}  // namespace o1602::sim
