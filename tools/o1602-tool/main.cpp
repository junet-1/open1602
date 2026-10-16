// SPDX-License-Identifier: GPL-3.0-or-later
//
// o1602-tool - inspect and convert original Anno 1602 data files.
//
//   o1602-tool chunks     <file>                         list chunks of any chunk container
//   o1602-tool cod        <file.cod> [out.txt]           decode a .cod script to UTF-8 text
//   o1602-tool cod-dump   <file.cod> [--predefined haeuser|figuren|none] [--type NAME]
//                                                        interpret a .cod script and print its records
//   o1602-tool bsh-info   <file.bsh|.zei>                list sprites
//   o1602-tool bsh-export <file.bsh|.zei> <outdir> [--palette FILE] [--sheet] [--first N] [--count N]
//   o1602-tool islands    <file.scp|.szs|.szm|.gam>      list islands and tile statistics
//   o1602-tool savegames                                 list savegames (names from Game.dat)
//   o1602-tool render-island <file.scp|.szs|.gam> <out.png> [--island N] [--zoom 0|1|2]
//                            [--rotation 0-3] [--anim N] [--no-template] [--base FILE.scp]
//                                                        render an island (needs ANNO1602_DIR)
//   o1602-tool render-world <file.szs|.gam> <out.png> [--zoom 0|1|2] [--rotation 0-3] [--time MS]
//                           [--tile X,Y] [--size WxH] [--owners]
//                                                        render the world map or a part of it
//
// The palette defaults to $ANNO1602_DIR/TOOLGFX/STADTFLD.COL.

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
#include "o1602/assets/text_table.hpp"
#include "o1602/core/file.hpp"
#include "o1602/core/png_writer.hpp"
#include "o1602/core/text.hpp"
#include "o1602/render/animation_clock.hpp"
#include "o1602/render/island_renderer.hpp"
#include "o1602/render/land_figures.hpp"
#include "o1602/render/sprite_atlas.hpp"
#include "o1602/render/walker_figures.hpp"
#include "o1602/render/world_graphics.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/construction.hpp"
#include "o1602/sim/island_map.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/sim/world.hpp"

namespace fs = std::filesystem;
using namespace o1602;

namespace {

int fail(const core::Error& error) {
    std::fprintf(stderr, "error: %s\n", error.message.c_str());
    return 1;
}

int usage() {
    std::fputs(
        "usage:\n"
        "  o1602-tool chunks     <file>\n"
        "  o1602-tool cod        <file.cod> [out.txt]\n"
        "  o1602-tool cod-dump   <file.cod> [--predefined haeuser|figuren|none] [--type NAME]\n"
        "  o1602-tool bsh-info   <file.bsh|.zei>\n"
        "  o1602-tool bsh-export <file.bsh|.zei> <outdir> [--palette FILE] [--sheet] [--first N] [--count N]\n"
        "  o1602-tool islands    <file.scp|.szs|.szm|.gam>\n"
        "  o1602-tool savegames                               (names from Game.dat, needs ANNO1602_DIR)\n"
        "  o1602-tool state      <file.gam|.szs> [--wares] [--production]\n"
        "                        (players, cities, warehouses, production, ships, deer)\n"
        "  o1602-tool render-island <file.scp|.szs|.gam> <out.png> [--island N] [--zoom 0|1|2] [--rotation 0-3]\n"
        "                           [--anim N] [--no-template] [--base FILE.scp]\n"
        "  o1602-tool render-world <file.szs|.gam> <out.png> [--zoom 0|1|2] [--rotation 0-3] [--time MS]\n"
        "                          [--tile X,Y] [--size WxH] [--owners] [--original-sheets] [--original-flags]\n"
        "                          [--banner P:RRGGBB] [--simulate SECONDS] [--walkers]\n"
        "                          [--build \"ID@X,Y[,R];...\"]  (world tiles, for the first human player)\n"
        "                          (default: whole world at zoom 2; --simulate: run the economy first; "
        "--original-sheets: no figures where the\n"
        "                          original has no graphics for them; --original-flags: the original flag\n"
        "                          images instead of the tinted generic flag)\n",
        stderr);
    return 2;
}

/// Simple "--name value" / "--flag" option lookup.
struct Args {
    std::vector<std::string_view> positional;
    std::map<std::string_view, std::string_view> options;

    Args(int argc, char** argv) {
        for (int i = 2; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if (arg.starts_with("--")) {
                const bool has_value = i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--");
                options[arg] = has_value ? std::string_view(argv[++i]) : std::string_view();
            } else {
                positional.push_back(arg);
            }
        }
    }

