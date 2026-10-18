// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's frame around the map (docs/re/ui.md): the panel on the right with the minimap,
// compass, rotate and zoom buttons and the four modes (CTRL.GAD), and the status bar below the map
// with the status line and the icons of the last three messages (LEIST.GAD). The rest of the panel
// (build menu, windows) comes later.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
        enum class Kind : std::uint8_t { RotateLeft, RotateRight, ZoomIn, ZoomOut, Minimap, Message };
        Kind kind = Kind::RotateLeft;
        double tile_x = 0.0;  ///< Minimap and Message: the world tile to look at
        double tile_y = 0.0;
    };

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
    /// Text of the status line when the mouse is on no gadget (the build tool's report).
    void set_status(std::string text) { m_status = std::move(text); }
    /// The view rotation for the compass, 0..3.
    void set_rotation(std::uint8_t r) noexcept { m_rotation = r; }

    /// Draws the frame. `view` are the corners of the visible map in world tiles (the minimap's
    /// frame), `world` the game for the minimap (redrawn every few seconds).
    void draw(const sim::World& world, const assets::BuildingTable& buildings, const render::BannerTable& banners,
              const std::array<std::pair<double, double>, 4>& view, std::uint64_t now_ms);

private:
    Hud(SDL_Renderer* renderer, const assets::GameDirectory& game, const assets::Palette& palette,
        assets::GadgetLayout ctrl, assets::GadgetLayout leist, const assets::TextTable* texts);
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
    std::vector<std::string> m_leiste;   ///< Text.cod [LEISTE]: help texts of the status line
    std::vector<std::string> m_meldung;  ///< Text.cod [MELDUNG]
    std::string m_status;
    std::uint8_t m_rotation = 0;
    std::deque<Event> m_events;  ///< newest first, at most three
    std::vector<std::uint8_t> m_minimap;
    std::int32_t m_minimap_w = 0;
    std::int32_t m_minimap_h = 0;
    std::uint64_t m_minimap_ms = 0;
    bool m_minimap_dirty = true;
    std::uint32_t m_world_w = 0;
    std::uint32_t m_world_h = 0;
};

}  // namespace o1602::app
