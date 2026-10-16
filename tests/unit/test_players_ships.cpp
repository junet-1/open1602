// SPDX-License-Identifier: GPL-3.0-or-later
// Players (PLAYER4), ships (SHIP4), figure table and their drawing (docs/formats/gam.md).

#include <algorithm>
#include <cstring>

#include "../test_framework.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/assets/scenario.hpp"
#include "o1602/assets/sprite_sheet.hpp"
#include "o1602/render/animation_clock.hpp"
#include "o1602/render/sprite_atlas.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;

namespace {

void put_u16(std::vector<std::uint8_t>& out, std::size_t at, std::uint16_t v) {
    out[at] = static_cast<std::uint8_t>(v);
    out[at + 1] = static_cast<std::uint8_t>(v >> 8);
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

void put_chunk(std::vector<std::uint8_t>& file, const char* name, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> header(16, 0);
    std::memcpy(header.data(), name, std::strlen(name));
    file.insert(file.end(), header.begin(), header.end());
    put_u32(file, static_cast<std::uint32_t>(payload.size()));
    file.insert(file.end(), payload.begin(), payload.end());
}

/// BSH with solid sprites of the given sizes (palette index 1).
assets::SpriteSheet make_sheet(const std::vector<std::pair<std::uint32_t, std::uint32_t>>& sizes) {
    std::vector<std::vector<std::uint8_t>> records;
    for (const auto& [w, h] : sizes) {
        std::vector<std::uint8_t> rle;
        for (std::uint32_t y = 0; y < h; ++y) {
            rle.push_back(0);
            rle.push_back(static_cast<std::uint8_t>(w));
            rle.insert(rle.end(), w, 1);
            rle.push_back(y + 1 == h ? 0xFF : 0xFE);
        }
        std::vector<std::uint8_t> rec;
        put_u32(rec, w);
        put_u32(rec, h);
        put_u32(rec, 1);
        put_u32(rec, static_cast<std::uint32_t>((16 + rle.size() + 3) & ~std::size_t{3}));
        rec.insert(rec.end(), rle.begin(), rle.end());
        rec.resize((rec.size() + 3) & ~std::size_t{3}, 0);
        records.push_back(std::move(rec));
    }
    std::vector<std::uint8_t> payload;
    auto offset = static_cast<std::uint32_t>(4 * records.size());
    for (const auto& rec : records) {
        put_u32(payload, offset);
        offset += static_cast<std::uint32_t>(rec.size());
    }
    for (const auto& rec : records) {
        payload.insert(payload.end(), rec.begin(), rec.end());
    }
    std::vector<std::uint8_t> file;
    put_chunk(file, "BSH", payload);
    auto chunks = assets::ChunkFile::from_bytes(file);
    auto sheet = assets::SpriteSheet::from_chunk_file(std::move(chunks).value());
    return std::move(sheet).value();
}

assets::Palette make_palette() {
    std::vector<std::uint8_t> payload;
    for (int i = 0; i < 256; ++i) {
        payload.insert(payload.end(), {static_cast<std::uint8_t>(i), 0, 0, 0});
    }
    std::vector<std::uint8_t> file;
    put_chunk(file, "COL", payload);
    auto chunks = assets::ChunkFile::from_bytes(file);
    return std::move(assets::Palette::from_chunk_file(*chunks)).value();
}

assets::FigureTable make_figures() {
    const assets::CodPredefinedSymbol types[] = {{"FIGUR", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: FIGUR\n"
        "  Nummer: 21\n  Gfx: 0\n  Blocknr: 2\n  Rotate: 1\n  Posoffs: -19, 34\n  Fahnoffs: 0, 0, 2.0\n"
        "  Nummer: 106\n  Gfx: 8\n  Blocknr: 2\n  Rotate: 8\n  Posoffs: 0, 0\n"
        "EndObj\n",
        types);
    auto table = assets::FigureTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

}  // namespace

TEST_CASE("scenario: PLAYER4 and SHIP4 records") {
    std::vector<std::uint8_t> players(2 * 0x430, 0);
    put_u16(players, 0, 10000);
    players[4] = 0x00;  // human
    players[5] = 0;
    players[6] = 1;
    players[7] = 1;  // blue
    std::memcpy(&players[0x3C0], "Anna", 4);
    players[0x430 + 0] = 0x50;  // 50000 = 0xC350
    players[0x430 + 1] = 0xC3;
    players[0x430 + 4] = 0x0E;  // pirate
    players[0x430 + 5] = 1;
    players[0x430 + 7] = 4;

    std::vector<std::uint8_t> ships(0x1B4, 0);
    std::memcpy(ships.data(), "Patria", 6);
    put_u16(ships, 0x1C, 249);
    put_u16(ships, 0x1E, 156);
    put_u16(ships, 0x48, 21);
    ships[0x4B] = 0;
    ships[0x4D] = 3;

    std::vector<std::uint8_t> file;
    put_chunk(file, "PLAYER4", players);
    put_chunk(file, "SHIP4", ships);
    auto chunks = assets::ChunkFile::from_bytes(file);
    REQUIRE(chunks.ok());

    auto p = assets::load_players(*chunks);
    REQUIRE(p.ok());
    REQUIRE(p->size() == 2);
    CHECK((*p)[0].money == 10000);
    CHECK((*p)[0].kind == assets::PlayerKind::Human);
    CHECK((*p)[0].color == assets::PlayerColor::Blue);
    CHECK((*p)[0].name == "Anna");
    CHECK((*p)[1].money == 50000);
    CHECK((*p)[1].kind == assets::PlayerKind::Pirate);
    CHECK((*p)[1].slot == 1);
    CHECK((*p)[1].color == assets::PlayerColor::Pirate);

    auto s = assets::load_ships(*chunks);
    REQUIRE(s.ok());
    REQUIRE(s->size() == 1);
    CHECK((*s)[0].name == "Patria");
    CHECK((*s)[0].x == 249);
    CHECK((*s)[0].y == 156);
    CHECK((*s)[0].figure == 21);
    CHECK((*s)[0].owner == 0);
    CHECK((*s)[0].direction == 3);

    // Truncated records are an error, missing chunks just mean "none".
    std::vector<std::uint8_t> bad;
    put_chunk(bad, "PLAYER4", std::vector<std::uint8_t>(100, 0));
    auto bad_chunks = assets::ChunkFile::from_bytes(bad);
    CHECK(!assets::load_players(*bad_chunks).ok());
    CHECK(assets::load_ships(*bad_chunks).ok());
    CHECK(assets::load_ships(*bad_chunks)->empty());
}

TEST_CASE("figure table: fields and flag lookup by player colour") {
    const auto figures = make_figures();
    const auto* ship = figures.get(21);
    REQUIRE(ship != nullptr);
    CHECK(ship->sheet == assets::FigureSheet::Ship);
    CHECK(ship->rotate == 1);
    CHECK(ship->posoffs[0] == -19);
    CHECK(ship->fahnoffs[2] == 2.0);
    CHECK(figures.get(22) == nullptr);
    CHECK(figures.flag_for_color(0) == figures.get(106));
    CHECK(figures.flag_for_color(1) == nullptr);  // FAHNE2 not defined in this test table
    CHECK(figures.flag_for_color(5) == nullptr);  // natives: no flag
}

TEST_CASE("sprite atlas: several sheets get consecutive index ranges") {
    const auto a = make_sheet({{4, 2}, {3, 3}});
    const auto b = make_sheet({{5, 1}});
    const auto palette = make_palette();
    const assets::SpriteSheet* sheets[] = {&a, &b};
    auto atlas = render::SpriteAtlas::build(sheets, palette, 64);
    REQUIRE(atlas.ok());
    CHECK(atlas->sprite_count() == 3);
    CHECK(atlas->sheet_base(0) == 0);
    CHECK(atlas->sheet_base(1) == 2);
    CHECK(atlas->rect(2).width == 5);
    CHECK(atlas->rect(2).height == 1);
}

TEST_CASE("world renderer: ship hull, owner flag and row") {
    // Sheets: terrain (1 sprite), ships: 8 hull directions 20x10, 8 flag frames 6x4.
    const auto terrain = make_sheet({{4, 2}});
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ship_sizes(8, {20, 10});
    ship_sizes.insert(ship_sizes.end(), 8, {6, 4});
    const auto ships = make_sheet(ship_sizes);
    const auto palette = make_palette();
    const assets::SpriteSheet* sheets[] = {&terrain, &ships};
    auto atlas = render::SpriteAtlas::build(sheets, palette, 256);
    REQUIRE(atlas.ok());
    const auto figures = make_figures();

    sim::World world;
    world.set_players({sim::Player{assets::PlayerKind::Human, assets::PlayerColor::Red, 0, ""},
                       sim::Player{assets::PlayerKind::Natives, assets::PlayerColor::Natives, 0, ""}});
    const sim::Ship ship{"Patria", 10, 20, 21, 0, 3};

    render::WorldLayers layers;
    layers.figures = &figures;
    layers.atlas = &*atlas;
    layers.ship_base = atlas->sheet_base(1);
    const render::WorldView view{render::Zoom::Large, 0};

    render::ShipSprites s;
    REQUIRE(render::ship_sprites(ship, world, view, layers, 0, s));
    // Tile centre: x = (10.5 - 20.5 + 350) * 32, y = 31 * 16. Hull: sprite Gfx + direction,
    // centred, bottom at y + Posoffs[0].
    CHECK(s.hull.sprite == layers.ship_base + 3);
    CHECK(s.hull.x == 340 * 32 - 10);
    CHECK(s.hull.y == 496 - 19);
    CHECK(s.row == (496 - 19 - 1) / 16);
    // Flag: FAHNE1 (red), frame 0, Fahnoffs height 2.0 * 32 above the figure point.
    REQUIRE(s.has_flag);
    CHECK(s.flag.sprite == layers.ship_base + 8);
    CHECK(s.flag.x == 340 * 32 - 3);
    CHECK(s.flag.y == 496 - 64 + 4);
    REQUIRE(render::ship_sprites(ship, world, view, layers, 200, s));
    CHECK(s.flag.sprite == layers.ship_base + 10);  // 90 ms per flag frame

    // View rotation adds two direction steps per quarter turn.
    REQUIRE(render::ship_sprites(ship, world, render::WorldView{render::Zoom::Large, 1}, layers, 0, s));
    CHECK(s.hull.sprite == layers.ship_base + 5);

    // Natives have no flag; unknown figures are not drawn.
    const sim::Ship native{"Kanu", 10, 20, 21, 1, 0};
    REQUIRE(render::ship_sprites(native, world, view, layers, 0, s));
    CHECK(!s.has_flag);
    const sim::Ship unknown{"?", 10, 20, 99, 0, 0};
    CHECK(!render::ship_sprites(unknown, world, view, layers, 0, s));
}

TEST_CASE("world renderer: ownership view adds diamonds in the owner's colour") {
    const assets::CodPredefinedSymbol types[] = {{"HAUS", 1}};
    const auto doc = assets::cod::parse("Objekt: HAUS\n@Nummer: 0\nId: 20100\nGfx: 0\nSize: 1, 1\nEndObj\n", types);
    auto buildings = assets::BuildingTable::from_cod(doc);
    REQUIRE(buildings.ok());

    sim::World world;
    world.set_players({sim::Player{}, sim::Player{assets::PlayerKind::Computer, assets::PlayerColor::Yellow, 0, ""}});
    sim::WorldIsland island{assets::IslandHeader{}, sim::IslandMap(2, 1), 5, 5};
    REQUIRE(island.map.place(*buildings->by_id(20100), 0, 0, 0, 0, 1).ok());  // owned by slot 1
    REQUIRE(island.map.place(*buildings->by_id(20100), 1, 0, 0, 0).ok());     // nobody
    REQUIRE(world.add_island(std::move(island)).ok());

    const render::AnimationClock clock(*buildings);
    render::WorldLayers layers;
    layers.extent = render::SpriteExtent{16, 8};
    layers.owner_tint = true;
    const render::WorldView view{render::Zoom::Small, 0};
    render::Viewport all;
    render::world_pixel_size(view, all.width, all.height);
    std::vector<render::DrawCommand> commands;
    render::build_world_draw_list(world, *buildings, clock, view, all, layers, commands);
    const auto diamonds = std::count_if(commands.begin(), commands.end(), [](const render::DrawCommand& c) {
        return c.sprite == render::DrawCommand::kOverlayDiamond;
    });
    CHECK(diamonds == 1);
    const auto it = std::find_if(commands.begin(), commands.end(), [](const render::DrawCommand& c) {
        return c.sprite == render::DrawCommand::kOverlayDiamond;
    });
    REQUIRE(it != commands.end());
    CHECK(it->tint == static_cast<std::uint8_t>(assets::PlayerColor::Yellow));
    CHECK(it->tint_size == 16);

    layers.owner_tint = false;
    render::build_world_draw_list(world, *buildings, clock, view, all, layers, commands);
    CHECK(std::none_of(commands.begin(), commands.end(),
                       [](const render::DrawCommand& c) { return c.sprite == render::DrawCommand::kOverlayDiamond; }));
}

TEST_CASE("scenario: savegame names from Game.dat") {
    const char* game_dat =
        "  Lastfile: \"X:\\Games\\Anno 1602\\Savegame\\lastgame.gam\"\n"
        "  Objekt:   SPIELNAME\n"
        "\n"
        "    Name:      0, \"Merlin01\", 1\n"
        "    Name:      1, \"Leer01\", 1\n"
        "    Name:      12, \"Insel mit \xfc\", 0\n"
        "  EndObj;\n"
        "  Name: 5, \"not a slot\", 1\n";
    const auto slots = assets::parse_savegame_slots(game_dat);
    REQUIRE(slots.size() == 3);
    CHECK(slots[0].slot == 0);
    CHECK(slots[0].name == "Merlin01");
    CHECK(slots[0].file == "Savegame/game00.gam");
    CHECK(slots[1].name == "Leer01");
    CHECK(slots[2].slot == 12);
    CHECK(slots[2].file == "Savegame/game12.gam");
    CHECK(slots[2].name == "Insel mit \xfc");
}
