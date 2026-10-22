// SPDX-License-Identifier: GPL-3.0-or-later
//
// o1602-tool bench: runs a game headless as fast as it goes and measures the ticks, for estimating
// what a computer needs (docs/performance.md). Every player is handed to the computer so that the
// world keeps building without anyone at the controls.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "tool.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/core/file.hpp"
#include "o1602/sim/new_game.hpp"
#include "o1602/sim/save_game.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/sim/world.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// windows.h first
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace o1602::tool {

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

/// The most memory the process has held so far, in MB; nothing where it cannot be asked.
std::optional<double> peak_memory_mb() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters) == 0) {
        return std::nullopt;
    }
    return static_cast<double>(counters.PeakWorkingSetSize) / (1024.0 * 1024.0);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return std::nullopt;
    }
#if defined(__APPLE__)
    return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);  // bytes
#else
    return static_cast<double>(usage.ru_maxrss) / 1024.0;  // KB
#endif
#endif
}

/// Tick times of one report period in microseconds.
struct Period {
    std::vector<double> ticks;

    [[nodiscard]] double mean() const {
        double sum = 0.0;
        for (const double t : ticks) {
            sum += t;
        }
        return ticks.empty() ? 0.0 : sum / static_cast<double>(ticks.size());
    }
    /// The value below which the share `q` of the ticks lies (sorts the ticks).
    [[nodiscard]] double quantile(double q) {
        if (ticks.empty()) {
            return 0.0;
        }
        std::sort(ticks.begin(), ticks.end());
        const auto i = static_cast<std::size_t>(q * static_cast<double>(ticks.size()));
        return ticks[std::min(i, ticks.size() - 1)];
    }
};

void print_world_line(const sim::World& world, const sim::Simulation& simulation) {
    std::uint64_t inhabitants = 0;
    for (const sim::City& city : world.cities()) {
        for (const std::uint32_t n : city.inhabitants) {
            inhabitants += n;
        }
    }
    std::printf(" %7zu %8llu %6zu %5zu %8zu %7zu\n", world.houses().size(),
                static_cast<unsigned long long>(inhabitants), world.productions().size(), world.ships().size(),
                world.soldiers().size(), simulation.logistics().walkers().size());
}

}  // namespace

