// SPDX-License-Identifier: GPL-3.0-or-later
#include "hud.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

#include "o1602/audio/game_audio.hpp"
#include "o1602/sim/pathfinding.hpp"

namespace o1602::app {

namespace {

// Gadgets of CTRL.GAD and LEIST.GAD.
constexpr std::int32_t kRotateLeft = 30001;
constexpr std::int32_t kRotateRight = 30002;
constexpr std::int32_t kZoomIn = 30003;
constexpr std::int32_t kZoomOut = 30004;
constexpr std::int32_t kMinimap = 30005;
constexpr std::int32_t kCompass = 30006;
constexpr std::int32_t kFirstMode = 30011;  // build, military, info, options
constexpr std::int32_t kStatusLine = 100001;
constexpr std::int32_t kFirstEvent = 100011;
constexpr std::size_t kEvents = 3;
// TOOLS.INC: the compass (GFXMAP + 1 + rotation) and the message icons (GFXLEISTE).
constexpr std::int32_t kGfxParchment = 17;
constexpr std::int32_t kGfxCompass = 18;
constexpr std::int32_t kGfxLeiste = 253;
constexpr std::uint64_t kEventMs = 45'000;   // an icon stays this long (449 time units of 0x00430F20)
constexpr std::uint64_t kMinimapMs = 2'000;  // the minimap is redrawn this often

/// Help text line of [LEISTE] for the gadgets of the panel's head (in the order of the texts).
std::optional<std::size_t> help_line(std::int32_t id) {
    switch (id) {
        case kRotateLeft:
            return 2;
        case kRotateRight:
            return 3;
        case kZoomIn:
            return 4;
        case kZoomOut:
            return 5;
        default:
            break;
    }
    if (id >= kFirstMode && id < kFirstMode + 4) {
        return static_cast<std::size_t>(6 + id - kFirstMode);
    }
    return std::nullopt;
}

/// The icon of a message (Event_UpdateStatusIcons 0x0043AFA0): types 0..3 by colour (! event,
/// scroll treaty or trade, swords attack, lips other players), 4..7 plague, fire, drought, volcano.
/// Which message has which type is not documented; this follows the icons.
std::uint8_t event_type(std::uint16_t message) {
    switch (message) {
        case 0x01:
            return 4;
        case 0x02:
            return 5;
        case 0x03:
            return 6;
        case 0x5E:
            return 7;
        default:
            break;
    }
    const audio::MessageAudio* m = audio::message_audio(message);
    const std::uint8_t kind = m != nullptr ? m->kind : 0;
    return kind == 5 ? 2 : kind == 4 || kind == 7 ? 1 : kind == 3 ? 3 : 0;
}

}  // namespace

std::unique_ptr<Hud> Hud::create(SDL_Renderer* renderer, const assets::GameDirectory& game,
                                 const assets::Palette& palette, const assets::TextTable* texts) {
    auto ctrl = assets::load_gad(game, "CTRL.GAD");
    auto leist = assets::load_gad(game, "LEIST.GAD");
    if (!ctrl || !leist || !game.resolve(ui::sheet_file(0, ui::ArtSize::Large))) {
        return nullptr;
    }
    return std::unique_ptr<Hud>(
        new Hud(renderer, game, palette, std::move(ctrl).value(), std::move(leist).value(), texts));
}

Hud::Hud(SDL_Renderer* renderer, const assets::GameDirectory& game, const assets::Palette& palette,
         assets::GadgetLayout ctrl, assets::GadgetLayout leist, const assets::TextTable* texts)
    : m_sdl(renderer),
      m_game(&game),
      m_palette(palette),
      m_renderer(std::make_unique<render::SdlUiRenderer>(renderer, game, palette, ui::ArtSize::Large)),
      m_ctrl(std::move(ctrl), 1.0),
      m_leist(std::move(leist), 1.0) {
    if (texts != nullptr) {
        m_texts = *texts;
        m_leiste = texts->section("LEISTE");
        m_meldung = texts->section("MELDUNG");
    }
    const assets::TextTable* own = m_texts ? &*m_texts : nullptr;
    for (std::size_t i = 0; i < kEvents; ++i) {
        m_leist.set_hidden(kFirstEvent + static_cast<std::int32_t>(i), true);
    }
    // The panels of the modes; without their layouts the head stays alone.
    if (auto bau = assets::load_gad(game, "BAU.GAD")) {
        m_build = std::make_unique<BuildPanel>(std::move(bau).value(), ui::original_build_menu(), own);
    }
    if (auto info = assets::load_gad(game, "INFO.GAD")) {
        m_info = std::make_unique<InfoPanel>(std::move(info).value(), own);
    }
    m_ctrl.set_selected(m_mode, true);
    m_gfx_baumat = m_ctrl.layout().constant("GFXBAUMAT").value_or(m_gfx_baumat);
}

void Hud::draw_resources() {
    // Design units from the top left of the map, icons at the top, text cells ending at 30.
    const double s = m_regions.scale;
    const ui::Rect& v = m_regions.game_view;
    const std::int32_t x0 = v.x + static_cast<std::int32_t>(5.5 * s);
    std::vector<ui::DrawItem> items;
    const auto icon = [&](std::int32_t gfx, std::int32_t x) {
        ui::DrawItem d;
        d.kind = ui::DrawItem::Kind::Sprite;
        d.gfx = gfx;
        d.x = ui::to_pixels(x, s);
        d.y = 0;
        items.push_back(d);
    };
    const auto text = [&](std::string t, std::int32_t x) {
        ui::DrawItem d;
        d.kind = ui::DrawItem::Kind::Text;
        d.block = 0;   // ZEINR
        d.colour = 4;  // light grey (240, 240, 240)
        d.x = ui::to_pixels(x, s);
        d.y = ui::to_pixels(30, s);
        d.text = std::move(t);
        items.push_back(d);
    };
    const auto with_cost = [](std::int64_t value, const std::optional<std::int64_t>& cost) {
        return cost ? std::to_string(value) + " (" + std::to_string(*cost) + ")" : std::to_string(value);
    };
    const Resources& r = m_resources;
    icon(m_gfx_baumat + 11, 16);
    text(r.money ? with_cost(*r.money, r.money_cost) : std::string("???"), 46);
    if (r.goods) {
        // Werkzeug, Holz, Ziegel: GFXBAUMAT + 13, 14, 12.
        const std::array<std::int32_t, 3> icons{m_gfx_baumat + 13, m_gfx_baumat + 14, m_gfx_baumat + 12};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto x = static_cast<std::int32_t>(296 + 110 * i);
            icon(icons[i], x);
            const auto cost = r.goods_cost ? std::optional<std::int64_t>((*r.goods_cost)[i]) : std::nullopt;
            text(with_cost((*r.goods)[i], cost), x + 30);
        }
    }
    if (r.inhabitants) {
        if (r.shield >= 0) {
            icon(m_gfx_baumat + 17 + std::min(r.shield, 3), 666);
        } else {
            icon(m_gfx_baumat + 15, 646);
        }
        text(std::to_string(*r.inhabitants), 696);
    }
    m_renderer->draw(items, x0, v.y, s);
}

