// SPDX-License-Identifier: GPL-3.0-or-later
//
// Building and demolishing (docs/re/construction.md).
//
// A build order is a list of buildings a player wants to place on one island, in the order of the
// drag (a road, a field area) or a single building. Like the original (0x004084D0), the whole
// order belongs to the city under its first item; items are checked one after the other, costs add
// up in that order, and once the money or a material runs out that item and all later ones stay
// unbuilt. Unlike the original, every peer checks the order itself (the original sent the result
// and trusted it, M-007), the unlocks are checked here and not only in the menu (S-014), and a building
// must lie completely in its city (S-008).
//
// Placing a building registers it with the lists of the simulation (production, houses, growth,
// shipyards and castles), claims territory for markets and Kontors, founds a city with the first
// Kontor on an island and pays the costs. Demolishing restores the ground, without refunds in the
// original and with the refund the game settings ask for in Open1602 (Rules::demolish_refund), and
// may shrink the territory: buildings that lose it change city or fall to ruins (0x00482120).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/sim/pathfinding.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

/// Why an item of a build order is not built.
enum class BuildRefusal : std::uint8_t {
    None,
    UnknownBuilding,  ///< no such building type
    OutsideIsland,    ///< the footprint leaves the island
    Terrain,          ///< a tile does not take this building (0x00464660)
    Territory,        ///< not completely inside the player's city
    ForeignCity,      ///< the first item lies in another player's city
    CityLimit,        ///< the player has a city on the island already, or the island has 7
    SecondKontor,     ///< a city has one Kontor (larger ones replace it)
    NoCoast,          ///< Kontor, shipyard: no coast in the right place
    NoDeposit,        ///< mine: no ore of its kind in the mountain
    NoRock,           ///< mine: not exactly one rock face next to it
    NoRiver,          ///< water mill: no river in the right place
    UnitInTheWay,     ///< a soldier stands on the footprint (S-015)
    Locked,           ///< the player has not unlocked it (S-014)
    Limit,            ///< castle, cathedral, monument, arch: no more allowed
    Infertile,        ///< the crop does not grow here (S-003)
    Overlap,          ///< an earlier item of the order covers a tile
    Money,            ///< not enough gold for this and the earlier items
    Tools,
    Wood,
    Bricks,
    Cannons,
    ShipTooFar,  ///< founding from a ship: the ship is more than 16 tiles away
};

[[nodiscard]] const char* refusal_name(BuildRefusal refusal) noexcept;

/// Costs of buildings: goods in 1/32 t, gold.
struct BuildBill {
    std::uint32_t tools = 0;
    std::uint32_t wood = 0;
    std::uint32_t bricks = 0;
    std::uint32_t cannons = 0;
    std::int64_t money = 0;

    void add(const assets::BuildingType::BuildCost& c) noexcept {
        tools += c.tools;
        wood += c.wood;
        bricks += c.bricks;
        cannons += c.cannons;
        money += c.money;
    }
};

/// One building of an order. `building` is the Id of the record, already the variant the player
/// picked (the original rolls the variant and the animation phase in the interface).
struct BuildItem {
    std::uint32_t building = 0;
    std::int32_t x = 0;  ///< footprint origin (smallest corner) on the island
    std::int32_t y = 0;
    std::uint8_t rotation = 0;
    std::uint8_t anim_phase = 0;
};

struct BuildOrder {
    PlayerId player = kNoPlayer;
    std::uint16_t island = 0;  ///< index into World::islands()
    std::vector<BuildItem> items;
    /// Founding from a ship (index into World::ships()): its cargo pays the goods.
    std::optional<std::size_t> ship;
    /// Turns buildings with a door towards the nearest road within 4 tiles (the original's
    /// "Automatische Ausrichtung zur Straße").
    bool orient_to_road = true;
};

/// What becomes of one item.
struct PlannedBuild {
    BuildRefusal refusal = BuildRefusal::None;
    const assets::BuildingType* type = nullptr;  ///< final record (palm, upgrade...)
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint8_t rotation = 0;
    std::uint8_t anim_phase = 0;
    bool upgrade = false;  ///< replaces a building of the same family in place

    [[nodiscard]] bool ok() const noexcept { return refusal == BuildRefusal::None; }
};

struct BuildPlan {
    std::vector<PlannedBuild> items;
    BuildBill bill;                        ///< costs of the items that are built
    std::uint8_t city = MapCell::kNoCity;  ///< city of the order; kNoCity founds one
    std::size_t placed = 0;                ///< items that are built
};

/// Checks an order without changing anything (the preview of the build tool).
[[nodiscard]] BuildPlan plan_build(const World& world, const assets::BuildingTable& buildings, const BuildOrder& order);

/// What the construction changed, for the logistics, the renderer and the messages.
struct ConstructionEvent {
    enum class Kind : std::uint8_t {
        Placed,          ///< a building was placed (x, y origin)
        Removed,         ///< a building was removed; `production` is its former production entry
        CityFounded,     ///< a city was founded (city)
        CityDissolved,   ///< a city lost its Kontor and was dissolved
        StorageChanged,  ///< markets or Kontors of the city changed: warehouse room
    };
    Kind kind = Kind::Placed;
    std::uint16_t island = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint8_t city = MapCell::kNoCity;
    std::optional<std::uint32_t> production;
    const assets::BuildingType* type = nullptr;
};

/// Builds what plan_build() accepts and pays for it. New cities take the next name of
/// `city_names` (round robin, World::city_name_index()).
BuildPlan execute_build(World& world, const assets::BuildingTable& buildings, const BuildOrder& order,
                        const std::vector<std::string>& city_names, std::vector<ConstructionEvent>* events);

/// Demolishing a rectangle (the corners in any order).
struct DemolishOrder {
    PlayerId player = kNoPlayer;
    std::uint16_t island = 0;
    TilePos from;
    TilePos to;
};

/// Origins of the buildings an order removes (0x00464B40): only the player's own, no Kontor while
/// its city has other buildings; a rectangle larger than one tile leaves real buildings (houses,
/// workshops, towers, mines, Kontors, shipyards, mills) standing and only clears roads, walls,
/// fields, trees and ruins.
[[nodiscard]] std::vector<TilePos> plan_demolish(const World& world, const assets::BuildingTable& buildings,
                                                 const DemolishOrder& order);

struct DemolishResult {
    std::size_t removed = 0;  ///< buildings removed
    BuildBill refund;         ///< what the player got back (Rules::demolish_refund)
};

/// Removes them and restores the ground (forest becomes meadow). The original gives nothing back;
/// with `refund_percent` > 0 the player gets that share of each building's costs: the gold, and
/// the goods into the warehouse of the building's city as far as there is room.
DemolishResult execute_demolish(World& world, const assets::BuildingTable& buildings, const DemolishOrder& order,
                                std::vector<ConstructionEvent>* events, std::uint32_t refund_percent = 0);

/// Whether crops of `ware` grow on the tile (0x004684A0): the island's fertility for the ware
/// (0, 64 or 128), weighted with the Randwachs of the ground, picks one of five patterns.
[[nodiscard]] bool tile_fertile(const World& world, const assets::BuildingTable& buildings, std::uint16_t island,
                                std::uint8_t ware, std::int32_t x, std::int32_t y);

/// How fertile an island is for a ware (0x0046AFF0): crops and resources 45..58 from the
/// fertility mask (128) or the climate's own crops (64); ore by its deposits (big 128, small 64).
[[nodiscard]] std::uint32_t island_fertility(const WorldIsland& island, std::uint8_t ware) noexcept;

}  // namespace o1602::sim
