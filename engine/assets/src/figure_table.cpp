// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/figure_table.hpp"

#include <algorithm>

#include "o1602/assets/cod.hpp"
#include "o1602/assets/cod_predefined.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/savegame.hpp"
#include "o1602/core/file.hpp"

namespace o1602::assets {

using core::ErrorCode;
using core::make_error;

core::Result<FigureTable> FigureTable::from_cod(const cod::Document& figuren) {
    const auto* records = figuren.root.records("FIGUR");
    if (records == nullptr) {
        return make_error(ErrorCode::InvalidFormat, "figuren.cod contains no FIGUR records");
    }
    constexpr std::int32_t kMaxFigures = 1024;
    FigureTable table;
    for (const auto& [nummer, r] : *records) {
        if (nummer < 0 || nummer >= kMaxFigures) {
            return make_error(ErrorCode::InvalidFormat, "FIGUR number out of range: " + std::to_string(nummer));
        }
        if (static_cast<std::size_t>(nummer) >= table.m_types.size()) {
            table.m_types.resize(static_cast<std::size_t>(nummer) + 1);
        }
        FigureType f;
        f.number = static_cast<std::uint32_t>(nummer);
        f.kind = r.integer("Kind");
        f.gfx = static_cast<std::uint32_t>(std::max(0, r.integer("Gfx")));
        f.sheet = static_cast<FigureSheet>(std::clamp(r.integer("Blocknr"), 0, 6));
        f.shadow = static_cast<std::uint32_t>(std::max(0, r.integer("Schatten")));
        f.rotate = static_cast<std::uint32_t>(std::max(0, r.integer("Rotate")));
        f.posoffs = {r.integer("Posoffs", 0), r.integer("Posoffs", 1)};
        f.speed = static_cast<std::uint32_t>(std::max(0, r.integer("Speed")));
        f.speed_type = static_cast<std::uint8_t>(std::clamp(r.integer("Speedtyp"), 0, 3));
        f.max_carry = static_cast<std::uint32_t>(std::max(0, r.integer("Maxtrag"))) * kStockUnitsPerTon;
        if (const cod::Field* work = r.field("Worktime"); work != nullptr && !work->values.empty()) {
            f.work_time_ms = static_cast<std::uint32_t>(std::max(0.0, work->values[0].real * 1000.0 + 0.5));
        }
        if (const cod::Field* fahn = r.field("Fahnoffs")) {
            for (std::size_t i = 0; i < f.fahnoffs.size() && i < fahn->values.size(); ++i) {
                f.fahnoffs[i] = fahn->values[i].real;
            }
        }
        if (const auto* anims = r.records("ANIM")) {
            for (const auto& [index, a] : *anims) {
                if (index < 0 || index >= 64) {
                    continue;  // the original reserves room for a few dozen per figure
                }
                if (static_cast<std::size_t>(index) >= f.anims.size()) {
                    f.anims.resize(static_cast<std::size_t>(index) + 1);
                }
                FigureAnim anim;
                anim.kind = static_cast<AnimKind>(std::clamp(a.integer("Kind"), 0, 3));
                anim.offset = static_cast<std::uint32_t>(std::max(0, a.integer("AnimOffs")));
                anim.add = static_cast<std::uint32_t>(std::max(0, a.integer("AnimAdd", 0, 1)));
                anim.count = static_cast<std::uint32_t>(std::max(1, a.integer("AnimAnz", 0, 1)));
                anim.repeat = static_cast<std::uint32_t>(std::max(1, a.integer("AnimRept", 0, 1)));
                anim.speed_ms = static_cast<std::uint32_t>(std::max(1, a.integer("AnimSpeed", 0, 100)));
                anim.rotate = a.field("Rotate") != nullptr
                                  ? static_cast<std::uint32_t>(std::max(0, a.integer("Rotate")))
                                  : f.rotate;
                if (const cod::Field* next = a.field("AnimNr")) {
                    for (const cod::Value& v : next->values) {
                        anim.next.push_back(static_cast<std::uint8_t>(std::clamp(v.integer, 0, 255)));
                    }
                }
                anim.defined = true;
                f.anims[static_cast<std::size_t>(index)] = std::move(anim);
            }
        }
        f.defined = true;
        table.m_types[static_cast<std::size_t>(nummer)] = std::move(f);
    }
    return table;
}

std::uint32_t figure_sprite(const FigureType& type, std::size_t anim, std::uint32_t frame, std::uint8_t direction,
                            std::uint8_t view_rotation) noexcept {
    const std::uint32_t dir = (direction + 2U * view_rotation) & 7U;
    if (const FigureAnim* a = type.anim(anim)) {
        return type.gfx + a->offset + a->add * frame + dir * a->rotate;
    }
    return type.gfx + dir * type.rotate;
}

core::Result<FigureTable> FigureTable::load(const GameDirectory& game) {
    const auto path = game.resolve("figuren.cod");
    if (!path) {
        return make_error(ErrorCode::NotFound, "figuren.cod not found in the game directory");
    }
    auto bytes = core::read_file(*path);
    if (!bytes) {
        return bytes.error();
    }
    return from_cod(cod::parse(decode_cod(*bytes), cod_predefined_figuren()));
}

}  // namespace o1602::assets