std::optional<Hud::Action> Hud::build_tool_action() {
    if (!m_build) {
        return std::nullopt;
    }
    if (m_build->demolish()) {
        return Action{Action::Kind::Demolish};
    }
    const BuildPanel::Available available =
        m_available ? m_available : BuildPanel::Available([](std::uint32_t) { return true; });
    if (const auto id = m_build->current(available)) {
        Action a{Action::Kind::Build};
        a.building = *id;
        return a;
    }
    return Action{Action::Kind::ToolOff};
}

std::optional<Hud::Action> Hud::set_mode(std::int32_t mode) {
    if (mode < kModeBuild || mode > kModeOptions) {
        return std::nullopt;
    }
    m_ctrl.set_selected(m_mode, false);
    m_mode = mode;
    m_ctrl.set_selected(m_mode, true);
    if (m_build) {
        m_build->close_popup();
    }
    // Hud_SetMode 0x0043B660: the build mode arms the remembered building again, the others put
    // the tool away.
    return mode == kModeBuild ? build_tool_action() : std::optional<Action>(Action{Action::Kind::ToolOff});
}

ui::Rect Hud::layout(std::int32_t width, std::int32_t height) {
    // The whole panel stays on screen: the smaller of the two scales (the original: width / 1024).
    const double scale =
        std::min(static_cast<double>(width) / ui::kDesignWidth, static_cast<double>(height) / ui::kDesignHeight);
    m_regions = ui::screen_regions(width, height, scale);
    m_ctrl.set_scale(scale);
    m_leist.set_scale(scale);
    if (m_build) {
        m_build->set_scale(scale);
    }
    if (m_info) {
        m_info->set_scale(scale);
    }
    return m_regions.game_view;
}

