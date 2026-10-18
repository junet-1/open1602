// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's frame around the map (docs/re/ui.md): the panel on the right with the minimap,
// compass, rotate and zoom buttons and the four modes (CTRL.GAD), and the status bar below the map
// with the status line and the icons of the last three messages (LEIST.GAD). Below the head the
// mode's panel: the build menu or the player status (side_panels.hpp); the windows of the
// selections come later.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "side_panels.hpp"

#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/render/banner.hpp"
#include "o1602/render/sdl_ui_renderer.hpp"
#include "o1602/sim/world.hpp"
#include "o1602/ui/panel.hpp"

struct SDL_Renderer;

namespace o1602::app {

class Hud {
public:
    /// What a click on the frame asks for.
    struct Action {
        enum class Kind : std::uint8_t {
            RotateLeft,
            RotateRight,
            ZoomIn,
            ZoomOut,
            Minimap,
            Message,
            Build,         ///< build mode with `building`
            Demolish,      ///< build mode, the demolish tool
            TurnBuilding,  ///< the building to place by `step` quarters
            ToolOff,       ///< another mode: no building tool
            OrientToRoad,  ///< buildings with a door turn to the road (`step` 1) or not (0)
        };
        Kind kind = Kind::RotateLeft;
        double tile_x = 0.0;  ///< Minimap and Message: the world tile to look at
        double tile_y = 0.0;
        std::uint32_t building = 0;
        int step = 0;
    };
    /// The modes of the panel (CTRL.GAD).
    static constexpr std::int32_t kModeBuild = 30011;
    static constexpr std::int32_t kModeMilitary = 30012;
    static constexpr std::int32_t kModeInfo = 30013;
    static constexpr std::int32_t kModeOptions = 30014;

    /// Nothing if the layouts or the panel's art are missing (the viewer then runs without frame).
    static std::unique_ptr<Hud> create(SDL_Renderer* renderer, const assets::GameDirectory& game,
                                       const assets::Palette& palette, const assets::TextTable* texts);

    /// Lays the frame out for the window; returns the rectangle left for the map.
    ui::Rect layout(std::int32_t width, std::int32_t height);
    [[nodiscard]] const ui::Rect& game_view() const noexcept { return m_regions.game_view; }
    /// Whether a point belongs to the frame (the map does not see the mouse there).
    [[nodiscard]] bool covers(std::int32_t x, std::int32_t y) const noexcept;

    /// The mouse in window pixels; returns what a click asked for.
    std::optional<Action> mouse(std::int32_t x, std::int32_t y, bool down);

    /// A message for the local player (MELDUNG number) at a world tile, in the colour of `banner`.
    void message(std::uint16_t id, std::uint8_t banner, std::optional<std::pair<double, double>> tile,
                 std::uint64_t now_ms);
    /// What the resource bar at the top of the map shows (Hud_DrawResourceBar 0x0040D160): the
    /// player's money, and of the city under the mouse its tools, wood and bricks (own city) and
    /// inhabitants with the owner's shield. Costs of a pending build order in brackets.
    struct Resources {
        std::optional<std::int64_t> money;  ///< nothing: "???" (a player who is not human)
        std::optional<std::int64_t> money_cost;
        std::optional<std::array<std::int64_t, 3>> goods;  ///< tools, wood, bricks in t (own city)
        std::optional<std::array<std::int64_t, 3>> goods_cost;
        std::optional<std::int64_t> inhabitants;  ///< the city under the mouse (a discovered island)
        std::int32_t shield = -1;                 ///< colour of a human owner, -1 the plain shield
    };
    void set_resources(Resources r) { m_resources = std::move(r); }
    /// Text of the status line when the mouse is on no gadget (the build tool's report).
    void set_status(std::string text) { m_status = std::move(text); }
    /// The view rotation for the compass, 0..3.
    void set_rotation(std::uint8_t r) noexcept { m_rotation = r; }
    /// The rotation of the building to place (the preview turns with it).
    void set_building_rotation(std::uint8_t r) noexcept { m_building_rotation = r; }
    /// Which buildings the local player may pick (the build menu).
    void set_available(BuildPanel::Available available) { m_available = std::move(available); }
    [[nodiscard]] std::int32_t mode() const noexcept { return m_mode; }
    /// Switches the mode (also from the keys); returns what the tool should do.
    std::optional<Action> set_mode(std::int32_t mode);

    /// Draws the frame. `view` are the corners of the visible map in world tiles (the minimap's
    /// frame), `world` the game for the minimap (redrawn every few seconds).
    void draw(const sim::World& world, const assets::BuildingTable& buildings, const render::BannerTable& banners,
              const std::array<std::pair<double, double>, 4>& view, std::uint64_t now_ms,
              const sim::PlayerFigures& figures, std::uint8_t colour);

private:
    Hud(SDL_Renderer* renderer, const assets::GameDirectory& game, const assets::Palette& palette,
        assets::GadgetLayout ctrl, assets::GadgetLayout leist, const assets::TextTable* texts);
    /// What the build tool should do in the build mode now.
    [[nodiscard]] std::optional<Action> build_tool_action();
    void draw_resources();
    void build_minimap(const sim::World& world, const assets::BuildingTable& buildings,
                       const render::BannerTable& banners);
    [[nodiscard]] ui::Rect minimap_rect() const;

    struct Event {
        std::uint16_t id = 0;
        std::uint8_t banner = 0;
        std::optional<std::pair<double, double>> tile;
        std::uint64_t time_ms = 0;
    };

    SDL_Renderer* m_sdl;
    const assets::GameDirectory* m_game;
    assets::Palette m_palette;
    std::unique_ptr<render::SdlUiRenderer> m_renderer;
    ui::Panel m_ctrl;
    ui::Panel m_leist;
    ui::ScreenRegions m_regions;
    std::optional<assets::TextTable> m_texts;
    std::vector<std::string> m_leiste;   ///< Text.cod [LEISTE]: help texts of the status line
    std::vector<std::string> m_meldung;  ///< Text.cod [MELDUNG]
    std::string m_status;
    std::uint8_t m_rotation = 0;
    std::uint8_t m_building_rotation = 0;
    std::int32_t m_mode = kModeInfo;  ///< the original starts in the info mode (0x0042BEF7)
    std::unique_ptr<BuildPanel> m_build;
    std::unique_ptr<InfoPanel> m_info;
    BuildPanel::Available m_available;
    Resources m_resources;
    std::int32_t m_gfx_baumat = 46;  ///< TOOLS.INC GFXBAUMAT
    std::deque<Event> m_events;      ///< newest first, at most three
    std::vector<std::uint8_t> m_minimap;
    std::int32_t m_minimap_w = 0;
    std::int32_t m_minimap_h = 0;
    std::uint64_t m_minimap_ms = 0;
    bool m_minimap_dirty = true;
    std::uint32_t m_world_w = 0;
    std::uint32_t m_world_h = 0;
};

}  // namespace o1602::app
