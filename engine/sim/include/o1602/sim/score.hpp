// SPDX-License-Identifier: GPL-3.0-or-later
//
// Scores and the end of a game (docs/re/missions.md): the points of a
// player (Player_ComputeScore 0x00474F90), what identifies a game for its high score list, and the
// figures the end screen shows.
//
// PROVISIONAL: the score formula of the original is not analysed yet. player_score() adds the
// inhabitants of every own city weighted with the Hiscore of their class (haeuser.cod BGRUPPE,
// the only per-class value named for it) and doubles the sum for a winner (+100 %, confirmed). Once
// 0x00474F90 is decompiled, only score_parts() changes; callers stay as they are.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/sim/player.hpp"

namespace o1602::sim {

class World;

/// What a score is made of, for the end screen's lines.
struct ScoreParts {
    std::int64_t inhabitants = 0;  ///< inhabitants weighted with the Hiscore of their class
    std::int64_t subtotal = 0;     ///< before the bonus
    bool won = false;              ///< the winner's bonus: the subtotal counts twice
    std::int64_t total = 0;
};
[[nodiscard]] ScoreParts score_parts(const World& world, const assets::BuildingTable& buildings, PlayerId player);
[[nodiscard]] inline std::int64_t player_score(const World& world, const assets::BuildingTable& buildings,
                                               PlayerId player) {
    return score_parts(world, buildings, player).total;
}

/// What names a game's high score list: the scenario it comes from (the NAME chunk, which
/// savegames keep), its stars and numbers (SZENE_*) and the game id (TIMERS +0xAC, else
/// SZENE_GAMEID). Empty `name` when the world has no source (a test world).
struct GameIdentity {
    static constexpr std::uint32_t kNone = 0xFF;
    std::string name;  ///< Windows-1252, as the file names of Szenes/
    std::uint32_t game_id = 0;
    std::uint32_t stars = 0;
    std::uint32_t mission_number = kNone;  ///< SZENE_MISSNR, else TIMERS +0xA7
    std::uint32_t campaign = kNone;        ///< SZENE_KAMPAGNE
};
[[nodiscard]] GameIdentity game_identity(const World& world);

/// How the game ended for the player the screen is for.
enum class GameOutcome : std::uint8_t {
    Won,   ///< accomplished the mission (or met it when another did)
    Lost,  ///< defeated, or another player accomplished the mission
    Left,  ///< the player quit a game nobody had decided ("Ende" in the options)
};

/// One player's line on the end screen.
struct EndScreenRow {
    PlayerId player = kNoPlayer;
    std::string name;
    BannerId banner = 0;
    PlayerRole role = PlayerRole::Human;
    PlayerStatus status = PlayerStatus::Playing;
    bool won = false;
    std::uint32_t inhabitants = 0;  ///< Einwohner of all own cities, as the city list counts them
    std::uint32_t cities = 0;
    std::uint32_t ships = 0;  ///< not sinking
    std::int32_t money = 0;
    std::int64_t score = 0;
};

/// Everything the end screen shows, gathered once when it opens.
struct EndScreenSummary {
    GameOutcome outcome = GameOutcome::Left;
    PlayerId local = kNoPlayer;
    std::uint64_t time_ms = 0;  ///< game time when the game ended
    GameIdentity game;
    /// Humans and computers that took part (not absent), the local player first, then by score.
    std::vector<EndScreenRow> rows;
};
[[nodiscard]] EndScreenSummary end_screen_summary(const World& world, const assets::BuildingTable& buildings,
                                                  PlayerId local, GameOutcome outcome);

}  // namespace o1602::sim
