// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/building_table.hpp"

#include <algorithm>

#include "o1602/assets/cod.hpp"
#include "o1602/assets/cod_predefined.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/savegame.hpp"
#include "o1602/core/file.hpp"

namespace o1602::assets {

using core::ErrorCode;
using core::make_error;

namespace {

constexpr std::uint32_t kMaxId = 23500;       // Haus_GetById (0x004630F0) accepts 20000..23500
constexpr std::uint32_t kMaxSprite = 0x2000;  // 13 bits in the island grid

std::uint32_t non_negative(std::int32_t v) {
    return v < 0 ? 0U : static_cast<std::uint32_t>(v);
}

}  // namespace

core::Result<BuildingTable> BuildingTable::from_cod(const cod::Document& haeuser) {
    const auto* records = haeuser.root.records("HAUS");
    if (records == nullptr) {
        return make_error(ErrorCode::InvalidFormat, "haeuser.cod contains no HAUS records");
    }

    BuildingTable table;
    table.m_by_id.assign(kMaxId - kIdOffset + 1, -1);
    table.m_by_sprite.assign(kMaxSprite, -1);

    for (const auto& [nummer, r] : *records) {
        const std::int32_t id = r.integer("Id");
        if (id == 0) {
            continue;  // unused slot (filled with defaults by ObjFill)
        }
        if (id < static_cast<std::int32_t>(kIdOffset) || id > static_cast<std::int32_t>(kMaxId)) {
            return make_error(ErrorCode::InvalidFormat,
                              "HAUS #" + std::to_string(nummer) + " has invalid Id " + std::to_string(id));
        }
        BuildingType b;
        b.id = static_cast<std::uint32_t>(id);
        b.kind = r.integer("Kind");
        b.gfx = non_negative(r.integer("Gfx"));
        b.size_x = std::max(1U, non_negative(r.integer("Size", 0, 1)));
        b.size_y = std::max(1U, non_negative(r.integer("Size", 1, 1)));
        b.rotate = non_negative(r.integer("Rotate"));
        b.anim_count = non_negative(r.integer("AnimAnz"));
        b.anim_add = non_negative(r.integer("AnimAdd"));
        b.anim_time = r.integer("AnimTime");
        b.anim_frame = non_negative(r.integer("AnimFrame"));
        b.posoffs = r.integer("Posoffs");
        if (const cod::Field* einhoffs = r.field("Einhoffs"); einhoffs != nullptr && !einhoffs->values.empty()) {
            b.icon_height = einhoffs->values[0].real;
        }
        b.high = (r.integer("Highflg") & 1) != 0;
        b.draw_ground = (r.integer("Grundflg") & 1) != 0;
        b.build_gfx = non_negative(r.integer("Baugfx"));
        b.kreuz_base = r.integer("KreuzBase");
        b.rand_count = static_cast<std::uint16_t>(r.integer("RandAnz"));
        b.rand_add = static_cast<std::uint16_t>(r.integer("RandAdd"));
        if (const auto* prod = r.records("HAUS_PRODTYP"); prod != nullptr && !prod->empty()) {
            const auto& prodtyp = prod->begin()->second;
            b.prod_kind = prodtyp.integer("Kind");
            for (std::size_t i = 0; i < b.attached_figures.size(); ++i) {
                b.attached_figures[i] = static_cast<std::uint16_t>(std::max(0, prodtyp.integer("Rauchfignr", i)));
            }
            const auto tons = [&](const char* name) {
                const cod::Field* f = prodtyp.field(name);
                const double v = f != nullptr && !f->values.empty() ? f->values[0].real : 0.0;
                // The original truncates (_ftol); the values in haeuser.cod are exact multiples of 1/32.
                return v > 0.0 ? static_cast<std::uint32_t>(v * kStockUnitsPerTon) : 0U;
            };
            b.ware = static_cast<std::uint8_t>(non_negative(prodtyp.integer("Ware")));
            b.raw_ware = static_cast<std::uint8_t>(non_negative(prodtyp.integer("Rohstoff")));
            b.raw2_ware = static_cast<std::uint8_t>(non_negative(prodtyp.integer("Workstoff")));
            b.produce_amount = tons("Prodmenge");
            b.raw_amount = tons("Rohmenge");
            b.raw2_amount = tons("Workmenge");
            b.interval = non_negative(prodtyp.integer("Interval"));
            b.worker_figure = static_cast<std::uint16_t>(non_negative(prodtyp.integer("Figurnr")));
            b.worker_count = static_cast<std::uint8_t>(non_negative(prodtyp.integer("Figuranz")));
            b.max_stock = non_negative(prodtyp.integer("Maxlager")) * kStockUnitsPerTon;
            b.max_idle_cycles = non_negative(prodtyp.integer("Maxnorohst"));
            b.stock_animation = (prodtyp.integer("LagAniFlg") & 1) != 0;
            b.continuous_animation = (prodtyp.integer("Anicontflg") & 1) != 0;
        }

        // Duplicate ids exist in the original (three ruins); the first record wins, as in the
        // id -> record table the original builds while loading.
        const std::size_t slot = b.id - kIdOffset;
        if (table.m_by_id[slot] >= 0) {
            continue;
        }
        const auto index = static_cast<std::int32_t>(table.m_types.size());
        table.m_by_id[slot] = index;
        for (std::uint32_t k = 0; k < b.tile_count(); ++k) {
            if (b.gfx + k < kMaxSprite && table.m_by_sprite[b.gfx + k] < 0) {
                table.m_by_sprite[b.gfx + k] = index;
            }
        }
        table.m_types.push_back(b);
    }

    constexpr std::int32_t kKindMeer = 19;
    constexpr std::int32_t kProdKindOpenSea = 9;
    for (std::size_t i = 0; i < table.m_types.size(); ++i) {
        if (table.m_types[i].kind == kKindMeer && table.m_types[i].prod_kind == kProdKindOpenSea) {
            table.m_open_sea = static_cast<std::int32_t>(i);
            break;
        }
    }
    return table;
}

const BuildingType* BuildingTable::by_id(std::uint32_t id) const noexcept {
    if (id < kIdOffset || id > kMaxId) {
        return nullptr;
    }
    const std::int32_t index = m_by_id[id - kIdOffset];
    return index < 0 ? nullptr : &m_types[static_cast<std::size_t>(index)];
}

const BuildingType* BuildingTable::by_sprite(std::uint32_t sprite) const noexcept {
    if (sprite >= m_by_sprite.size()) {
        return nullptr;
    }
    const std::int32_t index = m_by_sprite[sprite];
    return index < 0 ? nullptr : &m_types[static_cast<std::size_t>(index)];
}

core::Result<BuildingTable> BuildingTable::load(const GameDirectory& game) {
    const auto path = game.resolve("haeuser.cod");
    if (!path) {
        return make_error(ErrorCode::NotFound, "haeuser.cod not found in the game directory");
    }
    auto bytes = core::read_file(*path);
    if (!bytes) {
        return bytes.error();
    }
    return from_cod(cod::parse(decode_cod(*bytes), cod_predefined_haeuser()));
}

}  // namespace o1602::assets