    [[nodiscard]] bool flag(std::string_view name) const { return options.contains(name); }

    [[nodiscard]] std::optional<std::size_t> number(std::string_view name) const {
        const auto it = options.find(name);
        if (it == options.end()) {
            return std::nullopt;
        }
        std::size_t value = 0;
        const auto [ptr, ec] = std::from_chars(it->second.data(), it->second.data() + it->second.size(), value);
        if (ec != std::errc{}) {
            return std::nullopt;
        }
        return value;
    }

    /// "--name A<sep>B" as two numbers, e.g. "--tile 120,80" or "--size 1920x1080".
    [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>> pair(std::string_view name,
                                                                            char separator) const {
        const auto it = options.find(name);
        if (it == options.end()) {
            return std::nullopt;
        }
        const std::string_view v = it->second;
        const auto sep = v.find(separator);
        if (sep == std::string_view::npos) {
            return std::nullopt;
        }
        std::int32_t a = 0;
        std::int32_t b = 0;
        if (std::from_chars(v.data(), v.data() + sep, a).ec != std::errc{} ||
            std::from_chars(v.data() + sep + 1, v.data() + v.size(), b).ec != std::errc{}) {
            return std::nullopt;
        }
        return std::pair{a, b};
    }
};

int cmd_chunks(const Args& args) {
    if (args.positional.size() != 1) {
        return usage();
    }
    auto file = assets::ChunkFile::load(fs::path(args.positional[0]));
    if (!file) {
        return fail(file.error());
    }
    std::printf("%-18s %10s %10s\n", "name", "offset", "size");
    for (const auto& chunk : file->chunks()) {
        std::printf("%-18s %10zu %10zu\n", chunk.name.c_str(), chunk.offset, chunk.size);
    }
    std::printf("%zu chunks\n", file->chunks().size());
    return 0;
}

int cmd_cod(const Args& args) {
    if (args.positional.empty() || args.positional.size() > 2) {
        return usage();
    }
    auto text = assets::load_cod_utf8(fs::path(args.positional[0]));
    if (!text) {
        return fail(text.error());
    }
    if (args.positional.size() == 2) {
        const auto* begin = reinterpret_cast<const std::uint8_t*>(text->data());
        if (auto status = core::write_file(fs::path(args.positional[1]), std::span(begin, text->size())); !status) {
            return fail(status.error());
        }
    } else {
        std::fwrite(text->data(), 1, text->size(), stdout);
    }
    return 0;
}

std::string format_value(const assets::cod::Value& v) {
    if (v.type == assets::cod::ValueType::String) {
        return "\"" + v.text + "\"";
    }
    if (v.real != static_cast<double>(v.integer)) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%g", v.real);
        return buf;
    }
    return std::to_string(v.integer);
}

void print_object(const assets::cod::Object& obj, int indent, const std::string& only_type) {
    const std::string pad(static_cast<std::size_t>(indent) * 2, ' ');
    for (const auto& [name, field] : obj.fields) {
        if (field.assigned_with_equals) {
            continue;  // constants (NAME = value) are not record properties
        }
        std::string line = pad + name + ":";
        for (std::size_t i = 0; i < field.values.size(); ++i) {
            line += (i == 0 ? " " : ", ") + format_value(field.values[i]);
        }
        std::printf("%s\n", line.c_str());
    }
    for (const auto& [type, records] : obj.children) {
        if (indent == 0 && !only_type.empty() && type != only_type) {
            continue;
        }
        for (const auto& [nummer, record] : records) {
            std::printf("%s[%s #%d]\n", pad.c_str(), type.c_str(), nummer);
            print_object(record, indent + 1, only_type);
        }
    }
}

int cmd_cod_dump(const Args& args) {
    if (args.positional.size() != 1) {
        return usage();
    }
    const fs::path path(args.positional[0]);
    auto bytes = core::read_file(path);
    if (!bytes) {
        return fail(bytes.error());
    }
    std::string predefined = "auto";
    if (const auto it = args.options.find("--predefined"); it != args.options.end()) {
        predefined = std::string(it->second);
    }
    if (predefined == "auto") {
        std::string stem = path.stem().string();
        std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return std::tolower(c); });
        predefined = stem == "haeuser" || stem == "figuren" ? stem : "none";
    }
    std::span<const assets::CodPredefinedSymbol> symbols;
    if (predefined == "haeuser") {
        symbols = assets::cod_predefined_haeuser();
    } else if (predefined == "figuren") {
        symbols = assets::cod_predefined_figuren();
    }

    const auto doc = assets::cod::parse(assets::decode_cod(*bytes), symbols);
    std::string only_type;
    if (const auto it = args.options.find("--type"); it != args.options.end()) {
        only_type = std::string(it->second);
    }
    print_object(doc.root, 0, only_type);

    std::fprintf(stderr, "-- %zu symbols, %zu diagnostics (predefined: %s)\n", doc.symbols.size(),
                 doc.diagnostics.size(), predefined.c_str());
    for (const auto& d : doc.diagnostics) {
        std::fprintf(stderr, "   line %u: %s\n", d.line, d.message.c_str());
    }
    return 0;
}

