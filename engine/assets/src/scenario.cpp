// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/scenario.hpp"

#include <algorithm>
#include <cstring>

namespace o1602::assets {

using core::ErrorCode;
using core::make_error;

namespace {

constexpr std::size_t kPlayerRecordSize = 0x430;
constexpr std::size_t kShipRecordSize = 0x1B4;

std::int32_t read_i32(std::span<const std::uint8_t> d, std::size_t o) {
    std::uint32_t v = 0;
    for (int i = 3; i >= 0; --i) {
        v = (v << 8) | d[o + static_cast<std::size_t>(i)];
    }
    return static_cast<std::int32_t>(v);
}

std::uint16_t read_u16(std::span<const std::uint8_t> d, std::size_t o) {
    return static_cast<std::uint16_t>(d[o] | (d[o + 1] << 8));
}

std::uint32_t read_u32(std::span<const std::uint8_t> d, std::size_t o) {
    return static_cast<std::uint32_t>(read_i32(d, o));
}

/// Zero terminated string inside a fixed field (Windows-1252, kept as raw bytes).
std::string read_string(std::span<const std::uint8_t> d, std::size_t o, std::size_t max) {
    const auto field = d.subspan(o, max);
    const auto end = std::find(field.begin(), field.end(), std::uint8_t{0});
    return {field.begin(), end};
}

}  // namespace

core::Result<std::vector<PlayerRecord>> load_players(const ChunkFile& file) {
    std::vector<PlayerRecord> players;
    const Chunk* chunk = file.find("PLAYER4");
    if (chunk == nullptr) {
        return players;
    }
    const auto data = file.payload(*chunk);
    if (data.size() % kPlayerRecordSize != 0) {
        return make_error(ErrorCode::InvalidFormat, "PLAYER4 size is not a multiple of 0x430");
    }
    for (std::size_t o = 0; o < data.size(); o += kPlayerRecordSize) {
        PlayerRecord p;
        p.money = read_i32(data, o);
        p.kind = static_cast<PlayerKind>(data[o + 4]);
        p.slot = data[o + 5];
        p.color = static_cast<PlayerColor>(std::min<std::uint8_t>(data[o + 7], kPlayerColorCount - 1));
        p.name = read_string(data, o + 0x3C0, 0x40);
        // Saver 0x00478160: the build unlocks and limits of the player record.
        p.infra_mask = read_u32(data, o + 0x34);
        p.castles_built = data[o + 0x0E];
        p.cathedrals_built = data[o + 0x0F];
        p.arches_allowed = read_u16(data, o + 0x10);
        p.arches_built = read_u16(data, o + 0x12);
        p.monuments_allowed = read_u16(data, o + 0x38);
        p.monuments_built = read_u16(data, o + 0x3A);
        players.push_back(std::move(p));
    }
    return players;
}

core::Result<std::vector<ShipRecord>> load_ships(const ChunkFile& file) {
    std::vector<ShipRecord> ships;
    const Chunk* chunk = file.find("SHIP4");
    if (chunk == nullptr) {
        return ships;
    }
    const auto data = file.payload(*chunk);
    if (data.size() % kShipRecordSize != 0) {
        return make_error(ErrorCode::InvalidFormat, "SHIP4 size is not a multiple of 0x1B4");
    }
    for (std::size_t o = 0; o < data.size(); o += kShipRecordSize) {
        ShipRecord s;
        s.name = read_string(data, o, 0x1C);
        s.x = read_u16(data, o + 0x1C);
        s.y = read_u16(data, o + 0x1E);
        s.figure = read_u16(data, o + 0x48);
        s.owner = data[o + 0x4B];
        s.island = data[o + 0x4D];
        s.direction = data[o + 0x50];
        for (std::size_t c = 0; c < s.cargo.size(); ++c) {
            const std::size_t at = o + 0x174 + 8 * c;
            const std::uint16_t producer = read_u16(data, at);
            if (producer != 0) {
                s.cargo[c] = ShipRecord::Cargo{std::uint32_t{producer} + 20000U,
                                               static_cast<std::uint16_t>(read_u16(data, at + 2) & 0x3FFFU),
                                               static_cast<std::uint16_t>(read_u16(data, at + 4) & 0x3FFU)};
            }
        }
        s.slot = read_u16(data, o + 0x46);
        s.kind = data[o + 0x4A];
        s.health = read_u16(data, o + 0x3C);
        s.cannons = data[o + 0x42];
        s.flags = data[o + 0x43];
        s.sale_factor = read_u16(data, o + 0x44);
        s.trader = data[o + 0x4C];
        s.route_stop = data[o + 0x4F];
        s.order = read_u32(data, o + 0x2C);
        s.target = read_u32(data, o + 0x30);
        s.previous = read_u32(data, o + 0x34);
        for (std::size_t r = 0; r < s.route.size(); ++r) {
            const std::size_t at = o + 0x52 + 0x24 * r;
            s.route[r].ref = read_u32(data, at);
            for (std::size_t e = 0; e < 3; ++e) {
                const std::size_t ea = at + 4 + 8 * e;
                const std::uint16_t producer = read_u16(data, ea);
                s.route[r].entries[e] = ShipRecord::RouteEntry{producer != 0 ? std::uint32_t{producer} + 20000U : 0U,
                                                               read_u16(data, ea + 2), (data[ea + 4] & 1U) != 0};
            }
        }
        ships.push_back(std::move(s));
    }
    return ships;
}

core::Result<std::vector<TraderRecord>> load_traders(const ChunkFile& file) {
    constexpr std::size_t kSize = 0x25C;
    constexpr std::size_t kGoods = 50;
    std::vector<TraderRecord> traders;
    const Chunk* chunk = file.find("HANDLER");
    if (chunk == nullptr) {
        return traders;
    }
    const auto data = file.payload(*chunk);
    if (data.size() % kSize != 0) {
        return make_error(ErrorCode::InvalidFormat, "HANDLER size is not a multiple of 0x25C");
    }
    for (std::size_t o = 0; o < data.size(); o += kSize) {
        if (data[o] == 0) {
            continue;  // free record
        }
        TraderRecord t;
        t.ship_slot = read_u16(data, o + 1);
        t.index = data[o + 3];
        for (std::size_t g = 0; g < kGoods; ++g) {
            const std::size_t at = o + 4 + 12 * g;
            const std::uint32_t a = read_u32(data, at);
            const std::uint32_t b = read_u32(data, at + 4);
            const std::uint16_t producer = read_u16(data, at + 8);
            if (producer == 0) {
                continue;
            }
            t.goods.push_back(TraderRecord::Good{std::uint32_t{producer} + 20000U,
                                                 static_cast<std::uint16_t>(a & 0x3FFU),
                                                 static_cast<std::uint8_t>((a >> 10) & 0x1FU), b});
        }
        traders.push_back(std::move(t));
    }
    return traders;
}

core::Result<std::optional<ScenarioPlan>> load_scenario_plan(const ChunkFile& file) {
    const Chunk* chunk = file.find("SZENE");
    if (chunk == nullptr) {
        return std::optional<ScenarioPlan>{};
    }
    const auto d = file.payload(*chunk);
    constexpr std::size_t kIslands = 0x174;
    constexpr std::size_t kIslandSize = 16;
    constexpr std::uint32_t kMaxIslands = 50;
    if (d.size() < kIslands) {
        return make_error(ErrorCode::InvalidFormat, "SZENE chunk too short");
    }
    ScenarioPlan plan;
    plan.natives = {read_u32(d, 0x40), read_u32(d, 0x44)};
    plan.max_crops = read_u32(d, 0x60);
    plan.gold_min_size = read_u32(d, 0x68);
    plan.gold_max_size = read_u32(d, 0x6C);
    const auto resource = [&](std::size_t o, bool typed) {
        ScenarioPlan::Resource r;
        r.building = read_u16(d, o) + std::uint32_t{20000};
        r.count = read_u16(d, o + 2);
        r.type = typed ? read_u32(d, o + 4) : 0;
        if (read_u16(d, o) == 0) {
            r = ScenarioPlan::Resource{};
        }
        return r;
    };
    for (std::size_t k = 0; k < plan.ores.size(); ++k) {
        plan.ores[k] = resource(0x94 + 8 * k, true);
    }
    for (std::size_t k = 0; k < plan.crops.size(); ++k) {
        plan.crops[k] = resource(0xB4 + 8 * k, false);
    }
    for (std::size_t k = 0; k < plan.treasures.size(); ++k) {
        plan.treasures[k] = resource(0x114 + 8 * k, false);
    }
    const std::uint32_t count = std::min(read_u32(d, 0x64), kMaxIslands);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t o = kIslands + i * kIslandSize;
        if (o + kIslandSize > d.size()) {
            return make_error(ErrorCode::InvalidFormat, "SZENE island list exceeds the chunk");
        }
        ScenarioPlan::Island island;
        island.climate = d[o];
        island.size = d[o + 1];
        island.flags = d[o + 2];
        island.number = d[o + 3];
        island.template_number = read_u16(d, o + 4);
        island.x = read_u32(d, o + 8);
        island.y = read_u32(d, o + 12);
        plan.islands.push_back(island);
    }
    return std::optional<ScenarioPlan>{std::move(plan)};
}