bool Hud::covers(std::int32_t x, std::int32_t y) const noexcept {
    return m_regions.side_panel.contains(x, y) || m_regions.status_bar.contains(x, y);
}

ui::Rect Hud::minimap_rect() const {
    const assets::Gadget* g = m_ctrl.layout().find(kMinimap);
    if (g == nullptr) {
        return {};
    }
    const double s = m_regions.scale;
    return {m_regions.side_panel.x + ui::to_pixels(g->pos[0], s), m_regions.side_panel.y + ui::to_pixels(g->pos[1], s),
            static_cast<std::int32_t>(g->size[0] * s), static_cast<std::int32_t>(g->size[1] * s)};
}

std::optional<Hud::Action> Hud::mouse(std::int32_t x, std::int32_t y, bool down) {
    std::optional<Action> action;
    const ui::Rect& panel = m_regions.side_panel;
    const ui::Rect& bar = m_regions.status_bar;
    // The mode's panel first: an open pop-up takes every press on the side panel.
    if (m_mode == kModeBuild && m_build) {
        const BuildPanel::Available available =
            m_available ? m_available : BuildPanel::Available([](std::uint32_t) { return true; });
        const bool popup = m_build->popup_open();
        bool consumed = false;
        if (const auto a = m_build->mouse(x - panel.x, y - panel.y, down, available, consumed)) {
            switch (a->kind) {
                case BuildPanel::Action::Kind::Pick:
                    action = Action{Action::Kind::Build};
                    action->building = a->building;
                    break;
                case BuildPanel::Action::Kind::Demolish:
                    action = Action{Action::Kind::Demolish};
                    break;
                case BuildPanel::Action::Kind::Rotate:
                    action = Action{Action::Kind::TurnBuilding};
                    action->step = a->step;
                    break;
                case BuildPanel::Action::Kind::OrientToRoad:
                    action = Action{Action::Kind::OrientToRoad};
                    action->step = a->step;
                    break;
            }
        }
        if (popup && consumed) {
            return action;
        }
    } else if (m_mode == kModeInfo && m_info) {
        m_info->mouse(x - panel.x, y - panel.y, down);
    }
    const auto ctrl = m_ctrl.mouse(x - panel.x, y - panel.y, down);
    const auto leist = m_leist.mouse(x - bar.x, y - bar.y, down);
    for (const ui::PanelEvent& e : ctrl) {
        if (e.kind != ui::PanelEvent::Kind::Clicked) {
            continue;
        }
        switch (e.id) {
            case kRotateLeft:
                action = Action{Action::Kind::RotateLeft};
                break;
            case kRotateRight:
                action = Action{Action::Kind::RotateRight};
                break;
            case kZoomIn:
                action = Action{Action::Kind::ZoomIn};
                break;
            case kZoomOut:
                action = Action{Action::Kind::ZoomOut};
                break;
            default:
                if (e.id >= kModeBuild && e.id <= kModeOptions) {
                    action = set_mode(e.id);
                }
                break;
        }
    }
    for (const ui::PanelEvent& e : leist) {
        const auto k = static_cast<std::size_t>(e.id - kFirstEvent);
        if (e.kind == ui::PanelEvent::Kind::Clicked && k < m_events.size() && m_events[k].tile) {
            action = Action{Action::Kind::Message, m_events[k].tile->first, m_events[k].tile->second};
        }
    }
    // The minimap: pressing or dragging on it moves the view there.
    const ui::Rect map = minimap_rect();
    if (down && map.contains(x, y) && m_world_w > 0 && m_world_h > 0) {
        const double fit = std::min(static_cast<double>(map.w) / m_world_w, static_cast<double>(map.h) / m_world_h);
        const double ox = map.x + (map.w - m_world_w * fit) / 2.0;
        const double oy = map.y + (map.h - m_world_h * fit) / 2.0;
        const double tx = (x - ox) / fit;
        const double ty = (y - oy) / fit;
        if (tx >= 0.0 && ty >= 0.0 && tx < m_world_w && ty < m_world_h) {
            action = Action{Action::Kind::Minimap, tx, ty};
        }
    }
    return action;
}