int cmd_bench(const Args& args) {
    if (args.positional.size() > 1) {
        return usage();
    }
    auto game = assets::GameDirectory::from_environment();
    if (!game) {
        return fail(game.error());
    }
    auto buildings = assets::BuildingTable::load(*game);
    if (!buildings) {
        return fail(buildings.error());
    }
    auto figures = assets::FigureTable::load(*game);
    if (!figures) {
        return fail(figures.error());
    }
    std::vector<std::string> warnings;
    const auto make_world = [&]() -> core::Result<sim::World> {
        if (!args.positional.empty()) {
            auto bytes = core::read_file(fs::path(args.positional[0]));
            if (!bytes) {
                return bytes.error();
            }
            if (sim::is_open1602_save(*bytes)) {
                return sim::read_game(*bytes, *buildings, &warnings, &*figures);
            }
            auto file = assets::ChunkFile::from_bytes(*bytes);
            if (!file) {
                return file.error();
            }
            return sim::load_world(*file, *buildings, *game, &warnings);
        }
        auto texts = assets::TextTable::load(*game);
        if (!texts) {
            return texts.error();
        }
        sim::NewGameOptions options;
        options.name = "Bench";
        options.difficulty = static_cast<std::uint8_t>(std::min<std::size_t>(args.number("--level").value_or(1), 3));
        options.world.seed = args.number("--seed").value_or(1);
        options.world.guarantee_resources = true;
        sim::WorldSettings settings;
        if (const auto size = args.pair("--world", 'x')) {
            settings.width = static_cast<std::uint32_t>(std::max(0, size->first));
            settings.height = static_cast<std::uint32_t>(std::max(0, size->second));
        }
        settings.players = static_cast<std::uint8_t>(
            std::clamp<std::size_t>(args.number("--players").value_or(8), 1, sim::WorldSettings::kMaxPlayers));
        options.custom = settings;
        return sim::start_new_game(*game, *buildings, *texts, options, &warnings);
    };
    auto world = make_world();
    if (!world) {
        return fail(world.error());
    }
    for (const auto& text : warnings) {
        std::fprintf(stderr, "warning: %s\n", text.c_str());
    }

    sim::Simulation simulation(*world, *buildings, *figures);
    std::size_t handed_over = 0;
    for (sim::PlayerId p = sim::first_human_player(*world); p != sim::kNoPlayer; p = sim::first_human_player(*world)) {
        if (!simulation.execute(sim::PlayerControl{p, true}).accepted) {
            return fail(core::make_error(core::ErrorCode::Unsupported,
                                         "player " + std::to_string(p) + " cannot be handed to the computer"));
        }
        ++handed_over;
    }
    const std::size_t minutes = std::max<std::size_t>(args.number("--minutes").value_or(600), 1);
    const std::size_t period_minutes = std::clamp<std::size_t>(args.number("--period").value_or(60), 1, minutes);
    std::printf("world %ux%u, %zu islands, %zu players handed to the computer; %zu game minutes\n", world->width(),
                world->height(), world->islands().size(), handed_over, minutes);
    std::printf("(times in microseconds per tick of %u ms game time; 20 ticks per second at speed 1, %u at speed %u)\n",
                sim::Simulation::kTickMs, 20 * simulation.rules().max_game_speed, simulation.rules().max_game_speed);
    std::printf("minute    mean    p50     p99     max  houses  inhabit.  prods ships soldiers walkers\n");

    const std::vector<std::string_view> steps = sim::Simulation::tick_step_names();
    std::vector<std::uint64_t> step_ns(steps.size(), 0);
    std::vector<std::uint64_t> step_total_ns(steps.size(), 0);
    std::vector<std::uint64_t> step_max_ns(steps.size(), 0);
    double total_us = 0.0;
    double max_us = 0.0;
    const std::uint64_t ticks_per_minute = 60'000 / sim::Simulation::kTickMs;
    Period period;
    period.ticks.reserve(period_minutes * ticks_per_minute);
    for (std::size_t minute = 1; minute <= minutes; ++minute) {
        for (std::uint64_t t = 0; t < ticks_per_minute; ++t) {
            std::fill(step_ns.begin(), step_ns.end(), 0);
            const auto start = Clock::now();
            simulation.tick_timed(step_ns);
            const double us = ms_since(start) * 1000.0;
            // What the interface would take; left lying, the buffers only grow to their bound.
            (void)simulation.take_production_events();
            (void)simulation.take_construction_events();
            (void)simulation.take_military_events();
            (void)simulation.take_city_events();
            (void)simulation.take_ship_events();
            (void)simulation.take_diplomacy_notices();
            (void)simulation.take_command_results();
            period.ticks.push_back(us);
            total_us += us;
            max_us = std::max(max_us, us);
            for (std::size_t i = 0; i < steps.size(); ++i) {
                step_total_ns[i] += step_ns[i];
                step_max_ns[i] = std::max(step_max_ns[i], step_ns[i]);
            }
        }
        if (minute % period_minutes == 0 || minute == minutes) {
            const double mean = period.mean();
            const double p50 = period.quantile(0.5);
            const double p99 = period.quantile(0.99);
            std::printf("%6zu %7.1f %6.0f %7.0f %7.0f", minute, mean, p50, p99, period.ticks.back());
            print_world_line(*world, simulation);
            std::fflush(stdout);
            period.ticks.clear();
        }
    }

    const double ticks = static_cast<double>(minutes * ticks_per_minute);
    std::printf("\nall ticks: mean %.1f us, max %.0f us; at speed %u one core is %.1f%% busy with the game\n",
                total_us / ticks, max_us, simulation.rules().max_game_speed,
                total_us / ticks * 20.0 * simulation.rules().max_game_speed / 10'000.0);
    std::printf("step           share    mean      max\n");
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const double step_us = static_cast<double>(step_total_ns[i]) / 1000.0;
        std::printf("%-13.*s %5.1f%% %7.1f %8.0f\n", static_cast<int>(steps[i].size()), steps[i].data(),
                    total_us > 0.0 ? 100.0 * step_us / total_us : 0.0, step_us / ticks,
                    static_cast<double>(step_max_ns[i]) / 1000.0);
    }

    // The multiplayer fingerprint (net::Session, every hash_interval frames) and a save.
    auto start = Clock::now();
    const auto hash = simulation.state_hash();
    const double hash_ms = ms_since(start);
    start = Clock::now();
    auto save = sim::write_game(*world, *buildings, &simulation);
    const double save_ms = ms_since(start);
    if (!save) {
        return fail(save.error());
    }
    std::printf("state hash %.1f ms%s; savegame %zu bytes in %.1f ms\n", hash_ms, hash ? "" : " (failed)", save->size(),
                save_ms);
    if (const auto memory = peak_memory_mb()) {
        std::printf("peak memory %.0f MB\n", *memory);
    }
    if (const auto it = args.options.find("--out"); it != args.options.end()) {
        if (auto status = core::write_file(fs::path(it->second), *save); !status) {
            return fail(status.error());
        }
        std::printf("-> %s\n", std::string(it->second).c_str());
    }
    return 0;
}

}  // namespace o1602::tool
