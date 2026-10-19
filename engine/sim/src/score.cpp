// SPDX-License-Identifier: GPL-3.0-or-later
//
// Scores and the figures of the end screen (docs/re/missions.md).
#include "o1602/sim/score.hpp"

#include <algorithm>

#include "o1602/assets/scenario.hpp"
#include "o1602/core/byte_reader.hpp"
#include "o1602/sim/mission.hpp"
#include "o1602/sim/world.hpp"

namespace o1602::sim {

namespace {

constexpr std::size_t kTimersMission = 0xA7;  // TIMERS: mission number (endless game 1)
constexpr std::size_t kTimersGameId = 0xAC;   // TIMERS: game id

}  // namespace

ScoreParts score_parts(const World& world, const assets::BuildingTable& buildings, PlayerId player) {
    ScoreParts s;
    const Player* p = world.player(player);
    if (p == nullptr) {
        return s;
    }
    // PROVISIONAL (0x00474F90 not analysed): inhabitants weighted with the Hiscore of their class.
    const auto& classes = buildings.resident_classes();
    for (const City& c : world.cities()) {
        if (c.owner != player) {
            continue;
        }
        for (std::size_t k = 0; k < c.inhabitants.size(); ++k) {
            const std::uint32_t weight = k < classes.size() ? classes[k].hiscore : 0;
            s.inhabitants += std::int64_t{c.inhabitants[k]} * weight;
        }
    }
    s.subtotal = s.inhabitants;
    s.won = p->won;
    // A winner's points count twice (HighScore_UpdateAllHumans 0x00475990, confirmed).
    s.total = s.won ? 2 * s.subtotal : s.subtotal;
    return s;
}

GameIdentity game_identity(const World& world) {
    GameIdentity id;
    const assets::ChunkFile* file = world.source().get();
    if (file == nullptr) {
        return id;
    }
    if (const assets::Chunk* name = file->find("NAME")) {
        const auto bytes = file->payload(*name);
        const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{0});
        id.name.assign(bytes.begin(), end);
    }
    const assets::ScenarioHeader header = assets::load_scenario_header(*file);
    id.stars = header.ranking;
    id.mission_number = header.mission_number;
    id.campaign = header.campaign;
    id.game_id = header.game_id;
    if (const assets::Chunk* timers = file->find("TIMERS")) {
        const auto t = file->payload(*timers);
        if (t.size() >= kTimersGameId + 4) {
            core::ByteReader r(t);
            if (r.seek(kTimersGameId)) {
                if (const auto game = r.u32(); game && *game != 0) {
                    id.game_id = *game;
                }
            }
        }
        if (id.mission_number == GameIdentity::kNone && t.size() > kTimersMission) {
            id.mission_number = t[kTimersMission];
        }
    }
    return id;
}

EndScreenSummary end_screen_summary(const World& world, const assets::BuildingTable& buildings, PlayerId local,
                                    GameOutcome outcome) {
    EndScreenSummary s;
    s.outcome = outcome;
    s.local = local;
    s.time_ms = world.game_end() ? world.game_end()->time_ms : world.time_ms();
    s.game = game_identity(world);
    const std::vector<std::uint32_t> workers = city_workers(world, buildings);
    for (std::size_t i = 0; i < world.players().size(); ++i) {
        const Player& p = world.players()[i];
        if ((p.role != PlayerRole::Human && p.role != PlayerRole::Computer) || p.status == PlayerStatus::Absent) {
            continue;
        }
        EndScreenRow r;
        r.player = static_cast<PlayerId>(i);
        r.name = p.name;
        r.banner = p.banner;
        r.role = p.role;
        r.status = p.status;
        r.won = p.won;
        r.money = p.money;
        for (std::size_t c = 0; c < world.cities().size(); ++c) {
            if (world.cities()[c].owner == r.player) {
                ++r.cities;
                r.inhabitants += city_einwohner(world.cities()[c], c < workers.size() ? workers[c] : 0);
            }
        }
        for (const Ship& ship : world.ships()) {
            r.ships += ship.owner == r.player && !ship.sinking ? 1U : 0U;
        }
        r.score = player_score(world, buildings, r.player);
        s.rows.push_back(std::move(r));
    }
    std::stable_sort(s.rows.begin(), s.rows.end(), [local](const EndScreenRow& a, const EndScreenRow& b) {
        if ((a.player == local) != (b.player == local)) {
            return a.player == local;
        }
        return a.score > b.score;
    });
    return s;
}

}  // namespace o1602::sim
