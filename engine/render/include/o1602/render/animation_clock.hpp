// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <vector>

#include "o1602/assets/building_table.hpp"

namespace o1602::render {

/// Global animation frame per building type, advanced in real time like the original
/// (0x00468420, called every frame with the GetTickCount delta): an accumulator per type
/// collects milliseconds; when accumulator + dt/2 reaches AnimTime, the frame counter steps by one.
/// Animations are presentation only - they do not influence the simulation (ADR 0004).
/// At most one step per update is kept from the original (U-006).
class AnimationClock {
public:
    explicit AnimationClock(const assets::BuildingTable& buildings);

    void advance(std::uint32_t delta_ms);

    /// Current global frame of a type (to be taken modulo AnimAnz by the caller).
    [[nodiscard]] std::uint32_t frame(const assets::BuildingType& type) const;

    /// Real time advanced so far (figure animations such as flags run on it directly).
    [[nodiscard]] std::uint64_t elapsed_ms() const noexcept { return m_elapsed; }

private:
    const assets::BuildingTable* m_buildings;
    std::vector<std::uint32_t> m_frame;
    std::vector<std::uint32_t> m_accumulator;
    std::uint64_t m_elapsed = 0;
};

}  // namespace o1602::render