void Hud::message(std::uint16_t id, std::uint8_t banner, std::optional<std::pair<double, double>> tile,
                  std::uint64_t now_ms) {
    // Event_Post 0x00430D50: the same message at the same place only refreshes its time; a new one
    // pushes the oldest out.
    for (Event& e : m_events) {
        if (e.id == id && e.tile == tile) {
            e.time_ms = now_ms;
            return;
        }
    }
    m_events.push_front(Event{id, banner, tile, now_ms});
    if (m_events.size() > kEvents) {
        m_events.pop_back();
    }
}

void Hud::build_minimap(const sim::World& world, const assets::BuildingTable& buildings,
                        const render::BannerTable& banners) {
    m_world_w = world.width();
    m_world_h = world.height();
    m_minimap.assign(std::size_t{m_world_w} * m_world_h * 4, 0);
    for (std::uint32_t y = 0; y < m_world_h; ++y) {
        for (std::uint32_t x = 0; x < m_world_w; ++x) {
            const sim::World::Cell& c = world.cell(x, y);
            std::array<std::uint8_t, 3> rgb{38, 70, 128};  // sea
            if (c.island != sim::World::kSea) {
                const sim::MapCell& top = world.islands()[c.island].map.top(c.local_x, c.local_y);
                const assets::BuildingType* b = top.empty() ? nullptr : buildings.by_sprite(top.sprite);
                const std::int32_t kind = b != nullptr ? b->kind : sim::building_kind::kSea;
                namespace bk = sim::building_kind;
                const bool water = kind == bk::kSea || kind == bk::kSurf || kind == bk::kSurfCorner;
                rgb = water                 ? std::array<std::uint8_t, 3>{38, 70, 128}
                      : kind == bk::kForest ? std::array<std::uint8_t, 3>{60, 96, 40}
                                            : std::array<std::uint8_t, 3>{112, 132, 66};
                const bool scenery = water || kind == bk::kForest || kind == bk::kGround || kind == bk::kRiverMouth;
                if (!scenery && top.owner != sim::kNoPlayer) {
                    const sim::Player* p = world.player(top.owner);
                    const render::Banner* banner = p != nullptr ? banners.get(p->banner) : nullptr;
                    if (banner != nullptr) {
                        rgb = {banner->color.r, banner->color.g, banner->color.b};
                    }
                }
            }
            const std::size_t i = (std::size_t{y} * m_world_w + x) * 4;
            m_minimap[i] = rgb[0];
            m_minimap[i + 1] = rgb[1];
            m_minimap[i + 2] = rgb[2];
            m_minimap[i + 3] = 255;
        }
    }
}

