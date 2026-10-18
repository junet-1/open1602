// SPDX-License-Identifier: GPL-3.0-or-later
//
// First interactive program: shows the world map of a scenario or savegame and lets the user
// scroll, zoom and rotate it. Becomes the in-game map view once the simulation runs.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "build_tool.hpp"
#include "game_sound.hpp"
#include "hud.hpp"
#include "video_player.hpp"

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
#include "o1602/sim/save_game.hpp"
#include "o1602/sim/savegame_writer.hpp"
#include "o1602/sim/simulation.hpp"
#include "o1602/sim/world.hpp"
#include "o1602/ui/key_bindings.hpp"
#include "o1602/ui/unit_selection.hpp"

union SDL_Event;

namespace o1602::app {

struct ViewerOptions {
    std::filesystem::path scenario;    ///< scenario (.szs / .szm), savegame (.gam) or Open1602 savegame (.o1602)
    std::uint32_t max_frames = 0;      ///< stop after N frames (0 = run until closed); for smoke tests
    std::filesystem::path screenshot;  ///< write the last frame as PNG before exiting
    std::filesystem::path save;        ///< save the game before exiting (.o1602, or .gam for the original)
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
    bool frame = true;    ///< the original's panel and status bar around the map
    bool effects = true;  ///< sounds and voice messages (also off in the original's Game.dat)
    bool music = true;
    bool videos = true;                  ///< the videos of messages, victory, defeat and pause
    bool intro = false;                  ///< the logos and the intro before the game (the original always shows them)
    std::optional<std::uint16_t> video;  ///< one video before the game (VideoSmk number)
    /// The original's mouse: the left button gives orders too, the right one only cancels
    /// (ADR 0010: Open1602 orders with the right button).
    bool original_mouse = false;
    /// Ask before scuttling, disbanding and declaring war by an attack (ADR 0010).
    bool confirm = true;
    /// Mouse input played at given frames (smoke tests of the interface without a person).
    struct ScriptedInput {
        enum class Kind : std::uint8_t { Move, Down, Up, Key };
        Kind kind = Kind::Move;
        std::uint32_t frame = 0;
        float x = 0.0F;
        float y = 0.0F;
        std::int32_t key = 0;  ///< Key: SDL key code
        bool right = false;    ///< Down/Up: the right button
    };
    std::vector<ScriptedInput> input;
    /// Select the building on this island tile at the start (island index, x, y) and look at it.
    std::optional<std::array<std::uint16_t, 3>> select;
    std::optional<std::uint32_t> select_ship;     ///< index into the game's ships
    std::optional<std::uint32_t> select_soldier;  ///< index into the game's land units
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
    void handle_key(int key, std::uint16_t mod);
    void update(std::uint32_t delta_ms);
    void render();
    void update_title(double fps);
    /// Where the camera starts and Home goes: the local player's Kontor, else his first ship,
    /// else the first island.
    [[nodiscard]] render::TilePos home_tile() const;
    core::Status save_screenshot(const std::filesystem::path& path);
    /// Saves the game: Open1602's own savegame, or a .GAM the original loads if the path ends in
    /// .gam. Never into the game's installation.
    core::Status save_game(const std::filesystem::path& path);
    /// Open1602's own folder for savegames (the user's data folder, not the installation).
    [[nodiscard]] static std::filesystem::path save_folder();

    /// Tells the panel which building is under the mouse for its window (none: the player status).
    void select_building_at_mouse();

