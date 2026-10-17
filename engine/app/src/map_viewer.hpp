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

#include "build_tool.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/core/result.hpp"
#include "o1602/platform/window.hpp"
#include "o1602/render/ambient.hpp"
#include "o1602/render/animation_clock.hpp"
#include "o1602/render/camera.hpp"
#include "o1602/render/land_figures.hpp"
#include "o1602/render/sdl_sprite_renderer.hpp"
#include "o1602/render/sprite_atlas.hpp"
#include "o1602/render/walker_figures.hpp"
#include "o1602/render/world_graphics.hpp"
#include "o1602/sim/new_game.hpp"
#include "o1602/sim/savegame_writer.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/sim/world.hpp"

union SDL_Event;

namespace o1602::app {

struct ViewerOptions {
    std::filesystem::path scenario;    ///< scenario (.szs / .szm) or savegame (.gam)
    std::uint32_t max_frames = 0;      ///< stop after N frames (0 = run until closed); for smoke tests
    std::filesystem::path screenshot;  ///< write the last frame as PNG before exiting
    std::filesystem::path save;        ///< write the game as a savegame (.gam) before exiting
    render::Zoom zoom = render::Zoom::Large;
    bool show_owners = false;  ///< start with the ownership view
    /// Show figures at the smallest zoom level from half-size graphics of the middle level (the
    /// original shows none there). Off: only the sheets the original has.
    bool derive_small_figures = true;
    /// The original red/blue/yellow/white flag images instead of the tinted generic flag.
    bool original_flags = false;
    std::int32_t width = 1280;
    std::int32_t height = 800;
    sim::Rules rules;  ///< settings of the game (refund on demolishing, ...)
    /// Start a new endless game instead of loading `scenario`.
    std::optional<sim::NewGameOptions> new_game;
    /// How the islands of a scenario that makes them at the start are drawn (also --scenario
    /// Szenes/Endlosspiel00.szs).
    sim::WorldStart world;
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
              assets::FigureTable figures, ViewerOptions options);

    /// Sprites of one zoom level: atlas (CPU side only rects remain) and its GPU textures.
    struct ZoomGraphics {
        render::WorldGraphics graphics;
        std::unique_ptr<render::SdlSpriteRenderer> renderer;
    };
    core::Status ensure_graphics(render::Zoom zoom);

    void handle_event(const SDL_Event& event);
    void handle_key(int key, bool shift);
    void update(std::uint32_t delta_ms);
    void render();
    void update_title(double fps);
    /// Where the camera starts and Home goes: the local player's Kontor, else his first ship,
    /// else the first island.
    [[nodiscard]] render::TilePos home_tile() const;
    core::Status save_screenshot(const std::filesystem::path& path);
    /// Writes the game as a savegame the original can load. Never into the game's installation.
    core::Status save_game(const std::filesystem::path& path);
    /// Open1602's own folder for savegames (the user's data folder, not the installation).
    [[nodiscard]] static std::filesystem::path save_folder();

    void zoom_by(int steps, double sx, double sy);
    /// Island tile under the mouse, nullopt over the sea.
    [[nodiscard]] std::optional<BuildTool::Spot> spot_at_mouse() const;
    /// Results of building and demolishing (messages, flags of new buildings).
    void apply_construction();
    /// Animation phases of buildings that started or stopped work (presentation only).
    void apply_production_events();
    /// Volcano fire and lava of the current game time (m_effects).
    void collect_effects();
    /// Townsfolk, sea creatures and jugglers of the game time since the last frame.
    void update_ambient();

    assets::GameDirectory m_game;
    assets::BuildingTable m_buildings;
    sim::World m_world;
    assets::Palette m_palette;
    assets::FigureTable m_figures;
    render::BannerTable m_banners;  ///< colours and flags of the players (ADR 0007)
    /// Flags, smoke and icons: buildings do not move, so they are collected once at load.
    std::vector<render::BuildingFigure> m_building_figures;
    sim::PlayerId m_local_player = sim::kNoPlayer;  ///< whose buildings show status icons
    render::SoldierFigures m_soldiers;              ///< land units in their owners' colours
    render::WalkerFigures m_walkers;                ///< carriers, carts, workers, deer and hunters
    ViewerOptions m_options;
    sim::Simulation m_simulation;  ///< runs the economy on m_world (after m_world, m_buildings, m_figures)

    std::unique_ptr<platform::SdlSystem> m_sdl;
    std::unique_ptr<platform::Window> m_window;
    std::array<std::optional<ZoomGraphics>, 3> m_graphics;

    render::Camera m_camera;
    render::AnimationClock m_clock;
    std::vector<render::DrawCommand> m_commands;  ///< reused every frame

    bool m_running = true;
    bool m_paused = false;
    bool m_show_owners = false;  ///< ownership overlay (key O)
    bool m_dragging = false;
    double m_mouse_x = 0.0;  ///< in output pixels
    double m_mouse_y = 0.0;
    std::size_t m_sprites_drawn = 0;
    BuildTool m_tool;
    std::string m_tool_status;                    ///< what the build tool would do at the mouse
    std::vector<render::EffectFigure> m_effects;  ///< volcano fire, lava, sea creatures, jugglers of this frame
    render::Ambient m_ambient;                    ///< life in the picture, no part of the game
    std::vector<render::TileMark> m_tile_marks;   ///< red ground under what cannot be built
};

}  // namespace o1602::app
