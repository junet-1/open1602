// SPDX-License-Identifier: GPL-3.0-or-later
// The goods statistics on games of the original (docs/architecture/0012-goods-statistics.md): what
// the ledgers booked as coming in and going out must be what the cities' goods changed by. The
// goods a city holds are those in its warehouses and, since production is booked where it happens,
// those waiting in its buildings (made goods and raw material); only what carriers and carts have
// on their way is in neither, so a small difference remains.

#include <cstdio>
#include <cstdlib>
#include <map>
#include <tuple>
#include <vector>

#include "../test_framework.hpp"
#include "fixtures.hpp"

#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/game_data.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/savegame.hpp"
#include "o1602/sim/goods_statistics.hpp"
#include "o1602/sim/production.hpp"
#include "o1602/sim/simulation.hpp"

using namespace o1602;

namespace {

/// A city by island and index, and a ware.
using Key = std::tuple<std::uint16_t, std::uint8_t, std::uint8_t>;

/// What the human players' cities hold of every trade good, 1/32 t.
std::map<Key, std::int64_t> held(const sim::World& world, const assets::BuildingTable& buildings) {
    std::map<Key, std::int64_t> out;
    for (const sim::City& c : world.cities()) {
        const sim::Player* owner = world.player(c.owner);
        if (owner == nullptr || owner->role != sim::PlayerRole::Human) {
            continue;
        }
        for (std::uint8_t w = assets::ware::kFirstTrade; w <= assets::ware::kLastTrade; ++w) {
            out[{c.island, c.index, w}] += c.stock(w);
        }
    }
    for (const sim::Production& p : world.productions()) {
        if (!p.used) {
            continue;
        }
        const sim::MapCell& cell = world.islands()[p.island].map.top(p.x, p.y);
        const assets::BuildingType* b = buildings.by_sprite(cell.sprite);
        const auto add = [&](std::uint8_t w, std::uint16_t amount) {
            if (const auto it = out.find({p.island, cell.city, w}); it != out.end()) {
                it->second += amount;
            }
        };
        if (b != nullptr && sim::is_producer(*b)) {
            add(b->ware, p.stock);
            add(b->raw_ware, p.raw);
            add(b->raw2_ware, p.raw2);
        }
    }
    return out;
}

/// Runs until the next game minute was closed.
void to_next_minute(sim::Simulation& simulation, const sim::World& world) {
    const std::uint32_t closed = world.cities().empty() ? 0 : world.cities().front().ledger.minutes_closed();
    for (std::uint32_t i = 0; i <= sim::kGoodsMinuteMs / sim::Simulation::kTickMs; ++i) {
        simulation.tick();
        if (world.cities().empty() || world.cities().front().ledger.minutes_closed() != closed) {
            return;
        }
    }
}

}  // namespace

TEST_CASE("data: the goods statistics of games of the original add up to what the cities hold") {
    auto dir = assets::GameDirectory::from_environment();
    if (!dir) {
        SKIP("ANNO1602_DIR not set");
    }
    auto buildings = assets::BuildingTable::load(*dir);
    auto figures = assets::FigureTable::load(*dir);
    REQUIRE(buildings.ok());
    REQUIRE(figures.ok());
    bool any = false;
    for (const char* name : {"SAVEGAME/game03.gam", "SAVEGAME/game10.gam"}) {
        const auto path = test::fixture(*dir, name);
        if (!path) {
            continue;
        }
        auto file = assets::ChunkFile::load(*path);
        REQUIRE(file.ok());
        auto world = sim::load_world(*file, *buildings, *dir);
        REQUIRE(world.ok());
        REQUIRE(!world->cities().empty());
        any = true;
        sim::Simulation simulation(*world, *buildings, *figures);
        to_next_minute(simulation, *world);
        const std::map<Key, std::int64_t> before = held(*world, *buildings);
        constexpr std::uint32_t kMinutes = 20;
        for (std::uint32_t m = 0; m < kMinutes; ++m) {
            to_next_minute(simulation, *world);
        }
        const std::map<Key, std::int64_t> after = held(*world, *buildings);

        std::int64_t moved = 0;
        std::int64_t off = 0;
        for (const auto& [key, start] : before) {
            const auto [island, index, ware] = key;
            const sim::City* city = world->city(island, index);
            REQUIRE(city != nullptr);
            const sim::FlowTotals t = city->ledger.totals(ware, kMinutes);
            const std::int64_t change = after.at(key) - start;
            const std::int64_t difference = std::llabs(change - t.net());
            moved += static_cast<std::int64_t>(t.in() + t.out());
            off += difference;
            // A cart holds a few tons; more than that is a movement nobody booked.
            if (difference > 16 * static_cast<std::int64_t>(assets::kStockUnitsPerTon)) {
                std::fprintf(stderr, "  %s: city %u/%u ware %u: held %+lld, booked %+lld (in %llu, out %llu)\n", name,
                             unsigned{island}, unsigned{index}, unsigned{ware}, static_cast<long long>(change),
                             static_cast<long long>(t.net()), static_cast<unsigned long long>(t.in()),
                             static_cast<unsigned long long>(t.out()));
            }
            CHECK(difference <= 16 * static_cast<std::int64_t>(assets::kStockUnitsPerTon));
        }
        std::fprintf(stderr, "  %s: %lld t moved in %u minutes, %lld t not accounted for (on the way)\n", name,
                     static_cast<long long>(moved / assets::kStockUnitsPerTon), kMinutes,
                     static_cast<long long>(off / assets::kStockUnitsPerTon));
        CHECK(moved > 0);
        CHECK(off * 20 <= moved);  // at most 5 % of what moved
    }
    if (!any) {
        SKIP("no savegames");
    }
}
