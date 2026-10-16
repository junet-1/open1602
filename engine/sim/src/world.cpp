// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/sim/world.hpp"

#include <map>

namespace o1602::sim {

using core::ErrorCode;
using core::make_error;

World::World(std::uint32_t width, std::uint32_t height)
    : m_width(width), m_height(height), m_cells(std::size_t{width} * height) {}

core::Status World::add_island(WorldIsland island) {
    const std::uint32_t w = island.map.width();
    const std::uint32_t h = island.map.height();
    if (island.x + w > m_width || island.y + h > m_height || w > kSea || h > kSea) {
        return make_error(ErrorCode::OutOfRange,
                          "island " + std::to_string(island.header.number) + " leaves the world map");
    }
    if (m_islands.size() >= kSea) {  // named limit instead of the original's 255 (L-003)
        return make_error(ErrorCode::OutOfRange, "too many islands (65535)");
    }
    for (std::uint32_t ly = 0; ly < h; ++ly) {
        for (std::uint32_t lx = 0; lx < w; ++lx) {
            if (m_cells[std::size_t{island.y + ly} * m_width + island.x + lx].island != kSea) {
                return make_error(ErrorCode::InvalidFormat,
                                  "island " + std::to_string(island.header.number) + " overlaps another island");
            }
        }
    }
    const auto index = static_cast<std::uint16_t>(m_islands.size());
    for (std::uint32_t ly = 0; ly < h; ++ly) {
        for (std::uint32_t lx = 0; lx < w; ++lx) {
            m_cells[std::size_t{island.y + ly} * m_width + island.x + lx] =
                Cell{index, static_cast<std::uint16_t>(lx), static_cast<std::uint16_t>(ly)};
        }
    }
    m_islands.push_back(std::move(island));
    return core::Ok{};
}

core::Result<World> load_world(const assets::ChunkFile& file, const assets::BuildingTable& buildings,
                               const assets::GameDirectory& game, std::vector<std::string>* warnings) {
    auto islands = assets::load_islands(file);
    if (!islands) {
        return islands.error();
    }
    const auto warn = [&](std::string message) {
        if (warnings != nullptr) {
            warnings->push_back(std::move(message));
        }
    };

    std::map<std::string, std::vector<assets::Island>> templates;  // cache: many islands share one
    World world(World::kOriginalWidth, World::kOriginalHeight);    // the original map size
    for (const auto& island : *islands) {
        const assets::Island* base = nullptr;
        if (const std::string path = island.header.template_path(); !path.empty()) {
            auto it = templates.find(path);
            if (it == templates.end()) {
                std::vector<assets::Island> loaded;
                if (const auto resolved = game.resolve(path)) {
                    if (auto tfile = assets::ChunkFile::load(*resolved)) {
                        if (auto tislands = assets::load_islands(*tfile)) {
                            loaded = std::move(tislands).value();
                        }
                    }
                }
                if (loaded.empty()) {
                    warn("island template not found or invalid: " + path);
                }
                it = templates.emplace(path, std::move(loaded)).first;
            }
            if (!it->second.empty()) {
                base = &it->second.front();
            }
        }
        WorldIsland wi{island.header, IslandMap::from_island(island, buildings, warnings, base), island.header.pos_x,
                       island.header.pos_y};
        if (auto status = world.add_island(std::move(wi)); !status) {
            warn(status.error().message);
        }
    }

    auto players = assets::load_players(file);
    if (!players) {
        return players.error();
    }
    std::vector<Player> slots;
    for (const auto& p : *players) {
        slots.push_back(Player{role_from_original(p.kind), banner_from_original(p.color), p.money, p.name});
    }
    world.set_players(std::move(slots));

    auto ships = assets::load_ships(file);
    if (!ships) {
        return ships.error();
    }
    for (const auto& s : *ships) {
        if (!world.contains(s.x, s.y)) {
            warn("ship '" + s.name + "' outside the world map");
            continue;
        }
        const auto direction = static_cast<std::uint8_t>(s.direction == 0xFF ? 0U : s.direction & 7U);
        world.add_ship(Ship{s.name, s.x, s.y, s.figure, player_from_original(s.owner), direction});
    }
    return world;
}

}  // namespace o1602::sim