int cmd_bsh_info(const Args& args) {
    if (args.positional.size() != 1) {
        return usage();
    }
    auto sheet = assets::SpriteSheet::load(fs::path(args.positional[0]));
    if (!sheet) {
        return fail(sheet.error());
    }
    std::printf("%6s %6s %6s %5s %8s %10s\n", "index", "width", "height", "type", "length", "offset");
    for (std::size_t i = 0; i < sheet->size(); ++i) {
        const auto& info = sheet->info(i);
        std::printf("%6zu %6u %6u %5u %8u %10u\n", i, info.width, info.height, info.type, info.length, info.offset);
    }
    std::printf("%zu sprites\n", sheet->size());
    return 0;
}

core::Result<assets::Palette> load_palette(const Args& args) {
    if (const auto it = args.options.find("--palette"); it != args.options.end()) {
        return assets::Palette::load(fs::path(it->second));
    }
    auto dir = assets::GameDirectory::from_environment();
    if (!dir) {
        return core::make_error(core::ErrorCode::NotFound, "no --palette given and " + dir.error().message);
    }
    const auto path = dir->resolve("toolgfx/stadtfld.col");
    if (!path) {
        return core::make_error(core::ErrorCode::NotFound, "TOOLGFX/STADTFLD.COL not found");
    }
    return assets::Palette::load(*path);
}

int cmd_bsh_export(const Args& args) {
    if (args.positional.size() != 2) {
        return usage();
    }
    auto sheet = assets::SpriteSheet::load(fs::path(args.positional[0]));
    if (!sheet) {
        return fail(sheet.error());
    }
    auto palette = load_palette(args);
    if (!palette) {
        return fail(palette.error());
    }

    const fs::path out_dir(args.positional[1]);
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        return fail(core::make_error(core::ErrorCode::IoError, "cannot create " + out_dir.string()));
    }

    const std::size_t first = std::min(args.number("--first").value_or(0), sheet->size());
    const std::size_t count = std::min(args.number("--count").value_or(sheet->size()), sheet->size() - first);
    const std::string stem = fs::path(args.positional[0]).stem().string();

    std::vector<assets::IndexedImage> images;
    for (std::size_t i = first; i < first + count; ++i) {
        auto image = sheet->decode(i);
        if (!image) {
            return fail(image.error());
        }
        if (args.flag("--sheet")) {
            images.push_back(std::move(image).value());
            continue;
        }
        char name[64];
        std::snprintf(name, sizeof(name), "%s_%05zu.png", stem.c_str(), i);
        if (auto status = core::write_png_rgba(out_dir / name, image->width, image->height, image->to_rgba(*palette));
            !status) {
            return fail(status.error());
        }
    }

    if (args.flag("--sheet") && !images.empty()) {
        // Grid of equally sized cells, sprites bottom-aligned (as they are drawn in game).
        std::uint32_t cell_w = 1;
        std::uint32_t cell_h = 1;
        for (const auto& img : images) {
            cell_w = std::max(cell_w, img.width);
            cell_h = std::max(cell_h, img.height);
        }
        const auto columns =
            static_cast<std::uint32_t>(std::max<std::size_t>(1, std::min<std::size_t>(16, images.size())));
        const auto rows = static_cast<std::uint32_t>((images.size() + columns - 1) / columns);
        const std::uint32_t width = cell_w * columns;
        const std::uint32_t height = cell_h * rows;
        std::vector<std::uint8_t> rgba(std::size_t{width} * height * 4, 0);
        for (std::size_t k = 0; k < images.size(); ++k) {
            const auto& img = images[k];
            const auto pixels = img.to_rgba(*palette);
            const std::uint32_t ox = static_cast<std::uint32_t>(k % columns) * cell_w;
            const std::uint32_t oy = static_cast<std::uint32_t>(k / columns) * cell_h + (cell_h - img.height);
            for (std::uint32_t y = 0; y < img.height; ++y) {
                std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(std::size_t{y} * img.width * 4),
                            std::size_t{img.width} * 4,
                            rgba.begin() + static_cast<std::ptrdiff_t>((std::size_t{oy + y} * width + ox) * 4));
            }
        }
        const fs::path out = out_dir / (stem + "_sheet.png");
        if (auto status = core::write_png_rgba(out, width, height, rgba); !status) {
            return fail(status.error());
        }
        std::printf("wrote %s (%ux%u, %zu sprites)\n", out.string().c_str(), width, height, images.size());
    } else {
        std::printf("wrote %zu sprites to %s\n", count, out_dir.string().c_str());
    }
    return 0;
}