void Hud::draw(const sim::World& world, const assets::BuildingTable& buildings, const render::BannerTable& banners,
               const std::array<std::pair<double, double>, 4>& view, std::uint64_t now_ms,
               const sim::PlayerFigures& figures, std::uint8_t colour) {
    const double s = m_regions.scale;
    draw_resources();
    // Black under the art where the window is larger than it.
    m_renderer->fill(m_regions.side_panel, 0);
    m_renderer->fill(m_regions.status_bar, 0);
    // Help text: the gadget under the mouse, else the status of the game.
    std::string status = m_status;
    if (const auto id = m_ctrl.hovered()) {
        if (const auto line = help_line(*id); line && *line < m_leiste.size()) {
            status = m_leiste[*line];
        }
    }
    // Message icons: expired ones go, the others show their type in the player's colour.
    std::erase_if(m_events, [&](const Event& e) { return now_ms > e.time_ms + kEventMs; });
    for (std::size_t i = 0; i < kEvents; ++i) {
        const auto id = kFirstEvent + static_cast<std::int32_t>(i);
        if (i >= m_events.size()) {
            m_leist.set_hidden(id, true);
            continue;
        }
        const Event& e = m_events[i];
        const std::uint8_t type = event_type(e.id);
        if (type < 4) {
            m_leist.set_graphic(id, kGfxLeiste + 1 + std::min<std::int32_t>(e.banner, 4) + type * 12, 6);
        } else {
            m_leist.set_graphic(id, kGfxLeiste + 1 + 0x28 + type * 2, 1);
        }
        m_leist.set_hidden(id, false);
        if (m_leist.hovered() == id && e.id < m_meldung.size()) {
            status = m_meldung[e.id];
        }
    }
    m_ctrl.set_graphic(kCompass, kGfxCompass + m_rotation, 0);
    m_renderer->draw(m_ctrl.draw(m_renderer->fonts()), m_regions.side_panel.x, m_regions.side_panel.y, s);
    // The mode's panel below the head, and its help texts.
    const BuildPanel::Available available =
        m_available ? m_available : BuildPanel::Available([](std::uint32_t) { return true; });
    if (m_mode == kModeBuild && m_build) {
        m_build->draw(*m_renderer, buildings, available,
                      static_cast<std::uint8_t>((m_rotation + m_building_rotation) & 3U), m_regions.side_panel, s);
        if (const auto text = m_build->help(m_leiste)) {
            status = *text;
        }
    } else if (m_mode == kModeInfo && m_info) {
        m_info->draw(*m_renderer, figures, colour, m_regions.side_panel, s);
        if (const auto text = m_info->help(m_leiste)) {
            status = *text;
        }
    }
    // The minimap on the parchment.
    if (m_minimap_dirty || now_ms >= m_minimap_ms + kMinimapMs || world.width() != m_world_w ||
        world.height() != m_world_h) {
        build_minimap(world, buildings, banners);
        m_minimap_ms = now_ms;
        m_minimap_dirty = true;
    }
    const ui::Rect map = minimap_rect();
    if (map.w > 0) {
        // The parchment under the map (GFXMAP, drawn by the code under the GAD_NIX area).
        ui::DrawItem parchment;
        parchment.kind = ui::DrawItem::Kind::Sprite;
        parchment.gfx = kGfxParchment;
        m_renderer->draw({parchment}, map.x, map.y, s);
    }
    if (map.w > 0 && m_world_w > 0) {
        const double fit = std::min(static_cast<double>(map.w) / m_world_w, static_cast<double>(map.h) / m_world_h);
        const ui::Rect at{map.x + static_cast<std::int32_t>((map.w - m_world_w * fit) / 2.0),
                          map.y + static_cast<std::int32_t>((map.h - m_world_h * fit) / 2.0),
                          static_cast<std::int32_t>(m_world_w * fit), static_cast<std::int32_t>(m_world_h * fit)};
        m_renderer->image(1, m_minimap, static_cast<std::int32_t>(m_world_w), static_cast<std::int32_t>(m_world_h), at,
                          m_minimap_dirty);
        m_minimap_dirty = false;
        // The visible part of the map as a white outline.
        std::array<SDL_FPoint, 5> points{};
        for (std::size_t i = 0; i < 4; ++i) {
            points[i] = SDL_FPoint{static_cast<float>(at.x + view[i].first * fit),
                                   static_cast<float>(at.y + view[i].second * fit)};
        }
        points[4] = points[0];
        const SDL_Rect clip{at.x, at.y, at.w, at.h};
        SDL_SetRenderClipRect(m_sdl, &clip);
        SDL_SetRenderDrawColor(m_sdl, 255, 255, 255, 255);
        SDL_RenderLines(m_sdl, points.data(), static_cast<int>(points.size()));
        SDL_SetRenderClipRect(m_sdl, nullptr);
    }
    // A status bar wider than its art (wide windows): the art's plain middle continues it.
    m_renderer->fill_with_middle(0, kGfxLeiste, m_regions.status_bar, s);
    m_leist.set_text(kStatusLine, status);
    m_renderer->draw(m_leist.draw(m_renderer->fonts()), m_regions.status_bar.x, m_regions.status_bar.y, s);
}

}  // namespace o1602::app