std::optional<std::array<std::uint16_t, 512>> load_random_table(const ChunkFile& file) {
    const Chunk* chunk = file.find("RANDTAB");
    if (chunk == nullptr || chunk->size < 2) {
        return std::nullopt;
    }
    const auto d = file.payload(*chunk);
    const std::size_t words = d.size() / 2;
    std::array<std::uint16_t, 512> table{};
    for (std::size_t i = 0; i < table.size(); ++i) {
        table[i] = read_u16(d, (i % words) * 2);
    }
    return table;
}

std::optional<std::uint32_t> load_mission_number(const ChunkFile& file) {
    const Chunk* chunk = file.find("SZENE_MISSNR");
    if (chunk == nullptr || chunk->size < 4) {
        return std::nullopt;
    }
    return read_u32(file.payload(*chunk), 0);
}

std::vector<SavegameSlot> parse_savegame_slots(std::string_view text) {
    std::vector<SavegameSlot> slots;
    bool in_names = false;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string_view::npos) {
            continue;
        }
        line.remove_prefix(first);
        if (line.starts_with("Objekt:")) {
            in_names = line.find("SPIELNAME") != std::string_view::npos;
            continue;
        }
        if (line.starts_with("EndObj")) {
            in_names = false;
            continue;
        }
        if (!in_names || !line.starts_with("Name:")) {
            continue;
        }
        // Name: <slot>, "<text>", <flag>
        line.remove_prefix(5);
        const auto comma = line.find(',');
        const auto q1 = line.find('"');
        const auto q2 = q1 == std::string_view::npos ? q1 : line.find('"', q1 + 1);
        if (comma == std::string_view::npos || q2 == std::string_view::npos) {
            continue;
        }
        std::uint32_t slot = 0;
        bool digits = false;
        for (char c : line.substr(0, comma)) {
            if (c >= '0' && c <= '9') {
                slot = slot * 10 + static_cast<std::uint32_t>(c - '0');
                digits = true;
            }
        }
        if (!digits || slot > 99) {
            continue;
        }
        SavegameSlot s;
        s.slot = slot;
        s.name = std::string(line.substr(q1 + 1, q2 - q1 - 1));
        s.file = "Savegame/game" + std::string(slot < 10 ? "0" : "") + std::to_string(slot) + ".gam";
        slots.push_back(std::move(s));
    }
    return slots;
}

}  // namespace o1602::assets