int cmd_savegames(const Args& args) {
    if (!args.positional.empty()) {
        return usage();
    }
    auto game = assets::GameDirectory::from_environment();
    if (!game) {
        return fail(game.error());
    }
    const auto dat = game->resolve("Game.dat");
    if (!dat) {
        return fail(core::make_error(core::ErrorCode::NotFound, "Game.dat not found (the game was never started?)"));
    }
    auto bytes = core::read_file(*dat);
    if (!bytes) {
        return fail(bytes.error());
    }
    const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    for (const auto& slot : assets::parse_savegame_slots(text)) {
        const auto path = game->resolve(slot.file);
        std::printf("%2u  %-24s %s", slot.slot, core::cp1252_to_utf8(slot.name).c_str(), slot.file.c_str());
        if (!path) {
            std::printf("  (missing)\n");
            continue;
        }
        auto file = assets::ChunkFile::load(*path);
        std::string scenario;
        std::string player;
        if (file) {
            if (const auto* name = file->find("NAME")) {
                const auto p = file->payload(*name);
                scenario.assign(p.begin(), std::find(p.begin(), p.end(), std::uint8_t{0}));
            }
            if (auto players = assets::load_players(*file); players && !players->empty()) {
                player = players->front().name;
            }
        }
        std::printf("  scenario \"%s\", player \"%s\"\n", core::cp1252_to_utf8(scenario).c_str(),
                    core::cp1252_to_utf8(player).c_str());
    }
    return 0;
}

int cmd_state(const Args& args) {
    if (args.positional.size() != 1) {
        return usage();
    }
    auto file = assets::ChunkFile::load(fs::path(args.positional[0]));
    if (!file) {
        return fail(file.error());
    }
    auto players = assets::load_players(*file);
    auto ships = assets::load_ships(*file);
    auto state = assets::load_game_state(*file);
    if (!players || !ships || !state) {
        return fail(!players ? players.error() : !ships ? ships.error() : state.error());
    }
    for (const auto& p : *players) {
        std::printf("player %u: kind 0x%02X, colour %u, money %d %s\n", p.slot, static_cast<unsigned>(p.kind),
                    static_cast<unsigned>(p.color), p.money, p.name.c_str());
    }
    for (const auto& c : state->cities) {
        std::printf("city: island %u #%u, owner %u, \"%s\"\n", c.island, c.index, c.owner, c.name.c_str());
    }
    for (const auto& w : state->warehouses) {
        std::size_t used = 0;
        for (const auto& s : w.wares) {
            used += (s.raw[0] | s.raw[1] | s.raw[2] | (s.raw[3] & 0xFFFFU)) != 0 ? 1U : 0U;
        }
        std::printf("warehouse: island %u #%u at %u,%u, %zu goods with data\n", w.island, w.city, w.x, w.y, used);
        if (args.flag("--wares")) {
            for (const auto& s : w.wares) {
                std::printf("  ware %2u (producer %u): %6.2f t   raw %08X %08X %08X %08X\n", s.ware, s.producer_id,
                            s.stock() / static_cast<double>(assets::kStockUnitsPerTon), s.raw[0], s.raw[1], s.raw[2],
                            s.raw[3]);
            }
        }
    }
    struct Counts {
        std::size_t buildings = 0;
        std::size_t working = 0;
        std::size_t disabled = 0;
    };
    std::map<unsigned, Counts> production;
    for (const auto& p : state->production) {
        Counts& c = production[p.island];
        ++c.buildings;
        c.working += p.enabled && p.rate != 0 ? 1U : 0U;
        c.disabled += p.enabled ? 0U : 1U;
        if (args.flag("--production")) {
            constexpr double kTon = assets::kStockUnitsPerTon;
            std::printf(
                "  production %u: %2u,%2u %-3s rate %3u, next %3u s, raw %5.2f/%5.2f t, stock %5.2f t, idle %2u, "
                "produced %5.2f t in %3u s\n",
                p.island, p.x, p.y, p.enabled ? "on" : "off", p.rate, p.countdown, p.raw / kTon, p.raw2 / kTon,
                p.stock / kTon, p.idle_cycles, p.produced / kTon, p.elapsed);
        }
    }
    for (const auto& [island, c] : production) {
        std::printf("production: island %u, %zu buildings, %zu working, %zu switched off\n", island, c.buildings,
                    c.working, c.disabled);
    }
    std::map<unsigned, std::size_t> deer;
    for (const auto& a : state->animals) {
        ++deer[a.island];
    }
    std::printf("deer: %zu on %zu islands\n", state->animals.size(), deer.size());
    for (const auto& s : *ships) {
        std::printf("ship \"%s\": figure %u, owner %u, at %u,%u, direction %u, island %s\n", s.name.c_str(), s.figure,
                    s.owner, s.x, s.y, s.direction, s.island == 0xFF ? "-" : std::to_string(s.island).c_str());
    }
    return 0;
}

