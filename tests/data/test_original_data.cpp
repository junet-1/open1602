// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests against an original installation. They are skipped unless ANNO1602_DIR points to it:
//
//   ANNO1602_DIR="$HOME/Games/Heroic/Anno 1602" ctest --preset debug
//
// Expected values were taken from the GOG "Koenigsedition" (German).

#include <string>

#include "../test_framework.hpp"

#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/cod.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/assets/sprite_sheet.hpp"

using namespace o1602;

namespace {

assets::GameDirectory require_game() {
    auto dir = assets::GameDirectory::from_environment();
    if (!dir) {
        SKIP("ANNO1602_DIR not set");
    }
    return std::move(dir).value();
}

void check_sheet_decodes(const assets::GameDirectory& game, const char* file, std::size_t expected_count) {
    const auto path = game.resolve(file);
    REQUIRE(path.has_value());
    auto sheet = assets::SpriteSheet::load(*path);
    REQUIRE(sheet.ok());
    CHECK(sheet->size() == expected_count);
    std::size_t failures = 0;
    for (std::size_t i = 0; i < sheet->size(); ++i) {
        if (!sheet->decode(i).ok()) {
            ++failures;
        }
    }
    CHECK(failures == 0);
}

}  // namespace

TEST_CASE("data: palette STADTFLD.COL") {
    const auto game = require_game();
    auto palette = assets::Palette::load(*game.resolve("toolgfx/stadtfld.col"));
    REQUIRE(palette.ok());
    CHECK((*palette)[0] == (assets::Rgba{0, 0, 0, 255}));
}

TEST_CASE("data: all sprites of the main BSH files decode") {
    const auto game = require_game();
    check_sheet_decodes(game, "gfx/stadtfld.bsh", 5964);
    check_sheet_decodes(game, "mgfx/stadtfld.bsh", 5964);
    check_sheet_decodes(game, "sgfx/stadtfld.bsh", 5964);
    check_sheet_decodes(game, "gfx/ship.bsh", 368);
    check_sheet_decodes(game, "gfx/traeger.bsh", 1784);
    check_sheet_decodes(game, "toolgfx/start.bsh", 90);
}

TEST_CASE("data: haeuser.cod decodes to readable text") {
    const auto game = require_game();
    auto text = assets::load_cod_utf8(*game.resolve("haeuser.cod"));
    REQUIRE(text.ok());
    CHECK(text->find("IDBODEN") != std::string::npos);
    CHECK(text->find("Objekt: HAUS") != std::string::npos);
}

TEST_CASE("data: island template NORD/big.scp") {
    const auto game = require_game();
    auto file = assets::ChunkFile::load(*game.resolve("nord/big.scp"));
    REQUIRE(file.ok());
    auto islands = assets::load_islands(*file);
    REQUIRE(islands.ok());
    REQUIRE(islands->size() == 1);
    const auto& island = (*islands)[0];
    CHECK(island.header.width == 70);
    CHECK(island.header.height == 60);
    REQUIRE(island.layers.size() == 1);
    for (const auto& tile : island.layers[0]) {
        CHECK(tile.x < island.header.width);
        CHECK(tile.y < island.header.height);
    }
}
