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
//   o1602-tool render-island <file.scp|.szs|.gam> <out.png> [--island N] [--zoom 0|1|2]
//                            [--rotation 0-3] [--anim N] [--no-template] [--base FILE.scp]
//                                                        render an island (needs ANNO1602_DIR)
//
// The palette defaults to $ANNO1602_DIR/TOOLGFX/STADTFLD.COL.

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
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
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/assets/sprite_sheet.hpp"
#include "o1602/core/file.hpp"
#include "o1602/core/png_writer.hpp"
#include "o1602/render/island_renderer.hpp"
#include "o1602/sim/island_map.hpp"

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
        "  o1602-tool render-island <file.scp|.szs|.gam> <out.png> [--island N] [--zoom 0|1|2] [--rotation 0-3]\n"
        "                           [--anim N] [--no-template] [--base FILE.scp]\n",
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

    // Building data from haeuser.cod
    auto cod_bytes = core::read_file(*game->resolve("haeuser.cod"));
    if (!cod_bytes) {
        return fail(cod_bytes.error());
    }
    const auto haeuser = assets::cod::parse(assets::decode_cod(*cod_bytes), assets::cod_predefined_haeuser());
    auto buildings = assets::BuildingTable::from_cod(haeuser);
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
    constexpr const char* kSheets[] = {"gfx/stadtfld.bsh", "mgfx/stadtfld.bsh", "sgfx/stadtfld.bsh"};
    auto sheet = assets::SpriteSheet::load(*game->resolve(kSheets[zoom]));
    if (!sheet) {
        return fail(sheet.error());
    }
    auto palette = assets::Palette::load(*game->resolve("toolgfx/stadtfld.col"));
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
    if (command == "islands") {
        return cmd_islands(args);
    }
    return usage();
}