int cmd_islands(const Args& args) {
    if (args.positional.size() != 1) {
        return usage();
    }
    auto file = assets::ChunkFile::load(fs::path(args.positional[0]));
    if (!file) {
        return fail(file.error());
    }
    auto islands = assets::load_islands(*file);
    if (!islands) {
        return fail(islands.error());
    }
    for (const auto& island : *islands) {
        const auto& h = island.header;
        std::printf("%s #%u  size %ux%u  pos (%u,%u)  layers %zu\n", h.chunk_name.c_str(), h.number, h.width, h.height,
                    h.pos_x, h.pos_y, island.layers.size());
        for (std::size_t l = 0; l < island.layers.size(); ++l) {
            std::map<std::uint16_t, std::size_t> histogram;
            for (const auto& tile : island.layers[l]) {
                ++histogram[tile.id];
            }
            std::vector<std::pair<std::size_t, std::uint16_t>> top;
            for (const auto& [id, n] : histogram) {
                top.emplace_back(n, id);
            }
            std::sort(top.rbegin(), top.rend());
            std::printf("  layer %zu: %zu tiles, %zu distinct ids; most common:", l, island.layers[l].size(),
                        histogram.size());
            for (std::size_t i = 0; i < std::min<std::size_t>(5, top.size()); ++i) {
                std::printf(" %u(x%zu)", top[i].second + assets::IslandTile::kCodIdOffset, top[i].first);
            }
            std::printf("\n");
        }
    }
    std::printf("%zu islands\n", islands->size());
    return 0;
}

int cmd_render_island(const Args& args) {
    if (args.positional.size() != 2) {
        return usage();
    }
    auto game = assets::GameDirectory::from_environment();
    if (!game) {
        return fail(game.error());
    }

    auto buildings = assets::BuildingTable::load(*game);
    if (!buildings) {
        return fail(buildings.error());
    }

    // Island
    auto file = assets::ChunkFile::load(fs::path(args.positional[0]));
    if (!file) {
        return fail(file.error());
    }
    auto islands = assets::load_islands(*file);
    if (!islands) {
        return fail(islands.error());
    }
    const std::size_t island_index = args.number("--island").value_or(0);
    if (island_index >= islands->size()) {
        return fail(core::make_error(core::ErrorCode::OutOfRange,
                                     "file has " + std::to_string(islands->size()) + " island(s)"));
    }
    // Base terrain: islands in scenarios/savegames only store deviations from a template .SCP.
    const assets::Island& island = (*islands)[island_index];
    std::vector<assets::Island> base;
    std::string base_path;
    if (const auto it = args.options.find("--base"); it != args.options.end()) {
        base_path = std::string(it->second);
    } else if (!args.flag("--no-template") && !island.header.template_path().empty()) {
        const auto resolved = game->resolve(island.header.template_path());
        if (!resolved) {
            return fail(core::make_error(core::ErrorCode::NotFound,
                                         "island template not found: " + island.header.template_path()));
        }
        base_path = resolved->string();
    }
    if (!base_path.empty()) {
        auto base_file = assets::ChunkFile::load(fs::path(base_path));
        if (!base_file) {
            return fail(base_file.error());
        }
        auto loaded = assets::load_islands(*base_file);
        if (!loaded || loaded->empty()) {
            return fail(core::make_error(core::ErrorCode::InvalidFormat, base_path + " has no island"));
        }
        base = std::move(loaded).value();
        std::printf("template: %s\n", base_path.c_str());
    }
    std::vector<std::string> warnings;
    const auto map = sim::IslandMap::from_island(island, *buildings, &warnings, base.empty() ? nullptr : &base[0]);

    // Graphics
    render::RenderOptions options;
    const std::size_t zoom = std::min<std::size_t>(args.number("--zoom").value_or(0), 2);
    options.zoom = static_cast<render::Zoom>(zoom);
    options.view_rotation = static_cast<std::uint8_t>(args.number("--rotation").value_or(0) & 3U);
    options.anim_step = static_cast<std::uint32_t>(args.number("--anim").value_or(0));
    auto sheet = assets::SpriteSheet::load(*game->resolve(render::terrain_sheet_path(options.zoom)));
    if (!sheet) {
        return fail(sheet.error());
    }
    auto palette = assets::Palette::load(*game->resolve(render::kTerrainPalettePath));
    if (!palette) {
        return fail(palette.error());
    }

    const auto commands = render::build_draw_list(map, *buildings, options);
    std::size_t missing = 0;
    const auto image = render::rasterize(commands, *sheet, *palette, assets::Rgba{24, 40, 72, 255}, &missing);
    if (image.width == 0) {
        return fail(core::make_error(core::ErrorCode::InvalidFormat, "nothing to draw"));
    }
    if (auto status = core::write_png_rgba(fs::path(args.positional[1]), image.width, image.height, image.pixels);
        !status) {
        return fail(status.error());
    }
    std::printf("island %zu: %ux%u tiles, %zu sprites drawn, %zu missing -> %s (%ux%u)\n", island_index, map.width(),
                map.height(), commands.size() - missing, missing, std::string(args.positional[1]).c_str(), image.width,
                image.height);
    for (const auto& w : warnings) {
        std::fprintf(stderr, "warning: %s\n", w.c_str());
    }
    return 0;
}

