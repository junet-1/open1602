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

constexpr std::uint32_t kMaxId = 23500;        // Haus_GetById (0x004630F0) accepts 20000..23500
constexpr std::uint32_t kMaxSprite = 0x2000;   // 13 bits in the island grid
constexpr std::int32_t kProdKindGrowing = 10;  // HAUS_PRODTYP Kind ROHSTWACHS

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
        b.record = static_cast<std::uint32_t>(std::max(0, nummer));
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
        b.door_on_edge = (r.integer("Tuerflg") & 1) != 0;
        for (std::size_t i = 0; i < b.walk_speed.size(); ++i) {
            // 0x00462834: trunc(value * 32 / 100), at most 126. Zero would mean infinite speed
            // (a division by zero in the original); Open1602 keeps at least 1.
            const std::int32_t percent = r.integer("Wegspeed", i, 100);
            b.walk_speed[i] = static_cast<std::uint8_t>(std::clamp(percent * 32 / 100, 1, 126));
        }
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
            b.radius = static_cast<std::uint8_t>(std::min(255U, non_negative(prodtyp.integer("Radius"))));
            b.max_stock = non_negative(prodtyp.integer("Maxlager")) * kStockUnitsPerTon;
            b.max_idle_cycles = non_negative(prodtyp.integer("Maxnorohst"));
            b.stock_animation = (prodtyp.integer("LagAniFlg") & 1) != 0;
            b.continuous_animation = (prodtyp.integer("Anicontflg") & 1) != 0;
            b.withered = (prodtyp.integer("Doerrflg") & 1) != 0;
            b.ore_mountain = static_cast<std::uint8_t>(non_negative(prodtyp.integer("Erzbergnr")));
        }

        if (b.prod_kind == kProdKindGrowing && b.anim_count > 0) {
            // 0x00462F1D: the number of k in 1..32 with 40 s + 7 s * k <= Interval / AnimAnz,
            // clamped to 31 when an entry is added (0x0047C760).
            const std::uint64_t stage_ms = std::uint64_t{b.interval} * 1000U / b.anim_count;
            std::uint32_t cls = 0;
            for (std::uint32_t k = 1; k <= 32; ++k) {
                cls += 40000U + 7000U * k <= stage_ms ? 1U : 0U;
            }
            b.growth_class = static_cast<std::uint8_t>(std::min(cls, 31U));
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
        if (b.record >= table.m_by_record.size()) {
            table.m_by_record.resize(std::size_t{b.record} + 1, -1);
        }
        table.m_by_record[b.record] = index;
        table.m_types.push_back(b);
    }

    // Classes of inhabitants and their needs (0x00460750: k = trunc(v * 8192) / 600).
    const auto per_tick = [](double v) -> std::uint32_t {
        return v > 0.0 ? static_cast<std::uint32_t>(static_cast<std::int64_t>(v * 8192.0) / 600) : 0U;
    };
    if (const cod::Field* food = haeuser.root.field("Nahrung"); food != nullptr && !food->values.empty()) {
        table.m_food_need = per_tick(food->values[0].real);
    }
    if (const cod::Symbol* nahrung = haeuser.symbols.find("NAHRUNG"); nahrung != nullptr && nahrung->defined) {
        table.m_food_ware = static_cast<std::uint8_t>(nahrung->values[0].integer);
    }
    if (const auto* groups = haeuser.root.records("BGRUPPE")) {
        for (const auto& [nummer, g] : *groups) {
            if (nummer < 0 || nummer > 255) {
                continue;
            }
            if (static_cast<std::size_t>(nummer) >= table.m_classes.size()) {
                table.m_classes.resize(static_cast<std::size_t>(nummer) + 1);
            }
            ResidentClass& c = table.m_classes[static_cast<std::size_t>(nummer)];
            c.share = non_negative(g.integer("Prozent")) * 128U / 100U;
            c.max_per_house = non_negative(g.integer("Maxwohn"));
            if (const cod::Field* tax = g.field("Steuer"); tax != nullptr && !tax->values.empty()) {
                c.tax = tax->values[0].real > 0.0 ? static_cast<std::uint32_t>(tax->values[0].real * 16.0 / 3.0) : 0U;
            }
            c.hiscore = non_negative(g.integer("Hiscore"));
            if (const auto* wares = g.records("BGRUPPE_WARE")) {
                for (const auto& [n, list] : *wares) {
                    for (const auto& [name, values] : list.history) {
                        if (name != "Ware" || values.size() < 2) {
                            continue;
                        }
                        const auto ware = static_cast<std::uint8_t>(std::max(0, values[0].integer));
                        const std::uint32_t amount = per_tick(values[1].real);
                        if (ware == 0 || amount == 0) {
                            continue;
                        }
                        auto it = std::find_if(c.needs.begin(), c.needs.end(),
                                               [&](const ResidentClass::Need& need) { return need.ware == ware; });
                        if (it != c.needs.end()) {
                            it->amount = amount;  // a later line replaces an earlier one
                        } else {
                            c.needs.push_back(ResidentClass::Need{ware, amount});
                        }
                    }
                }
            }
        }
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

const BuildingType* BuildingTable::by_record(std::uint32_t record) const noexcept {
    if (record >= m_by_record.size()) {
        return nullptr;
    }
    const std::int32_t index = m_by_record[record];
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
