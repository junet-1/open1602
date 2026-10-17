// SPDX-License-Identifier: GPL-3.0-or-later
//
// The computer players and the pirates' nest building (docs/re/ai.md).
//
// The original runs its AI only on the host and sends what it does over the network (AI_Tick
// 0x0042B4B0); its pace follows the frame rate, it draws from the shared C library rand() and seeds
// the island search with the clock (S-095). Open1602 runs the AI on every computer as part of the
// simulation: at a fixed pace, with its own random numbers and through the same build, ship and
// soldier orders a player gives. Its state is saved with the game; for games of the original it is
// rebuilt from the world as the original does after every load.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "o1602/core/byte_reader.hpp"
#include "o1602/core/byte_writer.hpp"
#include "o1602/sim/player.hpp"

namespace o1602::sim {

class Simulation;

namespace ai {
struct State;
class Engine;
}  // namespace ai

class ComputerPlayers {
public:
    explicit ComputerPlayers(Simulation& simulation);
    ~ComputerPlayers();
    ComputerPlayers(const ComputerPlayers&) = delete;
    ComputerPlayers& operator=(const ComputerPlayers&) = delete;

    /// One tick of game time (Simulation::kTickMs).
    void tick(std::uint32_t dt_ms);

    /// Rebuilds the state of every computer player from the world (AI_Init 0x0042B710).
    void reset();

    /// A ship a computer player or the pirates direct themselves (a nest founding): the pirates'
    /// own wandering leaves it alone.
    [[nodiscard]] bool directs(std::uint32_t ship) const;

    void save(core::ByteWriter& out) const;
    bool load(core::ByteReader& in);

    /// What a computer player is doing, for tests and the debug display.
    struct View {
        PlayerId player = kNoPlayer;
        std::uint32_t cities = 0;
        std::uint8_t max_stage = 0;
        std::uint8_t score = 0;
        bool settling = false;
        bool settle_failed = false;
        std::vector<std::uint8_t> stack;
        struct City {
            std::uint16_t island = 0;
            std::uint8_t city = 0;
            std::uint8_t stage = 0;
            std::uint32_t flags = 0;
            /// Per building slot: count, attempts, failure level.
            std::vector<std::array<std::uint16_t, 3>> slots;
        };
        std::vector<City> plans;
    };
    [[nodiscard]] std::vector<View> view() const;

private:
    std::unique_ptr<ai::State> m_state;
    std::unique_ptr<ai::Engine> m_engine;
};

}  // namespace o1602::sim
