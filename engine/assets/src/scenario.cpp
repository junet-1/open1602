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
        p.name = read_string(data, o + 0x3BC, 0x40);
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
        s.direction = data[o + 0x4D];
        ships.push_back(std::move(s));
    }
    return ships;
}

}  // namespace o1602::assets
