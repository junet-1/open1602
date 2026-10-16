// SPDX-License-Identifier: GPL-3.0-or-later
//
// open1602 - map viewer (milestone M2). Usage:
//
//   open1602 [--game-dir DIR] [--scenario FILE | --savegame NAME] [--size WxH] [--zoom 0|1|2]
//            [--owners] [--frames N] [--screenshot out.png]
//
// The game directory defaults to $ANNO1602_DIR, the scenario to "Szenes/Das Atoll.szs".
// A relative scenario path is looked up in the game directory first. --savegame NAME opens
// SAVEGAME/NAME.gam of the installation (scenarios and savegames share one format).

#include <SDL3/SDL_main.h>
#include <charconv>
#include <cstdio>
#include <string_view>

#include "map_viewer.hpp"

using namespace o1602;

namespace {

int usage() {
    std::fputs(
        "usage: open1602 [--game-dir DIR] [--scenario FILE | --savegame NAME] [--size WxH] [--zoom 0|1|2]\n"
        "                [--owners] [--frames N] [--screenshot out.png]\n"
        "\n"
        "  arrows/WASD    scroll (Shift: faster)      right/middle mouse drag  scroll\n"
        "  mouse wheel    zoom at cursor              1 / 2 / 3                zoom level\n"
        "  R / Shift+R    rotate view                 + / -                    pixel scale\n"
        "  Space          pause animations            Home                     first island\n"
        "  O              ownership view              F12                      screenshot\n"
        "  Esc / Q        quit\n",
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
        savegame_path = "Savegame/" + std::string(savegame);
        if (!savegame_path.ends_with(".gam") && !savegame_path.ends_with(".GAM")) {
            savegame_path += ".gam";
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
