// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/island.hpp"

#include "o1602/core/byte_reader.hpp"

namespace o1602::assets {

using core::ErrorCode;
using core::make_error;

namespace {

constexpr std::size_t kMinHeaderSize = 8;
constexpr std::size_t kTileSize = 8;

core::Result<IslandHeader> parse_header(const Chunk& chunk, std::span<const std::uint8_t> data) {
    if (data.size() < kMinHeaderSize) {
        return make_error(ErrorCode::InvalidFormat, chunk.name + " chunk too small");
    }
    core::ByteReader r(data);
    IslandHeader h;
    h.chunk_name = chunk.name;
    h.number = *r.u8();
    h.width = *r.u8();
    h.height = *r.u8();
    h.flags = *r.u8();
    h.pos_x = *r.u16();
    h.pos_y = *r.u16();
    h.raw.assign(data.begin(), data.end());
    return h;
}

core::Result<std::vector<IslandTile>> parse_tiles(std::span<const std::uint8_t> data) {
    if (data.size() % kTileSize != 0) {
        return make_error(ErrorCode::InvalidFormat, "INSELHAUS size is not a multiple of 8");
    }
    std::vector<IslandTile> tiles;
    tiles.reserve(data.size() / kTileSize);
    core::ByteReader r(data);
    while (!r.at_end()) {
        IslandTile t;
        t.id = *r.u16();
        t.x = *r.u8();
        t.y = *r.u8();
        t.bits = *r.u32();
        tiles.push_back(t);
    }
    return tiles;
}

}  // namespace

core::Result<std::vector<Island>> load_islands(const ChunkFile& file) {
    std::vector<Island> islands;
    for (const Chunk& chunk : file.chunks()) {
        if (chunk.name == "INSEL3" || chunk.name == "INSEL5") {
            auto header = parse_header(chunk, file.payload(chunk));
            if (!header) {
                return header.error();
            }
            islands.push_back(Island{std::move(header).value(), {}});
        } else if (chunk.name == "INSELHAUS") {
            if (islands.empty()) {
                return make_error(ErrorCode::InvalidFormat, "INSELHAUS chunk without preceding island header");
            }
            auto tiles = parse_tiles(file.payload(chunk));
            if (!tiles) {
                return tiles.error();
            }
            islands.back().layers.push_back(std::move(tiles).value());
        }
    }
    return islands;
}

}  // namespace o1602::assets