int cmd_render_world(const Args& args) {
    if (args.positional.size() != 2) {
        return usage();
    }
    auto game = assets::GameDirectory::from_environment();
    if (!game) {
        return fail(game.error());
    }
    auto buildings = assets::BuildingTable::load(*game);
    if (!buildings) {
        return fail(buildings.error());
    }
    auto file = assets::ChunkFile::load(fs::path(args.positional[0]));
    if (!file) {
        return fail(file.error());
    }
    std::vector<std::string> warnings;
    auto world = sim::load_world(*file, *buildings, *game, &warnings);
    if (!world) {
        return fail(world.error());
    }
    render::WorldView view;
    view.zoom = static_cast<render::Zoom>(std::min<std::size_t>(args.number("--zoom").value_or(2), 2));
    view.rotation = static_cast<std::uint8_t>(args.number("--rotation").value_or(0) & 3U);

    const auto palette_path = game->resolve(render::kTerrainPalettePath);
    if (!palette_path) {
        return fail(core::make_error(core::ErrorCode::NotFound, "stadtfld.col not found"));
    }
    auto palette = assets::Palette::load(*palette_path);
    if (!palette) {
        return fail(palette.error());
    }
    auto figures = assets::FigureTable::load(*game);
    if (!figures) {
        return fail(figures.error());
    }
    // --build "ID@X,Y[,R];..." places buildings for the first human player (world tiles).
    if (const auto it = args.options.find("--build"); it != args.options.end()) {
        std::vector<std::string> names;
        if (auto texts = assets::TextTable::load(*game)) {
            names = texts->section("STAEDTE");
        }
        std::string spec(it->second);
        std::size_t pos = 0;
        while (pos < spec.size()) {
            const std::size_t end = std::min(spec.find(';', pos), spec.size());
            const std::string item = spec.substr(pos, end - pos);
            pos = end + 1;
            unsigned id = 0;
            unsigned x = 0;
            unsigned y = 0;
            unsigned r = 0;
            if (std::sscanf(item.c_str(), "%u@%u,%u,%u", &id, &x, &y, &r) < 3 || !world->contains(x, y)) {
                std::fprintf(stderr, "bad --build item '%s'\n", item.c_str());
                continue;
            }
            const auto& cell = world->cell(x, y);
            if (cell.island == sim::World::kSea) {
                std::fprintf(stderr, "--build %s: open sea\n", item.c_str());
                continue;
            }
            sim::BuildOrder order;
            order.player = sim::first_human_player(*world);
            order.island = cell.island;
            order.items = {{id, cell.local_x, cell.local_y, static_cast<std::uint8_t>(r & 3U), 0}};
            const sim::BuildPlan plan = sim::execute_build(*world, *buildings, order, names, nullptr);
            std::printf("build %s: %s\n", item.c_str(),
                        plan.placed == 1 ? "ok" : sim::refusal_name(plan.items[0].refusal));
        }
    }
    std::optional<sim::Simulation> simulation;
    if (const auto seconds = args.number("--simulate")) {
        simulation.emplace(*world, *buildings, *figures);
        std::size_t produced = 0;
        std::size_t stopped = 0;
        for (std::uint64_t s = 0; s < static_cast<std::uint64_t>(*seconds); ++s) {
            simulation->advance(1000);
            for (const auto& e : simulation->take_production_events()) {
                produced += e.kind == sim::ProductionEvent::Kind::Produced ? 1U : 0U;
                stopped += e.kind == sim::ProductionEvent::Kind::Stopped ? 1U : 0U;
            }
        }
        std::size_t working = 0;
        for (const auto& p : world->productions()) {
            working += p.enabled && p.working() ? 1U : 0U;
        }
        const sim::LogisticsStats& ls = simulation->logistics().stats();
        std::printf("simulated %lld s: %zu tons produced, %zu buildings stopped, %zu working\n",
                    static_cast<long long>(*seconds), produced, stopped, working);
        std::printf(
            "logistics: %llu carrier trips (%.2f t raw material delivered), %llu cart trips (%.2f t "
            "collected), %zu figures on their way\n",
            static_cast<unsigned long long>(ls.carrier_trips),
            static_cast<double>(ls.raw_delivered) / assets::kStockUnitsPerTon,
            static_cast<unsigned long long>(ls.cart_trips),
            static_cast<double>(ls.goods_collected) / assets::kStockUnitsPerTon,
            simulation->logistics().walkers().size());
        std::printf("deer: %llu appeared, %llu hunted\n", static_cast<unsigned long long>(ls.deer_spawned),
                    static_cast<unsigned long long>(ls.deer_hunted));
        std::printf("raw producers: %llu worker trips, %.2f t harvested, %.2f t ore mined, %zu growing tiles\n",
                    static_cast<unsigned long long>(ls.worker_trips),
                    static_cast<double>(ls.harvested) / assets::kStockUnitsPerTon,
                    static_cast<double>(ls.ore_mined) / assets::kStockUnitsPerTon, world->growth().entries.size());
        if (args.flag("--walkers")) {
            simulation->logistics().walkers().for_each([&](sim::Handle, const sim::Walker& w) {
                const sim::WorldIsland& island = world->islands()[w.island];
                const sim::WalkerPosition pos = sim::walker_position(w);
                static constexpr const char* kKinds[] = {"carrier", "cart", "harvester", "grazer",
                                                         "fisher",  "deer", "hunter"};
                std::printf("  %-9s figure %u at %.2f,%.2f (island %u), ware %u, %s %.2f t\n",
                            kKinds[static_cast<int>(w.kind)], w.figure, island.x + pos.x, island.y + pos.y, w.island,
                            w.ware, w.returning ? "carrying" : "reserved",
                            static_cast<double>(w.returning ? w.load : w.reserved) / assets::kStockUnitsPerTon);
            });
        }
    }
    render::GraphicsOptions graphics_options;
    graphics_options.derive_missing_sheets = !args.flag("--original-sheets");
    auto banners = render::BannerTable::original(&*figures, render::BannerOptions{args.flag("--original-flags")});
    // --banner P:RRGGBB gives player P a new banner in any colour (tinted generic flag).
    if (const auto it = args.options.find("--banner"); it != args.options.end()) {
        const std::string spec(it->second);
        const auto colon = spec.find(':');
        if (colon == std::string::npos || spec.size() - colon - 1 != 6) {
            return usage();
        }
        const auto slot = static_cast<std::size_t>(std::strtoul(spec.substr(0, colon).c_str(), nullptr, 10));
        const auto rgb = static_cast<std::uint32_t>(std::strtoul(spec.substr(colon + 1).c_str(), nullptr, 16));
        auto players = world->players();
        if (slot >= players.size()) {
            return fail(core::make_error(core::ErrorCode::OutOfRange, "no player " + std::to_string(slot)));
        }
        players[slot].banner =
            banners.add_color("custom:" + spec.substr(colon + 1),
                              assets::Rgba{static_cast<std::uint8_t>(rgb >> 16), static_cast<std::uint8_t>(rgb >> 8),
                                           static_cast<std::uint8_t>(rgb), 255});
        world->set_players(std::move(players));
    }
    auto graphics = render::load_world_graphics(*game, *palette, view.zoom, render::kWorldFigureSheets,
                                                graphics_options, &banners, &*figures);
    if (!graphics) {
        return fail(graphics.error());
    }
    const render::SpriteAtlas* atlas = &graphics->atlas;

    render::AnimationClock clock(*buildings);
    clock.advance(static_cast<std::uint32_t>(args.number("--time").value_or(0)));

    render::Viewport viewport;
    world_pixel_size(view, viewport.width, viewport.height);
    if (const auto size = args.pair("--size", 'x')) {
        viewport.width = std::max(1, size->first);
        viewport.height = std::max(1, size->second);
    }
    if (const auto tile = args.pair("--tile", ',')) {
        std::int32_t cx = 0;
        std::int32_t cy = 0;
        render::world_tile_center(view, static_cast<std::uint32_t>(std::clamp(tile->first, 0, 499)),
                                  static_cast<std::uint32_t>(std::clamp(tile->second, 0, 349)), cx, cy);
        viewport.left = cx - viewport.width / 2;
        viewport.top = cy - viewport.height / 2;
    }
    if (std::int64_t{viewport.width} * viewport.height > 200'000'000) {
        return fail(core::make_error(core::ErrorCode::OutOfRange,
                                     "image too large; use --zoom 2 or --size/--tile for a part of the world"));
    }

    render::WorldLayers layers;
    layers.extent = render::SpriteExtent{atlas->max_sprite_width(), atlas->max_sprite_height()};
    layers.figures = &*figures;
    layers.atlas = atlas;
    layers.ship_base = graphics->sheet_base(assets::FigureSheet::Ship).value_or(0);
    layers.sheet_base = graphics->base;
    layers.banners = &banners;
    layers.banner_flags = &graphics->banner_flags;
    layers.owner_tint = args.flag("--owners");
    const auto building_figures = render::collect_building_figures(*world, *buildings, *figures);
    layers.building_figures = &building_figures;
    layers.local_player = sim::first_human_player(*world);
    render::AnimalFigures animals;
    animals.reset(*world, *figures);
    animals.advance(static_cast<std::uint32_t>(args.number("--time").value_or(0)), *figures);
    render::SoldierFigures soldiers;
    soldiers.reset(*world, *figures);
    soldiers.advance(static_cast<std::uint32_t>(args.number("--time").value_or(0)), *figures);
    render::WalkerFigures walkers;
    if (simulation) {
        walkers.update(simulation->logistics(), *world, *figures, 0);
    }
    if (simulation) {
        layers.land_figures = {&soldiers.instances(), &walkers.instances()};  // deer walk with the simulation
    } else {
        layers.land_figures = {&animals.instances(), &soldiers.instances()};
    }
    layers.unit_tint = &graphics->unit_tint;
    std::vector<render::DrawCommand> commands;
    render::build_world_draw_list(*world, *buildings, clock, view, viewport, layers, commands);
    render::RgbaImage image;
    image.width = static_cast<std::uint32_t>(viewport.width);
    image.height = static_cast<std::uint32_t>(viewport.height);
    image.pixels.assign(std::size_t{image.width} * image.height * 4, 0);
    for (std::size_t i = 3; i < image.pixels.size(); i += 4) {
        image.pixels[i] = 255;  // black outside the map, as in the original
    }
    render::blit_draw_list(commands, *atlas, image, viewport.left, viewport.top);
    if (auto status = core::write_png_rgba(fs::path(args.positional[1]), image.width, image.height, image.pixels);
        !status) {
        return fail(status.error());
    }
    for (const auto& p : world->players()) {
        const render::Banner* b = banners.get(p.banner);
        std::printf("player: %s, banner %s, money %d\n", sim::role_name(p.role), b != nullptr ? b->id.c_str() : "?",
                    p.money);
    }
    std::printf("%zu building figures (flags, smoke, status icons)\n", building_figures.size());

    std::printf("%zu ships, %zu islands, %zu sprites, atlas %zu page(s) -> %s (%ux%u)\n", world->ships().size(),
                world->islands().size(), commands.size(), atlas->pages().size(),
                std::string(args.positional[1]).c_str(), image.width, image.height);
    for (const auto& w : warnings) {
        std::fprintf(stderr, "warning: %s\n", w.c_str());
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return usage();
    }
    const std::string_view command = argv[1];
    const Args args(argc, argv);
    if (command == "chunks") {
        return cmd_chunks(args);
    }
    if (command == "cod") {
        return cmd_cod(args);
    }
    if (command == "cod-dump") {
        return cmd_cod_dump(args);
    }
    if (command == "bsh-info") {
        return cmd_bsh_info(args);
    }
    if (command == "bsh-export") {
        return cmd_bsh_export(args);
    }
    if (command == "render-island") {
        return cmd_render_island(args);
    }
    if (command == "render-world") {
        return cmd_render_world(args);
    }
    if (command == "savegames") {
        return cmd_savegames(args);
    }
    if (command == "islands") {
        return cmd_islands(args);
    }
    if (command == "state") {
        return cmd_state(args);
    }
    return usage();
}
