// SPDX-License-Identifier: GPL-3.0-or-later
//
// Life in the picture that the game does not depend on (docs/re/flavor.md): townsfolk walking from
// their houses to taverns, churches, schools and markets, whales, dolphins and krakens in the sea,
// jugglers on the markets of bigger cities, birds in the forests and surf at the beaches.
//
// The original runs these through the simulation: they take slots of its figure list, draw from
// the one rand() of the game and depend on where the player looks, so scrolling changes fires,
// plague and robbers (S-065). In Open1602 they live only here, on the side of the picture: own
// random numbers, nothing saved, nothing sent to other players, no effect on the game. They follow
// game time, so they stop when the game is paused.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/render/figure_animation.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/pathfinding.hpp"
#include "o1602/sim/random.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::render {

/// What the player sees: which islands are on screen, and a world tile for a point of the screen
/// (u, v in 0..1), nothing where the screen shows no map.
struct AmbientView {
    std::vector<bool> islands;  ///< by index into World::islands()
    std::function<std::optional<sim::TilePos>(double u, double v)> tile_at;
    std::uint32_t game_speed = 1;  ///< sounds play only at normal speed, as in the original
};

/// A sound of the scenery to play at a world point (no sound output yet).
struct AmbientSound {
    enum class Kind : std::uint8_t { Birds, Surf };
    Kind kind = Kind::Birds;
    std::uint8_t variant = 0;  ///< WAV_VOGEL1 + variant for birds
    double x = 0.0;
    double y = 0.0;
};

// Figures of figuren.cod.
inline constexpr std::uint16_t kFigureJuggler = 70;   ///< GAUKLER1
inline constexpr std::uint16_t kFigureWhale = 101;    ///< WAL
inline constexpr std::uint16_t kFigureDolphin = 102;  ///< DELPHIN
inline constexpr std::uint16_t kFigureKraken = 103;   ///< KRAKE

/// The townsfolk a walker going to a building of HAUS_PRODTYP Kind `kind` can be (0x004E857C),
/// four choices; FRAU for every other kind.
[[nodiscard]] std::array<std::uint16_t, 4> townsfolk_for(std::int32_t kind) noexcept;

/// HAUS_PRODTYP Kinds a walker may go to, by the roll 0..3 (0x00480370).
[[nodiscard]] std::vector<std::int32_t> walker_targets(std::uint32_t roll);

class Ambient {
public:
    static constexpr std::uint32_t kCityPeriodMs = 10'000;  ///< walkers: one round per city per 10 s
    static constexpr std::uint32_t kSeaPeriodMs = 5'000;    ///< sea creatures: a chance every 5 s

    explicit Ambient(std::uint64_t seed = 1602) noexcept : m_random(seed, 0xA4B1E47ULL) {}

    /// Runs `dt_ms` of game time.
    void update(const sim::World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures,
                const AmbientView& view, std::uint32_t dt_ms);

    /// Townsfolk on the roads (drawn like the carriers, from TRAEGER.BSH).
    [[nodiscard]] const std::vector<FigureInstance>& walkers() const noexcept { return m_walker_instances; }
    /// Sea creatures and jugglers.
    [[nodiscard]] const std::vector<EffectFigure>& effects() const noexcept { return m_effects; }
    /// Sounds due since the last call.
    [[nodiscard]] std::vector<AmbientSound> take_sounds() {
        std::vector<AmbientSound> s;
        s.swap(m_sounds);
        return s;
    }

    struct Stats {
        std::uint64_t walkers = 0;  ///< townsfolk sent out
        std::uint64_t creatures = 0;
    };
    [[nodiscard]] const Stats& stats() const noexcept { return m_stats; }

private:
    struct Walker {
        std::uint16_t island = 0;
        std::uint16_t figure = 0;
        std::uint64_t house = 0;         ///< key of the house it came from
        std::vector<sim::TilePos> path;  ///< island tiles, from the house to the target
        double position = 0.0;           ///< steps done along `path`
        double tiles_per_ms = 0.0;
        AnimState anim;
    };
    struct Creature {
        std::uint16_t figure = 0;
        double x = 0.0;
        double y = 0.0;
        double vx = 0.0;  ///< tiles per ms (whales only)
        double vy = 0.0;
        std::uint8_t direction = 0;
        std::uint32_t life_ms = 0;
        AnimState anim;
    };

    void city_walkers(const sim::World& world, const assets::BuildingTable& buildings,
                      const assets::FigureTable& figures, const AmbientView& view);
    void send_walker(const sim::World& world, const assets::BuildingTable& buildings,
                     const assets::FigureTable& figures, const sim::House& house, std::uint32_t roll);
    void sea_creatures(const sim::World& world, const assets::FigureTable& figures, const AmbientView& view);
    void jugglers(const sim::World& world, const assets::BuildingTable& buildings, const assets::FigureTable& figures,
                  const AmbientView& view, std::uint32_t dt_ms);
    void sounds(const sim::World& world, const assets::BuildingTable& buildings, const AmbientView& view,
                std::uint32_t dt_ms);

    sim::Random m_random;
    std::unordered_map<std::uint32_t, std::uint32_t> m_city_ms;  ///< key island << 8 | city: time to the next round
    std::uint32_t m_sea_ms = 0;
    std::uint64_t m_time_ms = 0;
    std::vector<Walker> m_walking;
    std::unordered_set<std::uint64_t> m_busy_houses;
    std::vector<Creature> m_creatures;
    std::unordered_map<std::uint64_t, AnimState> m_juggler_anims;
    struct Emitter {
        std::uint16_t x = 0;
        std::uint16_t y = 0;
        bool birds = true;
        std::uint32_t hash = 0;
    };
    struct IslandEmitters {
        std::uint64_t built_ms = 0;
        bool built = false;
        std::vector<Emitter> list;
    };
    std::vector<IslandEmitters> m_emitters;  ///< by island, looked for again every 10 s
    std::vector<FigureInstance> m_walker_instances;
    std::vector<EffectFigure> m_effects;
    std::vector<AmbientSound> m_sounds;
    Stats m_stats;
};

}  // namespace o1602::render
