// SPDX-License-Identifier: GPL-3.0-or-later
//
// First interactive program: shows the world map of a scenario or savegame and lets the user
// scroll, zoom and rotate it. Becomes the in-game map view once the simulation runs.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/core/result.hpp"
#include "o1602/platform/window.hpp"
#include "o1602/render/animation_clock.hpp"
#include "o1602/render/camera.hpp"
#include "o1602/render/sdl_sprite_renderer.hpp"
#include "o1602/render/sprite_atlas.hpp"
#include "o1602/sim/world.hpp"

union SDL_Event;

namespace o1602::app {

struct ViewerOptions {
    std::filesystem::path scenario;    ///< .szs / .szm / .gam
    std::uint32_t max_frames = 0;      ///< stop after N frames (0 = run until closed); for smoke tests
    std::filesystem::path screenshot;  ///< write the last frame as PNG before exiting
    render::Zoom zoom = render::Zoom::Large;
    std::int32_t width = 1280;
    std::int32_t height = 800;
};

class MapViewer {
public:
    static core::Result<std::unique_ptr<MapViewer>> create(const assets::GameDirectory& game,
                                                           const ViewerOptions& options);
    ~MapViewer();
    MapViewer(const MapViewer&) = delete;
    MapViewer& operator=(const MapViewer&) = delete;

    /// Main loop; returns the process exit code.
    int run();

private:
    MapViewer(assets::GameDirectory game, assets::BuildingTable buildings, sim::World world, assets::Palette palette,
              ViewerOptions options);

    /// Sprites of one zoom level: atlas (CPU side only rects remain) and its GPU textures.
    struct ZoomGraphics {
        render::SpriteAtlas atlas;
        std::unique_ptr<render::SdlSpriteRenderer> renderer;
    };
    core::Status ensure_graphics(render::Zoom zoom);

    void handle_event(const SDL_Event& event);
    void handle_key(int key, bool shift);
    void update(std::uint32_t delta_ms);
    void render();
    void update_title(double fps);
    core::Status save_screenshot(const std::filesystem::path& path);

    void zoom_by(int steps, double sx, double sy);

    assets::GameDirectory m_game;
    assets::BuildingTable m_buildings;
    sim::World m_world;
    assets::Palette m_palette;
    ViewerOptions m_options;

    std::unique_ptr<platform::SdlSystem> m_sdl;
    std::unique_ptr<platform::Window> m_window;
    std::array<std::optional<ZoomGraphics>, 3> m_graphics;

    render::Camera m_camera;
    render::AnimationClock m_clock;
    std::vector<render::DrawCommand> m_commands;  ///< reused every frame

    bool m_running = true;
    bool m_paused = false;
    bool m_dragging = false;
    double m_mouse_x = 0.0;  ///< in output pixels
    double m_mouse_y = 0.0;
    std::size_t m_sprites_drawn = 0;
};

}  // namespace o1602::app
