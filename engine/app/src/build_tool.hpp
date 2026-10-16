// SPDX-License-Identifier: GPL-3.0-or-later
//
// Building and demolishing with the mouse until the original's build menu exists: picks a
// building, turns it, and turns a click or a drag into a build or demolish order of the local
// player (docs/re/construction.md). Roads and walls are dragged along a line, fields and squares
// over a rectangle, everything else is placed where the mouse is let go.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/sim/construction.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::app {

class BuildTool {
public:
    enum class Mode : std::uint8_t { Off, Build, Demolish };

    explicit BuildTool(const assets::BuildingTable& buildings);

    [[nodiscard]] Mode mode() const noexcept { return m_mode; }
    void set_mode(Mode mode) noexcept;
    [[nodiscard]] bool active() const noexcept { return m_mode != Mode::Off; }

    /// Next or previous building of the list.
    void cycle(int step) noexcept;
    /// Turns the building by a quarter.
    void rotate(int step) noexcept;

    /// Where the mouse is (island tile), nullopt over the sea.
    struct Spot {
        std::uint16_t island = 0;
        std::int32_t x = 0;
        std::int32_t y = 0;
    };
    void press(const Spot& spot) noexcept;
    void release() noexcept { m_drag_start.reset(); }
    [[nodiscard]] bool dragging() const noexcept { return m_drag_start.has_value(); }

    [[nodiscard]] const assets::BuildingType* selected() const noexcept;

    /// The order the current drag (or a click at `spot`) would give.
    [[nodiscard]] std::optional<sim::BuildOrder> build_order(const sim::World& world, sim::PlayerId player,
                                                             const Spot& spot) const;
    [[nodiscard]] std::optional<sim::DemolishOrder> demolish_order(sim::PlayerId player, const Spot& spot) const;

    /// One line for the window title.
    [[nodiscard]] std::string describe() const;

private:
    const assets::BuildingTable* m_buildings;
    std::vector<const assets::BuildingType*> m_list;
    std::size_t m_selected = 0;
    std::uint8_t m_rotation = 0;
    Mode m_mode = Mode::Off;
    std::optional<Spot> m_drag_start;
};

}  // namespace o1602::app
