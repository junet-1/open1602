// SPDX-License-Identifier: GPL-3.0-or-later
//
// open1602 - map viewer (milestone M2). Usage:
//
//   open1602 [--game-dir DIR] [--scenario FILE | --savegame NAME] [--size WxH] [--zoom 0|1|2]
//            [--owners] [--original-sheets] [--frames N] [--screenshot out.png]
//
// The game directory defaults to $ANNO1602_DIR, the scenario to "Szenes/Das Atoll.szs".
// A relative scenario path is looked up in the game directory first. --savegame NAME opens a
// savegame by the name shown in the game (looked up in Game.dat, e.g. "Merlin01" ->
// SAVEGAME/game00.gam) or by file name. Scenarios and savegames share one format.

#include <SDL3/SDL_main.h>
#include <charconv>
#include <cstdio>
#include <string_view>

#include "map_viewer.hpp"

#include "o1602/assets/scenario.hpp"
#include "o1602/core/file.hpp"
#include "o1602/core/text.hpp"

using namespace o1602;

namespace {

int usage() {
    std::fputs(
        "usage: open1602 [--game-dir DIR] [--scenario FILE | --savegame NAME] [--size WxH] [--zoom 0|1|2]\n"
        "                [--owners] [--original-sheets] [--original-flags] [--refund MODE] [--frames N]\n"
        "                [--screenshot out.png]\n"
        "  --original-sheets  no figures at the smallest zoom level, as in the original\n"
        "  --original-flags   original flag images instead of the tinted generic flag\n"
        "  --refund MODE      what demolishing gives back: none (original), partial[:PERCENT] (default 50), full\n"
        "\n"
        "  arrows/WASD    scroll (Shift: faster)      right/middle mouse drag  scroll\n"
        "  mouse wheel    zoom at cursor              1 / 2 / 3                zoom level\n"
        "  R / Shift+R    rotate view                 + / -                    pixel scale\n"
        "  Space          pause the game              Home                     first island\n"
        "  PageUp/PageDn  game speed x2 / /2\n"
        "  O              ownership view              F12                      screenshot\n"
        "  B              build tool                  X                        demolish tool\n"
        "  Tab / Shift+Tab next / previous building   , / .                    turn the building\n"
        "  left mouse     build or demolish (drag: roads, walls, fields, areas)\n"
        "  Esc            leave the tool, else quit   Q                        quit\n",
        stderr);
    return 2;
}

bool parse_number(std::string_view text, std::int32_t& value) {
    return std::from_chars(text.data(), text.data() + text.size(), value).ec == std::errc{};
}

}  // namespace

int main(int argc, char** argv) {
    std::string_view game_dir;
    std::string_view scenario = "Szenes/Das Atoll.szs";
    std::string_view savegame;
    app::ViewerOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--game-dir" && has_value) {
            game_dir = argv[++i];
        } else if (arg == "--scenario" && has_value) {
            scenario = argv[++i];
        } else if (arg == "--savegame" && has_value) {
            savegame = argv[++i];
        } else if (arg == "--owners") {
            options.show_owners = true;
        } else if (arg == "--original-sheets") {
            options.derive_small_figures = false;
        } else if (arg == "--original-flags") {
            options.original_flags = true;
        } else if (arg == "--frames" && has_value) {
            std::int32_t frames = 0;
            if (!parse_number(argv[++i], frames) || frames < 0) {
                return usage();
            }
            options.max_frames = static_cast<std::uint32_t>(frames);
        } else if (arg == "--zoom" && has_value) {
            std::int32_t zoom = 0;
            if (!parse_number(argv[++i], zoom) || zoom < 0 || zoom > 2) {
                return usage();
            }
            options.zoom = static_cast<render::Zoom>(zoom);
        } else if (arg == "--screenshot" && has_value) {
            options.screenshot = argv[++i];
        } else if (arg == "--refund" && has_value) {
            // Game setting: what demolishing gives back (none as in the original, partial[:N], full).
            const std::string_view mode = argv[++i];
            if (mode == "none") {
                options.rules.demolish_refund = sim::Refund::None;
            } else if (mode == "full") {
                options.rules.demolish_refund = sim::Refund::Full;
            } else if (mode.starts_with("partial")) {
                options.rules.demolish_refund = sim::Refund::Partial;
                if (mode.size() > 8 && mode[7] == ':') {
                    std::int32_t percent = 0;
                    if (!parse_number(mode.substr(8), percent) || percent < 1 || percent > 100) {
                        return usage();
                    }
                    options.rules.refund_percent = static_cast<std::uint8_t>(percent);
                }
            } else {
                return usage();
            }
        } else if (arg == "--size" && has_value) {
            const std::string_view size = argv[++i];
            const auto x = size.find('x');
            if (x == std::string_view::npos || !parse_number(size.substr(0, x), options.width) ||
                !parse_number(size.substr(x + 1), options.height)) {
                return usage();
            }
        } else {
            return usage();
        }
    }

    auto game = game_dir.empty() ? assets::GameDirectory::from_environment()
                                 : assets::GameDirectory::open(std::filesystem::path(game_dir));
    if (!game) {
        std::fprintf(stderr, "error: %s\n(set ANNO1602_DIR or pass --game-dir)\n", game.error().message.c_str());
        return 1;
    }
    std::string savegame_path;
    if (!savegame.empty()) {
        // The name shown in the game (Game.dat, "SPIELNAME") or a file name in SAVEGAME/.
        if (const auto dat = game->resolve("Game.dat")) {
            if (auto bytes = core::read_file(*dat)) {
                const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
                for (const auto& slot : assets::parse_savegame_slots(text)) {
                    if (core::equals_ignore_case(slot.name, savegame)) {
                        savegame_path = slot.file;
                    }
                }
            }
        }
        if (savegame_path.empty()) {
            savegame_path = "Savegame/" + std::string(savegame);
            if (!savegame_path.ends_with(".gam") && !savegame_path.ends_with(".GAM")) {
                savegame_path += ".gam";
            }
        }
        scenario = savegame_path;
    }
    if (const auto resolved = game->resolve(scenario)) {
        options.scenario = *resolved;
    } else {
        options.scenario = std::filesystem::path(scenario);
    }

    auto viewer = app::MapViewer::create(*game, options);
    if (!viewer) {
        std::fprintf(stderr, "error: %s\n", viewer.error().message.c_str());
        return 1;
    }
    return (*viewer)->run();
}
