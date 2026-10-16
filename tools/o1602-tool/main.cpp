// SPDX-License-Identifier: GPL-3.0-or-later
//
// o1602-tool - inspect and convert original Anno 1602 data files.
//
//   o1602-tool chunks     <file>                         list chunks of any chunk container
//   o1602-tool cod        <file.cod> [out.txt]           decode a .cod script to UTF-8 text
//   o1602-tool bsh-info   <file.bsh|.zei>                list sprites
//   o1602-tool bsh-export <file.bsh|.zei> <outdir> [--palette FILE] [--sheet] [--first N] [--count N]
//   o1602-tool islands    <file.scp|.szs|.szm|.gam>      list islands and tile statistics
//
// The palette defaults to $ANNO1602_DIR/TOOLGFX/STADTFLD.COL.

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/cod.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/island.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/assets/sprite_sheet.hpp"
#include "o1602/core/file.hpp"
#include "o1602/core/png_writer.hpp"

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
        "  o1602-tool bsh-info   <file.bsh|.zei>\n"
        "  o1602-tool bsh-export <file.bsh|.zei> <outdir> [--palette FILE] [--sheet] [--first N] [--count N]\n"
        "  o1602-tool islands    <file.scp|.szs|.szm|.gam>\n",
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
    if (command == "bsh-info") {
        return cmd_bsh_info(args);
    }
    if (command == "bsh-export") {
        return cmd_bsh_export(args);
    }
    if (command == "islands") {
        return cmd_islands(args);
    }
    return usage();
}
