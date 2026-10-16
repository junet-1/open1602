// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests against an original installation. They are skipped unless ANNO1602_DIR points to it:
//
//   ANNO1602_DIR="$HOME/Games/Heroic/Anno 1602" ctest --preset debug
//
// Expected values were taken from the GOG "Koenigsedition" (German).

#include <set>
#include <string>

#include "../test_framework.hpp"

#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/cod.hpp"
#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/assets/sprite_sheet.hpp"
#include "o1602/core/file.hpp"

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

// ---------------------------------------------------------------------------------------------
// .cod interpreter against the original files

namespace {

assets::cod::Document parse_original_cod(const assets::GameDirectory& game, const char* file,
                                         std::span<const assets::CodPredefinedSymbol> predefined) {
    const auto path = game.resolve(file);
    REQUIRE(path.has_value());
    auto bytes = core::read_file(*path);
    REQUIRE(bytes.ok());
    return assets::cod::parse(assets::decode_cod(*bytes), predefined);
}

std::set<std::string> undefined_symbols(const assets::cod::Document& doc) {
    std::set<std::string> names;
    for (const auto& d : doc.diagnostics) {
        const auto a = d.message.find('\'');
        const auto b = d.message.rfind('\'');
        if (d.message.starts_with("undefined symbol") && a != std::string::npos && b > a) {
            names.insert(d.message.substr(a + 1, b - a - 1));
        }
    }
    return names;
}

}  // namespace

TEST_CASE("data: haeuser.cod interprets with the predefined symbols") {
    const auto game = require_game();
    const auto doc = parse_original_cod(game, "haeuser.cod", assets::cod_predefined_haeuser());

    // Only the two references that are undefined in the original as well (see docs/re/bugs.md).
    CHECK(undefined_symbols(doc) == (std::set<std::string>{"WAV_BAECKER", "FAHNEKONTOR"}));
    CHECK(doc.diagnostics.size() == 2);
    CHECK(doc.symbols.size() < 650);  // capacity of the original symbol table

    const auto* haus = doc.root.records("HAUS");
    REQUIRE(haus != nullptr);
    CHECK(haus->size() == 500);  // ObjFill: 0,MAXHAUS

    const assets::cod::Object* meer = nullptr;
    const assets::cod::Object* baeckerei = nullptr;
    for (const auto& [nummer, record] : *haus) {
        if (record.integer("Id") == 21201) {  // IDMEER+0
            meer = &record;
        }
        if (record.integer("Id") == 20503) {  // IDHANDW+2
            baeckerei = &record;
        }
    }
    REQUIRE(meer != nullptr);
    CHECK(meer->integer("Gfx") == 758);  // GFXMEER+78 = (0+432+152+96)+78
    CHECK(meer->integer("Kind") == 19);  // MEER
    REQUIRE(baeckerei != nullptr);
    CHECK(baeckerei->integer("Size", 0) == 2);
    CHECK(baeckerei->integer("Size", 1) == 2);
    CHECK(baeckerei->records("HAUS_PRODTYP") != nullptr);
}

TEST_CASE("data: island tile ids of NORD/big.scp exist in haeuser.cod") {
    const auto game = require_game();
    const auto doc = parse_original_cod(game, "haeuser.cod", assets::cod_predefined_haeuser());
    std::set<std::int32_t> ids;
    for (const auto& [nummer, record] : *doc.root.records("HAUS")) {
        ids.insert(record.integer("Id"));
    }
    auto file = assets::ChunkFile::load(*game.resolve("nord/big.scp"));
    REQUIRE(file.ok());
    auto islands = assets::load_islands(*file);
    REQUIRE(islands.ok());
    std::size_t missing = 0;
    for (const auto& tile : (*islands)[0].layers[0]) {
        if (!ids.contains(static_cast<std::int32_t>(tile.cod_id()))) {
            ++missing;
        }
    }
    CHECK(missing == 0);
}

TEST_CASE("data: figuren.cod interprets with the predefined symbols") {
    const auto game = require_game();
    const auto doc = parse_original_cod(game, "figuren.cod", assets::cod_predefined_figuren());
    CHECK(undefined_symbols(doc) == (std::set<std::string>{"FIGTYP_KANONTURM", "FIGTYP_PIRATSHIP"}));
    CHECK(doc.diagnostics.size() == 2);
    const auto* figur = doc.root.records("FIGUR");
    REQUIRE(figur != nullptr);
    CHECK(figur->size() == 119);  // ObjFill: 0,MAXFIGUR

    std::size_t soldat_lines = 0;
    for (const auto& [name, values] : doc.root.history) {
        if (name == "Soldat") {
            ++soldat_lines;
        }
    }
    CHECK(soldat_lines == 4);  // Schwert, Kavalerie, Musketier, Kanonier
}
