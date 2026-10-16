// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests against an original installation. They are skipped unless ANNO1602_DIR points to it:
//
//   ANNO1602_DIR="$HOME/Games/Heroic/Anno 1602" ctest --preset debug
//
// Expected values were taken from the GOG "Koenigsedition" (German).

#include <algorithm>
#include <set>
#include <string>

#include "../test_framework.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/cod.hpp"
#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/assets/savegame.hpp"
#include "o1602/assets/scenario.hpp"
#include "o1602/assets/sprite_sheet.hpp"
#include "o1602/core/file.hpp"
#include "o1602/render/animation_clock.hpp"
#include "o1602/render/island_renderer.hpp"
#include "o1602/render/sprite_atlas.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/island_map.hpp"
#include "o1602/sim/world.hpp"

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

// ---------------------------------------------------------------------------------------------
// Building table, island maps and rendering against the original data

namespace {

assets::BuildingTable original_buildings(const assets::GameDirectory& game) {
    const auto doc = parse_original_cod(game, "haeuser.cod", assets::cod_predefined_haeuser());
    auto table = assets::BuildingTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

}  // namespace

TEST_CASE("data: building table from haeuser.cod") {
    const auto game = require_game();
    const auto table = original_buildings(game);
    CHECK(table.types().size() == 495);  // distinct non-zero ids; 3 ids occur twice in the original
    const auto* meer = table.by_id(21201);
    REQUIRE(meer != nullptr);
    CHECK(meer->gfx == 758);
    CHECK(meer->anim_count == 6);
    CHECK(table.by_sprite(758) == meer);
    const auto* baeckerei = table.by_id(20503);
    REQUIRE(baeckerei != nullptr);
    CHECK(baeckerei->rotate == baeckerei->tile_count());  // 2x2 building: 4 sprites per rotation
    CHECK(table.by_sprite(baeckerei->gfx + 3) == baeckerei);
}

TEST_CASE("data: islands of a scenario build complete maps and render without missing sprites") {
    const auto game = require_game();
    const auto table = original_buildings(game);
    auto sheet = assets::SpriteSheet::load(*game.resolve("gfx/stadtfld.bsh"));
    REQUIRE(sheet.ok());
    auto palette = assets::Palette::load(*game.resolve("toolgfx/stadtfld.col"));
    REQUIRE(palette.ok());

    auto file = assets::ChunkFile::load(*game.resolve("szenes/Das Atoll.szs"));
    REQUIRE(file.ok());
    auto islands = assets::load_islands(*file);
    REQUIRE(islands.ok());
    REQUIRE(islands->size() == 34);

    CHECK((*islands)[0].header.template_path() == "Nord/big12.scp");
    CHECK((*islands)[2].header.template_path() == "Sued/big06.scp");

    // Islands store only deviations from their template; load it like the original loader.
    const auto load_template = [&](const assets::Island& island) -> std::vector<assets::Island> {
        if (island.header.template_path().empty()) {
            return {};
        }
        const auto path = game.resolve(island.header.template_path());
        REQUIRE(path.has_value());
        auto tfile = assets::ChunkFile::load(*path);
        REQUIRE(tfile.ok());
        auto tislands = assets::load_islands(*tfile);
        REQUIRE(tislands.ok());
        REQUIRE(!tislands->empty());
        return std::move(tislands).value();
    };

    std::size_t warnings_total = 0;
    std::size_t sub_tile_errors = 0;
    std::size_t empty_cells = 0;
    for (const auto& island : *islands) {
        const auto base = load_template(island);
        std::vector<std::string> warnings;
        const auto map = sim::IslandMap::from_island(island, table, &warnings, base.empty() ? nullptr : &base[0]);
        warnings_total += warnings.size();
        for (std::uint32_t y = 0; y < map.height(); ++y) {
            for (std::uint32_t x = 0; x < map.width(); ++x) {
                if (map.top(x, y).empty()) {
                    if (!base.empty()) {
                        ++empty_cells;  // templates are complete (incl. sea), so no gaps allowed
                    }
                    continue;
                }
                const auto sub = map.sub_tile(table, x, y);
                if (sub.building == nullptr || sub.origin_x < 0 || sub.origin_y < 0 ||
                    table.by_sprite(
                        map.top(static_cast<std::uint32_t>(sub.origin_x), static_cast<std::uint32_t>(sub.origin_y))
                            .sprite) != sub.building) {
                    ++sub_tile_errors;
                }
            }
        }
    }
    CHECK(warnings_total == 0);
    CHECK(sub_tile_errors == 0);
    CHECK(empty_cells == 0);

    // Render the first island at full zoom.
    const auto base0 = load_template((*islands)[0]);
    REQUIRE(!base0.empty());
    const auto map = sim::IslandMap::from_island((*islands)[0], table, nullptr, &base0[0]);
    const auto commands = render::build_draw_list(map, table, render::RenderOptions{});
    std::size_t missing = 0;
    const auto image = render::rasterize(commands, *sheet, *palette, {}, &missing);
    CHECK(missing == 0);
    CHECK(commands.size() == 4200);  // 70 x 60, complete with template
    CHECK(image.width > 0);
    CHECK(image.height > 0);
}

TEST_CASE("data: all island header variants (INSEL3/4/5) load") {
    const auto game = require_game();
    for (const char* f : {"nord/big03.scp", "sued/big11.scp", "sued/big04.scp"}) {
        auto file = assets::ChunkFile::load(*game.resolve(f));
        REQUIRE(file.ok());
        auto islands = assets::load_islands(*file);
        REQUIRE(islands.ok());
        CHECK(islands->size() == 1);
    }
}

TEST_CASE("data: world of a scenario loads and draws without missing sprites") {
    const auto game = require_game();
    auto buildings = assets::BuildingTable::load(game);
    REQUIRE(buildings.ok());
    REQUIRE(buildings->open_sea() != nullptr);
    CHECK(buildings->open_sea()->id == 21201);

    const auto scenario = game.resolve("szenes/das atoll.szs");
    REQUIRE(scenario.has_value());
    auto file = assets::ChunkFile::load(*scenario);
    REQUIRE(file.ok());
    std::vector<std::string> warnings;
    auto world = sim::load_world(*file, *buildings, game, &warnings);
    REQUIRE(world.ok());
    CHECK(warnings.empty());
    CHECK(world->islands().size() == 34);

    // Whole world at the smallest zoom, all four view rotations: every sprite exists in the atlas.
    auto sheet = assets::SpriteSheet::load(*game.resolve(render::terrain_sheet_path(render::Zoom::Small)));
    REQUIRE(sheet.ok());
    auto palette = assets::Palette::load(*game.resolve(render::kTerrainPalettePath));
    REQUIRE(palette.ok());
    auto atlas = render::SpriteAtlas::build(*sheet, *palette);
    REQUIRE(atlas.ok());
    const render::AnimationClock clock(*buildings);
    for (std::uint8_t rotation = 0; rotation < 4; ++rotation) {
        render::WorldView view;
        view.zoom = render::Zoom::Small;
        view.rotation = rotation;
        render::Viewport all;
        render::world_pixel_size(view, all.width, all.height);
        const auto commands =
            render::build_world_draw_list(*world, *buildings, clock, view, all,
                                          render::SpriteExtent{atlas->max_sprite_width(), atlas->max_sprite_height()});
        CHECK(commands.size() >= std::size_t{500} * 350);
        std::size_t missing = 0;
        for (const auto& c : commands) {
            if (atlas->rect(c.sprite).empty()) {
                ++missing;
            }
        }
        CHECK(missing == 0);
    }
}

TEST_CASE("data: players, colours and ships of scenarios") {
    const auto game = require_game();
    auto atoll = assets::ChunkFile::load(*game.resolve("szenes/das atoll.szs"));
    REQUIRE(atoll.ok());
    auto players = assets::load_players(*atoll);
    REQUIRE(players.ok());
    REQUIRE(players->size() == 7);
    using K = assets::PlayerKind;
    using C = assets::PlayerColor;
    const K kinds[] = {K::Human, K::Computer, K::Computer, K::Computer, K::Trader, K::Pirate, K::Natives};
    const C colors[] = {C::Red, C::Blue, C::Yellow, C::White, C::Trader, C::Pirate, C::Natives};
    for (std::size_t i = 0; i < 7; ++i) {
        CHECK((*players)[i].slot == i);
        CHECK((*players)[i].kind == kinds[i]);
        CHECK((*players)[i].color == colors[i]);
    }
    CHECK((*players)[0].money == 10000);

    auto ships = assets::load_ships(*atoll);
    REQUIRE(ships.ok());
    REQUIRE(ships->size() == 8);
    CHECK((*ships)[7].name == "Patria");
    CHECK((*ships)[7].x == 249);
    CHECK((*ships)[7].y == 156);
    CHECK((*ships)[7].owner == 0);
    CHECK((*ships)[0].owner == 5);    // pirate ships
    CHECK((*ships)[0].figure == 31);  // PIRAT

    // Endlosspiel00: the human player has the blue flag, the first computer player the red one.
    auto endless = assets::ChunkFile::load(*game.resolve("szenes/endlosspiel00.szs"));
    REQUIRE(endless.ok());
    auto endless_players = assets::load_players(*endless);
    REQUIRE(endless_players.ok());
    CHECK((*endless_players)[0].color == C::Blue);
    CHECK((*endless_players)[1].color == C::Red);

    // Every ship refers to a ship figure; every flag colour has its sprites in SHIP.BSH.
    auto figures = assets::FigureTable::load(game);
    REQUIRE(figures.ok());
    for (const auto& ship : *ships) {
        const auto* f = figures->get(ship.figure);
        REQUIRE(f != nullptr);
        CHECK(f->sheet == assets::FigureSheet::Ship);
    }
    for (std::uint8_t c = 0; c < 5; ++c) {
        const auto* flag = figures->flag_for_color(c);
        REQUIRE(flag != nullptr);
        CHECK(flag->gfx == 192U + 8U * c);
    }
    CHECK(figures->flag_for_color(5) == nullptr);
    CHECK(figures->flag_for_color(6) == nullptr);

    // The base game's banners carry exactly these flags.
    const auto banners = render::BannerTable::original(&*figures);
    REQUIRE(banners.size() == 7);
    for (sim::BannerId b = 0; b < 5; ++b) {
        REQUIRE(banners.get(b)->flag_gfx.has_value());
        CHECK(*banners.get(b)->flag_gfx == 192U + 8U * b);
    }
    CHECK(!banners.get(5)->flag_gfx.has_value());
    CHECK(!banners.get(6)->flag_gfx.has_value());
}

TEST_CASE("data: warehouse, market and tower flags of a scenario") {
    const auto game = require_game();
    auto buildings = assets::BuildingTable::load(game);
    REQUIRE(buildings.ok());
    auto figures = assets::FigureTable::load(game);
    REQUIRE(figures.ok());
    auto file = assets::ChunkFile::load(*game.resolve("szenes/das atoll.szs"));
    REQUIRE(file.ok());
    auto world = sim::load_world(*file, *buildings, game, nullptr);
    REQUIRE(world.ok());

    const auto flags = render::collect_building_flags(*world, *buildings, *figures);
    // "Das Atoll": the three computer players start with warehouses and markets, the pirates
    // with a fortress full of towers; the natives' buildings and the pirate warehouse fly none.
    std::size_t per_owner[8] = {};
    for (const auto& f : flags) {
        const auto* fig = figures->get(f.figure);
        REQUIRE(fig != nullptr);
        CHECK(fig->sheet == assets::FigureSheet::Ship);
        CHECK(fig->gfx == 192U);  // FAHNEMARKT and its copies: one 8-frame ANIM per colour
        ++per_owner[f.owner & 7U];
    }
    CHECK(per_owner[0] == 0);
    CHECK(per_owner[1] == 8);
    CHECK(per_owner[2] == 7);
    CHECK(per_owner[3] == 7);
    CHECK(per_owner[5] == 61);
    CHECK(per_owner[6] == 0);
}

TEST_CASE("data: savegames of the installation (if any) load like scenarios") {
    const auto game = require_game();
    const auto dat = game.resolve("Game.dat");
    if (!dat) {
        SKIP("no Game.dat (game never started)");
    }
    auto bytes = core::read_file(*dat);
    REQUIRE(bytes.ok());
    const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    auto buildings = assets::BuildingTable::load(game);
    REQUIRE(buildings.ok());
    std::size_t loaded = 0;
    for (const auto& slot : assets::parse_savegame_slots(text)) {
        const auto path = game.resolve(slot.file);
        if (!path) {
            continue;
        }
        auto file = assets::ChunkFile::load(*path);
        REQUIRE(file.ok());
        std::vector<std::string> warnings;
        auto world = sim::load_world(*file, *buildings, game, &warnings);
        REQUIRE(world.ok());
        CHECK(warnings.empty());
        CHECK(!world->islands().empty());
        REQUIRE(world->players().size() == 7);
        CHECK(world->players()[0].role == sim::PlayerRole::Human);
        CHECK(!world->players()[0].name.empty());  // savegames store the human player's name

        // Cities and warehouses: every city of a settler player has a warehouse whose position
        // is a warehouse building on that island.
        auto state = assets::load_game_state(*file);
        REQUIRE(state.ok());
        CHECK(!world->cities().empty());
        for (const auto& w : state->warehouses) {
            const auto it = std::find_if(world->cities().begin(), world->cities().end(), [&](const sim::City& c) {
                return world->islands()[c.island].header.number == w.island && c.index == w.city;
            });
            REQUIRE(it != world->cities().end());
            if (it->owner >= 4) {
                continue;  // natives, pirates and the trader have no regular warehouse
            }
            const auto& map = world->islands()[it->island].map;
            REQUIRE(w.x < map.width());
            REQUIRE(w.y < map.height());
            const sim::SubTile sub = map.sub_tile(*buildings, w.x, w.y);
            REQUIRE(sub.building != nullptr);
            CHECK(sub.building->prod_kind == 8);  // HAUS_PRODTYP Kind KONTOR
            CHECK(map.top(w.x, w.y).city == w.city);
        }
        ++loaded;
    }
    if (loaded == 0) {
        SKIP("no savegames");
    }
}