    // ---- Input (map_input.cpp, ADR 0010) ----
    /// The keys of ADR 0010.
    void setup_keys();
    [[nodiscard]] ui::KeyContext key_context() const;
    void run_action(const ui::KeyBinding& binding);
    /// The map scrolls while its keys are held.
    void scroll_with_keys(std::uint32_t delta_ms);
    /// Screen position of a world position in tiles.
    [[nodiscard]] std::pair<double, double> screen_of(double tx, double ty) const;
    /// Where a unit is drawn on screen: its box (x, y, w, h).
    [[nodiscard]] std::optional<std::array<double, 4>> unit_box(const ui::UnitRef& ref) const;
    /// Ships and land units whose picture lies in the rectangle (screen), with the part inside.
    [[nodiscard]] std::vector<ui::UnitCandidate> units_in(double x0, double y0, double x1, double y1) const;
    /// The unit under a screen point, the local player's first.
    [[nodiscard]] std::optional<ui::UnitRef> unit_at(double sx, double sy) const;
    /// Left button released on the map (no tool): a click or the end of a rectangle.
    void map_left_up(double x, double y, std::uint16_t mod, int clicks);
    /// An order of the selection to the screen point (right button; the left one with the
    /// original's mouse); `queue` with Shift.
    void map_order(double x, double y, bool queue);
    /// An order of the selection to a world position (the minimap).
    void order_to_tile(double tx, double ty, bool queue);
    /// Escape or the right button (original mouse): out of a pick, back a page, deselect.
    void cancel_action();
    /// The selection changed: the windows follow (a single ship in the info mode, groups and land
    /// units in the military mode).
    void selection_changed(bool box);
    /// Orders to the simulation; those that cannot be taken back wait for an answer (ADR 0010).
    void submit_orders(std::vector<sim::Command> orders, bool sound = true);
    [[nodiscard]] std::optional<std::string> question_for(const sim::Command& command) const;
    /// Health bars over the selected units and the selection rectangle.
    void draw_selection_overlay();
    /// Tells the sound where the view is (every frame, before events are heard).
    void place_listener();
    /// Gives the map the window without the frame.
    void lay_out();
    /// What a click on the frame asked for.
    void apply_hud(const Hud::Action& action);
    /// The resource bar: money, and goods and inhabitants of the city under the mouse.
    [[nodiscard]] Hud::Resources resources() const;
    [[nodiscard]] render::TilePos home_or_centre() const;
    /// Shots, hits, victory and defeat: sounds, messages and lines in the terminal.
    void apply_game_events();
    /// A message for the local player with its voice; a message that asks for it slows the game
    /// to normal speed (flag 0x02).
    void announce(std::uint16_t message, std::optional<std::pair<double, double>> tile = std::nullopt);
    /// The same with the tracks of its video (bit i = track i, 0 = all): the messages 0x6B..0x6E
    /// speak to the winner on track 1 and to the others on track 2.
    void announce_with_tracks(std::uint16_t message, std::uint8_t tracks);
    /// Videos (docs/re/videos.md): a new request replaces the running video and the waiting ones,
    /// `intro` ones only queue after each other.
    void request_video(std::uint16_t number, std::uint8_t tracks = 0, bool loop = false, bool intro = false);
    /// Starts the next waiting video, if any; false if none plays.
    bool start_next_video(std::uint64_t now_ms);
    void stop_video();
    /// Input while a video plays: Escape skips (the logos and the intro together), Pause and
    /// Escape end the pause video; clicks on the game are swallowed.
    void handle_video_event(const SDL_Event& event);
    /// The pause of the original (Pause key): the pause video loops; without it the game still
    /// stops (U-018).
    void toggle_pause_video();
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

    static constexpr std::int32_t kMaxPixelScale = 4;

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
    double m_drag_distance = 0.0;  ///< mouse travel while the right button is held (a click if small)
    ui::UnitSelection m_units;     ///< the selected ships or land units
    ui::KeyBindings m_keys;
    /// A pick the next click on the map answers: an attack (A) or a patrol (P).
    enum class Pick : std::uint8_t { None, Attack, Patrol };
    Pick m_pick = Pick::None;
    std::optional<std::pair<double, double>> m_press;  ///< left button pressed on the map, screen
    bool m_rubber_band = false;                        ///< the press became a selection rectangle
    std::vector<sim::Command> m_pending_orders;        ///< waiting for the answer to a question
    std::size_t m_last_group = 0xFF;                   ///< the group key pressed last (twice: go there)
    std::uint64_t m_last_group_ms = 0;
    std::size_t m_formation = 0;  ///< the next formation of the F key
    double m_mouse_x = 0.0;       ///< in output pixels
    double m_mouse_y = 0.0;
    std::size_t m_sprites_drawn = 0;
    BuildTool m_tool;
    std::string m_tool_status;                    ///< what the build tool would do at the mouse
    std::vector<render::EffectFigure> m_effects;  ///< volcano fire, lava, sea creatures, jugglers of this frame
    render::Ambient m_ambient;                    ///< life in the picture, no part of the game
    std::unique_ptr<GameSound> m_sound;           ///< effects, voices and music (after the window)
    std::unique_ptr<Hud> m_hud;                   ///< panel and status bar (after the window), may be null
    bool m_left_down = false;                     ///< the left mouse button, for the frame's buttons
    std::vector<render::TileMark> m_tile_marks;   ///< red ground under what cannot be built
    struct VideoRequest {
        std::uint16_t number = 0;
        std::uint8_t tracks = 0;
        bool loop = false;
        bool intro = false;
    };
    std::optional<sim::BuildBill> m_pending_bill;  ///< costs of the order the build tool shows
    sim::PlayerFigures m_player_figures;           ///< the info panel's figures, refreshed once a second
    std::uint64_t m_figures_ms = 0;
    std::unique_ptr<VideoPlayer> m_video;  ///< after the window; null without videos
    std::deque<VideoRequest> m_videos;     ///< waiting
    VideoRequest m_video_now;              ///< playing
    bool m_pause_video = false;            ///< the original's pause is on
    std::uint32_t m_defeat_variant = 0;    ///< own counter for 1010/2020 (no game random numbers)
};

}  // namespace o1602::app
