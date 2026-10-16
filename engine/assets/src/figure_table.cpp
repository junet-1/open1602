// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/figure_table.hpp"

#include <algorithm>

#include "o1602/assets/cod.hpp"
#include "o1602/assets/cod_predefined.hpp"
#include "o1602/assets/game_directory.hpp"
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
        f.rotate = static_cast<std::uint32_t>(std::max(0, r.integer("Rotate")));
        f.posoffs = {r.integer("Posoffs", 0), r.integer("Posoffs", 1)};
        if (const cod::Field* fahn = r.field("Fahnoffs")) {
            for (std::size_t i = 0; i < f.fahnoffs.size() && i < fahn->values.size(); ++i) {
                f.fahnoffs[i] = fahn->values[i].real;
            }
        }
        f.defined = true;
        table.m_types[static_cast<std::size_t>(nummer)] = f;
    }
    return table;
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
