// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/savegame.hpp"

#include <algorithm>

namespace o1602::assets {

using core::ErrorCode;
using core::make_error;

namespace {

constexpr std::size_t kCitySize = 0xA8;
constexpr std::size_t kWarehouseSize = 0x3EC;
constexpr std::size_t kWarehouseSlots = 23;
constexpr std::size_t kWareSlotSize = 20;
constexpr std::size_t kWareSlotsOffset = 0x2C;
constexpr std::uint8_t kFirstWare = 2;
constexpr std::size_t kProductionSize = 0x18;
constexpr std::size_t kAnimalSize = 8;
constexpr std::size_t kSoldierSize = 0x44;

std::uint32_t u32(std::span<const std::uint8_t> d, std::size_t o) {
    return static_cast<std::uint32_t>(d[o]) | (static_cast<std::uint32_t>(d[o + 1]) << 8) |
           (static_cast<std::uint32_t>(d[o + 2]) << 16) | (static_cast<std::uint32_t>(d[o + 3]) << 24);
}

/// Payloads of all chunks with this name (savegames: once, scenarios: once per island).
template <class Fn>
core::Status for_each_record(const ChunkFile& file, const char* name, std::size_t size, Fn&& fn) {
    for (const Chunk& chunk : file.chunks()) {
        if (chunk.name != name) {
            continue;
        }
        const auto data = file.payload(chunk);
        if (data.size() % size != 0) {
            return make_error(ErrorCode::InvalidFormat,
                              std::string(name) + " size is not a multiple of " + std::to_string(size));
        }
        for (std::size_t o = 0; o < data.size(); o += size) {
            fn(data.subspan(o, size));
        }
    }
    return core::Ok{};
}

}  // namespace

core::Result<GameState> load_game_state(const ChunkFile& file) {
    GameState state;
    auto status = for_each_record(file, "STADT4", kCitySize, [&](std::span<const std::uint8_t> r) {
        CityRecord c;
        c.island = r[0];
        c.index = r[1];
        c.owner = r[2];
        const auto name = r.subspan(0x87);
        c.name.assign(name.begin(), std::find(name.begin(), name.end(), std::uint8_t{0}));
        state.cities.push_back(std::move(c));
    });
    if (!status) {
        return status.error();
    }
    status = for_each_record(file, "KONTOR2", kWarehouseSize, [&](std::span<const std::uint8_t> r) {
        const std::uint32_t head = u32(r, 0);
        WarehouseRecord w;
        w.island = static_cast<std::uint8_t>(head);
        w.x = static_cast<std::uint8_t>(head >> 8);
        w.y = static_cast<std::uint8_t>(head >> 16);
        w.city = static_cast<std::uint8_t>((head >> 24) & 0xFU);
        for (std::size_t i = 0; i < kWarehouseSlots; ++i) {
            const std::size_t o = kWareSlotsOffset + i * kWareSlotSize;
            WareSlot s;
            s.ware = static_cast<std::uint8_t>(kFirstWare + i);
            const std::uint32_t id = u32(r, o + 16) & 0xFFFFU;
            s.producer_id = id == 0 ? 0 : static_cast<std::uint16_t>(id + 20000);
            for (std::size_t k = 0; k < s.raw.size(); ++k) {
                s.raw[k] = u32(r, o + 4 * k);
            }
            w.wares.push_back(s);
        }
        state.warehouses.push_back(std::move(w));
    });
    if (!status) {
        return status.error();
    }
    status = for_each_record(file, "PRODLIST2", kProductionSize, [&](std::span<const std::uint8_t> r) {
        ProductionRecord p;
        p.island = r[0];
        p.x = r[1];
        p.y = r[2];
        const auto u16 = [&](std::size_t o) { return static_cast<std::uint16_t>(r[o] | (r[o + 1] << 8)); };
        p.enabled = (r[0x14] & 0x01U) != 0;
        p.anim_phase = static_cast<std::uint8_t>((r[0x14] >> 2) & 0x0FU);
        p.stock = u16(5);
        p.countdown = u16(8);
        p.raw2 = u16(0x0A);
        p.raw = u16(0x0C);
        p.rate = r[0x0F];
        p.produced = u16(0x10);
        p.elapsed = u16(0x12);
        p.idle_cycles = static_cast<std::uint8_t>(r[0x15] & 0x0FU);
        p.flags = static_cast<std::uint8_t>((((r[0x14] >> 6) & 1U) << 4) | (((r[0x14] >> 1) & 1U) << 6));
        std::copy(r.begin() + 3, r.end(), p.raw_bytes.begin());
        state.production.push_back(p);
    });
    if (!status) {
        return status.error();
    }
    status = for_each_record(file, "HIRSCH2", kAnimalSize, [&](std::span<const std::uint8_t> r) {
        state.animals.push_back(AnimalRecord{r[0], r[1], r[2], u32(r, 4)});
    });
    if (!status) {
        return status.error();
    }
    status = for_each_record(file, "SOLDAT3", kSoldierSize, [&](std::span<const std::uint8_t> r) {
        SoldierRecord s;
        s.x2 = static_cast<std::uint16_t>(r[0] | (r[1] << 8));
        s.y2 = static_cast<std::uint16_t>(r[2] | (r[3] << 8));
        s.figure = static_cast<std::uint16_t>(r[6] | (r[7] << 8));
        s.owner = r[0x18];
        s.anim = r[0x19];
        s.direction = static_cast<std::uint8_t>(r[0x1B] & 7U);
        std::copy(r.begin(), r.end(), s.raw.begin());
        state.soldiers.push_back(s);
    });
    if (!status) {
        return status.error();
    }
    return state;
}

}  // namespace o1602::assets
