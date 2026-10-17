// SPDX-License-Identifier: GPL-3.0-or-later
#include "map_viewer.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/sprite_sheet.hpp"
#include "o1602/assets/text_table.hpp"
#include "o1602/core/png_writer.hpp"
#include "o1602/render/world_renderer.hpp"
#include "o1602/sim/growth.hpp"

namespace o1602::app {

namespace {

/// Ground under a building that cannot be built there.
constexpr assets::Rgba kRefusedTint{220, 30, 30, 140};

constexpr double kScrollSpeed = 1200.0;  ///< screen pixels per second (keyboard)
constexpr std::int32_t kMaxPixelScale = 4;

const char* zoom_name(render::Zoom zoom) {
    switch (zoom) {
        case render::Zoom::Large:
            return "GFX";
        case render::Zoom::Medium:
            return "MGFX";
        default:
            return "SGFX";
    }
}

std::string player_label(const sim::Player& p, sim::PlayerId slot, const render::BannerTable& banners) {
    std::string label = "player " + std::to_string(slot);
    if (const render::Banner* b = banners.get(p.banner)) {
        const auto colon = b->id.find(':');
        label += " " + (colon == std::string::npos ? b->id : b->id.substr(colon + 1));
    }
    return label + " (" + sim::role_name(p.role) + ")";
}

}  // namespace

core::Result<std::unique_ptr<MapViewer>> MapViewer::create(const assets::GameDirectory& game,
                                                           const ViewerOptions& options) {
    auto buildings = assets::BuildingTable::load(game);
    if (!buildings) {
        return buildings.error();
    }
    auto file = assets::ChunkFile::load(options.scenario);
    if (!file) {
        return file.error();
    }
    std::vector<std::string> warnings;
    auto world = sim::load_world(*file, *buildings, game, &warnings);
    if (!world) {
        return world.error();
    }
    for (const auto& w : warnings) {
        std::fprintf(stderr, "warning: %s\n", w.c_str());
    }
    const auto banners = render::BannerTable::original();
    for (std::size_t i = 0; i < world->players().size(); ++i) {
        const auto& p = world->players()[i];
        std::fprintf(stderr, "%s, money %d\n", player_label(p, static_cast<sim::PlayerId>(i), banners).c_str(),
                     p.money);
    }
    std::fprintf(stderr, "%zu islands, %zu ships\n", world->islands().size(), world->ships().size());
    const auto palette_path = game.resolve(render::kTerrainPalettePath);
    if (!palette_path) {
        return core::make_error(core::ErrorCode::NotFound, "toolgfx/stadtfld.col not found");
    }
    auto palette = assets::Palette::load(*palette_path);
    if (!palette) {
        return palette.error();
    }
    auto figures = assets::FigureTable::load(game);
    if (!figures) {
        return figures.error();
    }

    std::unique_ptr<MapViewer> viewer(new MapViewer(game, std::move(buildings).value(), std::move(world).value(),
                                                    std::move(palette).value(), std::move(figures).value(), options));
    viewer->m_banners =
        render::BannerTable::original(&viewer->m_figures, render::BannerOptions{options.original_flags});
    viewer->m_building_figures =
        render::collect_building_figures(viewer->m_world, viewer->m_buildings, viewer->m_figures);
    viewer->m_local_player = sim::first_human_player(viewer->m_world);
    if (auto texts = assets::TextTable::load(game)) {
        viewer->m_simulation.set_city_names(texts->section("STAEDTE"));
    }
    viewer->m_soldiers.reset(viewer->m_world, viewer->m_figures);
    viewer->m_show_owners = options.show_owners;
    auto sdl = platform::SdlSystem::init();
    if (!sdl) {
        return sdl.error();
    }
    viewer->m_sdl = std::move(sdl).value();
    platform::WindowConfig config;
    config.title = "Open1602";
    config.width = options.width;
    config.height = options.height;
    auto window = platform::Window::create(config);
    if (!window) {
        return window.error();
    }
    viewer->m_window = std::move(window).value();
    std::fprintf(stderr, "renderer: %s, max texture %d\n", viewer->m_window->renderer_name().c_str(),
                 viewer->m_window->max_texture_size());

    std::int32_t w = 0;
    std::int32_t h = 0;
    viewer->m_window->output_size(w, h);
    const auto scale = static_cast<std::int32_t>(std::lround(viewer->m_window->pixel_density()));
    viewer->m_camera.set_world_size(viewer->m_world.width(), viewer->m_world.height());
    viewer->m_camera.set_screen(w, h, std::clamp(scale, 1, kMaxPixelScale));
    viewer->m_camera.set_zoom(options.zoom, w / 2.0, h / 2.0);
    // Start over the player's own city.
    viewer->m_camera.center_on(viewer->home_tile());
    if (auto status = viewer->ensure_graphics(viewer->m_camera.view().zoom); !status) {
        return status.error();
    }
    return viewer;
}

MapViewer::MapViewer(assets::GameDirectory game, assets::BuildingTable buildings, sim::World world,
                     assets::Palette palette, assets::FigureTable figures, ViewerOptions options)
    : m_game(std::move(game)),
      m_buildings(std::move(buildings)),
      m_world(std::move(world)),
      m_palette(std::move(palette)),
      m_figures(std::move(figures)),
      m_options(std::move(options)),
      m_simulation(m_world, m_buildings, m_figures, m_options.rules),
      m_clock(m_buildings),
      m_tool(m_buildings) {}

MapViewer::~MapViewer() {
    // GPU resources must go before the renderer/window, which go before SDL_Quit.
    for (auto& g : m_graphics) {
        g.reset();
    }
    m_window.reset();
    m_sdl.reset();
}

core::Status MapViewer::ensure_graphics(render::Zoom zoom) {
    auto& slot = m_graphics[static_cast<std::size_t>(zoom)];
    if (slot) {
        return core::Ok{};
    }
    const std::int32_t max_texture = m_window->max_texture_size();
    render::GraphicsOptions options;
    options.page_size = max_texture >= 4096 ? 4096U : 2048U;
    options.derive_missing_sheets = m_options.derive_small_figures;
    const std::uint64_t start = platform::ticks_ms();
    auto graphics = render::load_world_graphics(m_game, m_palette, zoom, render::kWorldFigureSheets, options,
                                                &m_banners, &m_figures);
    if (!graphics) {
        return graphics.error();
    }
    const std::uint32_t page_size = options.page_size;
    slot.emplace(ZoomGraphics{std::move(graphics).value(), nullptr});
    slot->renderer = std::make_unique<render::SdlSpriteRenderer>(m_window->sdl_renderer());
    if (auto status = slot->renderer->upload(slot->graphics.atlas); !status) {
        slot.reset();
        return status;
    }
    slot->graphics.atlas.drop_pixels();
    std::fprintf(stderr, "%s: %zu sprites in %zu page(s) of %u px, %llu ms\n", zoom_name(zoom),
                 slot->graphics.atlas.sprite_count(), slot->graphics.atlas.pages().size(), page_size,
                 static_cast<unsigned long long>(platform::ticks_ms() - start));
    return core::Ok{};
}

int MapViewer::run() {
    std::uint64_t last = platform::ticks_ms();
    const std::uint64_t run_start = last;
    std::uint64_t fps_start = last;
    std::uint32_t fps_frames = 0;
    std::uint32_t frames = 0;
    update_title(0.0);
    while (m_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handle_event(event);
        }
        const std::uint64_t now = platform::ticks_ms();
        const auto delta = static_cast<std::uint32_t>(std::min<std::uint64_t>(now - last, 1000));
        last = now;
        update(delta);
        render();

        ++frames;
        ++fps_frames;
        if (now - fps_start >= 500) {
            update_title(fps_frames * 1000.0 / static_cast<double>(now - fps_start));
            fps_start = now;
            fps_frames = 0;
        }
        if (m_options.max_frames != 0 && frames >= m_options.max_frames) {
            break;
        }
    }
    if (m_options.max_frames != 0) {
        const std::uint64_t elapsed = std::max<std::uint64_t>(platform::ticks_ms() - run_start, 1);
        std::fprintf(stderr, "%u frames in %llu ms (%.1f fps), %zu sprites in the last frame\n", frames,
                     static_cast<unsigned long long>(elapsed), frames * 1000.0 / static_cast<double>(elapsed),
                     m_sprites_drawn);
    }
    if (!m_options.screenshot.empty()) {
        render();  // read back a complete frame
        if (auto status = save_screenshot(m_options.screenshot); !status) {
            std::fprintf(stderr, "error: %s\n", status.error().message.c_str());
            return 1;
        }
    }
    return 0;
}

void MapViewer::handle_event(const SDL_Event& original) {
    SDL_Event event = original;
    // Mouse positions in output pixels, independent of the screen's pixel density.
    SDL_ConvertEventToRenderCoordinates(m_window->sdl_renderer(), &event);
    switch (event.type) {
        case SDL_EVENT_QUIT:
            m_running = false;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            std::int32_t w = 0;
            std::int32_t h = 0;
            m_window->output_size(w, h);
            m_camera.set_screen(w, h, m_camera.pixel_scale());
            break;
        }
        case SDL_EVENT_KEY_DOWN:
            handle_key(static_cast<int>(event.key.key), (event.key.mod & SDL_KMOD_SHIFT) != 0);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            m_mouse_x = event.motion.x;
            m_mouse_y = event.motion.y;
            if (m_dragging) {
                m_camera.scroll(-event.motion.xrel, -event.motion.yrel);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE) {
                m_dragging = true;
            } else if (event.button.button == SDL_BUTTON_LEFT && m_tool.active()) {
                if (const auto spot = spot_at_mouse()) {
                    m_tool.press(*spot);
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE) {
                m_dragging = false;
            } else if (event.button.button == SDL_BUTTON_LEFT && m_tool.dragging()) {
                if (const auto spot = spot_at_mouse()) {
                    if (auto order = m_tool.build_order(m_world, m_local_player, *spot)) {
                        m_simulation.submit(std::move(*order));
                    } else if (auto demolish = m_tool.demolish_order(m_local_player, *spot)) {
                        m_simulation.submit(*demolish);
                    }
                }
                m_tool.release();
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL: {  // U-001: the original has no wheel zoom
            const float y = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
            if (y != 0.0F) {
                zoom_by(y > 0.0F ? -1 : 1, event.wheel.mouse_x, event.wheel.mouse_y);
            }
            break;
        }
        default:
            break;
    }
}

void MapViewer::zoom_by(int steps, double sx, double sy) {
    const int current = static_cast<int>(m_camera.view().zoom);
    const int next = std::clamp(current + steps, 0, 2);
    if (next == current) {
        return;
    }
    const auto zoom = static_cast<render::Zoom>(next);
    if (auto status = ensure_graphics(zoom); !status) {
        std::fprintf(stderr, "error: %s\n", status.error().message.c_str());
        return;
    }
    m_camera.set_zoom(zoom, sx, sy);
}

void MapViewer::handle_key(int key, bool shift) {
    std::int32_t w = 0;
    std::int32_t h = 0;
    m_window->output_size(w, h);
    const double cx = w / 2.0;
    const double cy = h / 2.0;
    switch (key) {
        case SDLK_ESCAPE:
            if (m_tool.active()) {
                m_tool.set_mode(BuildTool::Mode::Off);
            } else {
                m_running = false;
            }
            break;
        case SDLK_Q:
            m_running = false;
            break;
        case SDLK_B:
            m_tool.set_mode(m_tool.mode() == BuildTool::Mode::Build ? BuildTool::Mode::Off : BuildTool::Mode::Build);
            break;
        case SDLK_X:
            m_tool.set_mode(m_tool.mode() == BuildTool::Mode::Demolish ? BuildTool::Mode::Off
                                                                       : BuildTool::Mode::Demolish);
            break;
        case SDLK_TAB:
            m_tool.cycle(shift ? -1 : 1);
            break;
        case SDLK_COMMA:
            m_tool.rotate(-1);
            break;
        case SDLK_PERIOD:
            m_tool.rotate(1);
            break;
        case SDLK_1:
        case SDLK_2:
        case SDLK_3:
            zoom_by(key - static_cast<int>(SDLK_1) - static_cast<int>(m_camera.view().zoom), cx, cy);
            break;
        case SDLK_R:
            m_camera.set_rotation(static_cast<std::uint8_t>(m_camera.view().rotation + (shift ? 3 : 1)), cx, cy);
            break;
        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            m_camera.set_pixel_scale(std::min(m_camera.pixel_scale() + 1, kMaxPixelScale), cx, cy);
            break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            m_camera.set_pixel_scale(std::max(m_camera.pixel_scale() - 1, 1), cx, cy);
            break;
        case SDLK_O:
            m_show_owners = !m_show_owners;
            break;
        case SDLK_SPACE:
            m_paused = !m_paused;
            break;
        case SDLK_PAGEUP:  // game speed
            m_simulation.set_speed(m_simulation.speed() * 2);
            break;
        case SDLK_PAGEDOWN:
            m_simulation.set_speed(std::max(1U, m_simulation.speed() / 2));
            break;
        case SDLK_HOME:
            m_camera.center_on(home_tile());
            break;
        case SDLK_F12: {  // U-002: the original's screenshot key does not work
            const auto status = save_screenshot("open1602-screenshot.png");
            std::fprintf(stderr, "%s\n",
                         status ? "screenshot saved: open1602-screenshot.png" : status.error().message.c_str());
            break;
        }
        default:
            break;
    }
}

void MapViewer::update(std::uint32_t delta_ms) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    double dx = 0.0;
    double dy = 0.0;
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
        dx -= 1.0;
    }
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
        dx += 1.0;
    }
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
        dy -= 1.0;
    }
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) {
        dy += 1.0;
    }
    if (dx != 0.0 || dy != 0.0) {
        const bool fast = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
        const double distance = kScrollSpeed * (fast ? 3.0 : 1.0) * delta_ms / 1000.0;
        m_camera.scroll(dx * distance, dy * distance);
    }
    if (m_paused) {
        m_simulation.apply_commands();  // building is possible while paused
    } else {
        m_simulation.advance(delta_ms);
        apply_production_events();
        m_clock.advance(delta_ms);
        m_soldiers.advance(static_cast<std::uint32_t>(delta_ms), m_figures);
        m_walkers.update(m_simulation.logistics(), m_world, m_figures,
                         static_cast<std::uint32_t>(delta_ms) * m_simulation.speed());
    }
    apply_construction();
}

std::optional<BuildTool::Spot> MapViewer::spot_at_mouse() const {
    const auto tile = m_camera.tile_at_screen(m_mouse_x, m_mouse_y);
    if (!tile) {
        return std::nullopt;
    }
    const auto& cell = m_world.cell(tile->x, tile->y);
    if (cell.island == sim::World::kSea) {
        return std::nullopt;
    }
    return BuildTool::Spot{cell.island, cell.local_x, cell.local_y};
}

void MapViewer::apply_construction() {
    for (const sim::CommandResult& r : m_simulation.take_command_results()) {
        if (!r.build) {
            if (r.demolished != 0) {
                std::fprintf(stderr, "demolished %zu, back: %lld gold, %u t tools, %u t wood, %u t bricks\n",
                             r.demolished, static_cast<long long>(r.refund.money), r.refund.tools / 32,
                             r.refund.wood / 32, r.refund.bricks / 32);
            }
            continue;
        }
        std::fprintf(stderr, "built %zu of %zu", r.build->placed, r.build->items.size());
        for (const sim::PlannedBuild& item : r.build->items) {
            if (!item.ok()) {
                std::fprintf(stderr, " - %s", sim::refusal_name(item.refusal));
                break;
            }
        }
        std::fprintf(stderr, "\n");
    }
    bool changed = false;
    for (const sim::ConstructionEvent& e : m_simulation.take_construction_events()) {
        changed = true;
        if (e.kind == sim::ConstructionEvent::Kind::CityFounded) {
            if (const sim::City* c = m_world.city(e.island, e.city)) {
                std::fprintf(stderr, "a new city was founded: %s\n", c->name.c_str());
            }
        }
    }
    if (changed) {
        // Flags, smoke and status icons of the new buildings.
        m_building_figures = render::collect_building_figures(m_world, m_buildings, m_figures);
    }
}

void MapViewer::render() {
    SDL_Renderer* r = m_window->sdl_renderer();
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    auto& graphics = m_graphics[static_cast<std::size_t>(m_camera.view().zoom)];
    if (graphics) {
        render::WorldLayers layers;
        const render::SpriteAtlas& atlas = graphics->graphics.atlas;
        layers.extent = render::SpriteExtent{atlas.max_sprite_width(), atlas.max_sprite_height()};
        layers.figures = &m_figures;
        layers.atlas = &atlas;
        layers.ship_base = graphics->graphics.sheet_base(assets::FigureSheet::Ship).value_or(0);
        layers.sheet_base = graphics->graphics.base;
        // The deer walk as figures of the simulation (m_walkers).
        layers.land_figures = {&m_soldiers.instances(), &m_walkers.instances()};
        layers.unit_tint = &graphics->graphics.unit_tint;
        layers.banners = &m_banners;
        layers.banner_flags = &graphics->graphics.banner_flags;
        layers.owner_tint = m_show_owners;
        layers.building_figures = &m_building_figures;
        layers.local_player = m_local_player;
        // The building the tool would place, drawn into the map for this frame only.
        struct Saved {
            std::uint16_t island;
            std::uint32_t x;
            std::uint32_t y;
            sim::MapCell cell;
        };
        std::vector<Saved> saved;
        m_tool_status.clear();
        m_tile_marks.clear();
        layers.tile_marks = &m_tile_marks;
        if (m_tool.mode() == BuildTool::Mode::Build) {
            if (const auto spot = spot_at_mouse()) {
                if (const auto order = m_tool.build_order(m_world, m_local_player, *spot)) {
                    const sim::BuildPlan plan = sim::plan_build(m_world, m_buildings, *order);
                    sim::IslandMap& map = m_world.island(order->island).map;
                    for (const sim::PlannedBuild& item : plan.items) {
                        if (!item.ok() && m_tool_status.empty()) {
                            m_tool_status = sim::refusal_name(item.refusal);
                        }
                        if (item.type == nullptr) {
                            continue;
                        }
                        // What cannot be built is shown too, on red ground, as in the original.
                        std::uint32_t w = 1;
                        std::uint32_t h = 1;
                        sim::rotated_size(*item.type, item.rotation, w, h);
                        bool inside = true;
                        for (std::uint32_t j = 0; j < h; ++j) {
                            for (std::uint32_t i = 0; i < w; ++i) {
                                const std::int32_t tx = item.x + static_cast<std::int32_t>(i);
                                const std::int32_t ty = item.y + static_cast<std::int32_t>(j);
                                if (!map.contains(tx, ty)) {
                                    inside = false;
                                    continue;
                                }
                                if (!item.ok()) {
                                    m_tile_marks.push_back(
                                        render::TileMark{order->island, static_cast<std::uint16_t>(tx),
                                                         static_cast<std::uint16_t>(ty), kRefusedTint});
                                }
                            }
                        }
                        // What only lacks gold or goods is shown on its red ground; anything else that
                        // cannot be built only as red ground (it might stand over a building or the sea).
                        const bool short_of_means =
                            item.refusal == sim::BuildRefusal::Money || item.refusal == sim::BuildRefusal::Tools ||
                            item.refusal == sim::BuildRefusal::Wood || item.refusal == sim::BuildRefusal::Bricks ||
                            item.refusal == sim::BuildRefusal::Cannons;
                        if (!inside || (!item.ok() && !short_of_means)) {
                            continue;
                        }
                        for (std::uint32_t j = 0; j < h; ++j) {
                            for (std::uint32_t i = 0; i < w; ++i) {
                                const auto tx = static_cast<std::uint32_t>(item.x) + i;
                                const auto ty = static_cast<std::uint32_t>(item.y) + j;
                                saved.push_back(Saved{order->island, tx, ty, map.top(tx, ty)});
                            }
                        }
                        const sim::MapCell& at =
                            map.top(static_cast<std::uint32_t>(item.x), static_cast<std::uint32_t>(item.y));
                        // A field or tree is shown grown, a seedling would hardly be seen.
                        const assets::BuildingType* shown = item.type;
                        if (const assets::BuildingType* ripe = sim::ripe_record(m_buildings, *item.type)) {
                            shown = ripe;
                        }
                        (void)map.build(*shown, item.x, item.y, item.rotation, 0, at.owner, at.city);
                    }
                    if (m_tool_status.empty()) {
                        m_tool_status = std::to_string(plan.bill.money) + " gold";
                    }
                }
            }
        }
        render::build_world_draw_list(m_world, m_buildings, m_clock, m_camera.view(), m_camera.viewport(), layers,
                                      m_commands);
        for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
            sim::IslandMap& map = m_world.island(it->island).map;
            map.set_sprite(it->x, it->y, it->cell.sprite);
            map.set_city(it->x, it->y, it->cell.city, it->cell.owner);
            map.set_anim_phase(it->x, it->y, it->cell.anim_phase);
            map.set_rotation(it->x, it->y, it->cell.rotation);
        }
        std::int32_t ox = 0;
        std::int32_t oy = 0;
        m_camera.screen_origin(ox, oy);
        m_sprites_drawn = graphics->renderer->draw(m_commands, ox, oy, m_camera.pixel_scale());
    }
    SDL_RenderPresent(r);
}

void MapViewer::apply_production_events() {
    for (const sim::ProductionEvent& e : m_simulation.take_production_events()) {
        if (e.kind != sim::ProductionEvent::Kind::Started && e.kind != sim::ProductionEvent::Kind::Stopped) {
            continue;
        }
        sim::Production& p = m_world.productions()[e.production];
        const sim::MapCell& cell = m_world.islands()[p.island].map.top(p.x, p.y);
        if (const assets::BuildingType* b = m_buildings.by_sprite(cell.sprite)) {
            p.anim_phase = render::phase_after_change(*b, p.anim_phase, m_clock.frame(*b),
                                                      e.kind == sim::ProductionEvent::Kind::Started);
        }
    }
    // Messages for the local player until there is an interface for them.
    for (const sim::CityEvent& e : m_simulation.take_city_events()) {
        if (e.owner != m_local_player) {
            continue;
        }
        const sim::City* city = m_world.city(e.island, e.city);
        const std::string name = city != nullptr ? city->name : std::string("?");
        if (e.kind == sim::CityEvent::Kind::Hunger) {
            std::fprintf(stderr, "%s: the people are starving\n", name.c_str());
        } else if (e.kind == sim::CityEvent::Kind::Unlocked) {
            std::fprintf(stderr, "%s: build level %u reached\n", name.c_str(), e.infra);
        } else {
            std::fprintf(stderr, "%s: short of ware %u\n", name.c_str(), e.ware);
        }
    }
    // Fires and plagues in the player's cities (MELDUNG 1, 2).
    for (const sim::DisasterEvent& e : m_simulation.take_disaster_events()) {
        if (e.owner != m_local_player) {
            continue;
        }
        const sim::City* city = m_world.city(e.island, e.city);
        const char* name = city != nullptr ? city->name.c_str() : "?";
        const bool fire = e.disaster == sim::Disaster::Kind::Fire;
        if (e.kind == sim::DisasterEvent::Kind::Started && !e.spread) {
            std::fprintf(stderr, "%s: %s (%u,%u)\n", name,
                         fire ? "a fire broke out!" : "the plague haunts your settlement!", e.x, e.y);
        } else if (e.kind == sim::DisasterEvent::Kind::BurnedDown) {
            std::fprintf(stderr, "%s: a building burned down (%u,%u)\n", name, e.x, e.y);
        }
    }
    // Droughts: the player hears of those on islands where he has a city (MELDUNG 3).
    for (const sim::IslandEvent& e : m_simulation.take_island_events()) {
        const sim::City* own = nullptr;
        for (const sim::City& c : m_world.cities()) {
            if (c.island == e.island && c.owner == m_local_player) {
                own = &c;
            }
        }
        if (own == nullptr) {
            continue;
        }
        switch (e.kind) {
            case sim::IslandEvent::Kind::DroughtStarted:
                std::fprintf(stderr, "%s: the drought destroys your harvest!\n", own->name.c_str());
                break;
            case sim::IslandEvent::Kind::Withered:
                std::fprintf(stderr, "%s: %u fields withered\n", own->name.c_str(), e.count);
                break;
            case sim::IslandEvent::Kind::Recovered:
                std::fprintf(stderr, "%s: %u fields grow again\n", own->name.c_str(), e.count);
                break;
            case sim::IslandEvent::Kind::DroughtOver:
                std::fprintf(stderr, "%s: the drought is over\n", own->name.c_str());
                break;
        }
    }
}

render::TilePos MapViewer::home_tile() const {
    for (const sim::City& c : m_world.cities()) {
        if (c.owner == m_local_player && c.warehouse_x && c.warehouse_y && c.island < m_world.islands().size()) {
            const sim::WorldIsland& island = m_world.islands()[c.island];
            return render::TilePos{island.x + *c.warehouse_x, island.y + *c.warehouse_y};
        }
    }
    for (const sim::Ship& s : m_world.ships()) {
        if (s.owner == m_local_player) {
            return render::TilePos{static_cast<std::uint32_t>(s.x), static_cast<std::uint32_t>(s.y)};
        }
    }
    if (!m_world.islands().empty()) {
        const auto& island = m_world.islands().front();
        return render::TilePos{island.x + island.map.width() / 2, island.y + island.map.height() / 2};
    }
    return render::TilePos{m_world.width() / 2, m_world.height() / 2};
}

void MapViewer::update_title(double fps) {
    const auto& view = m_camera.view();
    std::string title = "Open1602 - " + m_options.scenario.stem().string() + " | " + zoom_name(view.zoom) + " x" +
                        std::to_string(m_camera.pixel_scale()) + " | rotation " + std::to_string(view.rotation);
    if (const auto tile = m_camera.tile_at_screen(m_mouse_x, m_mouse_y)) {
        title += " | tile " + std::to_string(tile->x) + "," + std::to_string(tile->y);
        const auto& cell = m_world.cell(tile->x, tile->y);
        if (cell.island != sim::World::kSea) {
            title += " (island " + std::to_string(cell.island);
            const auto& island = m_world.islands()[cell.island];
            const sim::MapCell& top = island.map.top(cell.local_x, cell.local_y);
            if (const sim::City* city = m_world.city(cell.island, top.city)) {
                title += ", " + city->name;  // Windows-1252; the UI will convert it
            }
            if (const sim::Player* p = m_world.player(top.owner)) {
                title += ", " + player_label(*p, top.owner, m_banners);
            }
            title += ")";
        }
    }
    if (m_tool.active()) {
        title += " | " + m_tool.describe();
        if (!m_tool_status.empty()) {
            title += ": " + m_tool_status;
        }
    }
    char perf[64];
    std::snprintf(perf, sizeof perf, " | %.0f fps, %zu sprites", fps, m_sprites_drawn);
    title += perf;
    if (m_paused) {
        title += " | paused";
    } else if (m_simulation.speed() != 1) {
        title += " | speed x" + std::to_string(m_simulation.speed());
    }
    const std::uint64_t seconds = m_simulation.game_time_ms() / 1000;
    char clock[32];
    std::snprintf(clock, sizeof clock, " | %02llu:%02llu:%02llu", static_cast<unsigned long long>(seconds / 3600),
                  static_cast<unsigned long long>(seconds / 60 % 60), static_cast<unsigned long long>(seconds % 60));
    title += clock;
    m_window->set_title(title);
}

core::Status MapViewer::save_screenshot(const std::filesystem::path& path) {
    SDL_Surface* surface = SDL_RenderReadPixels(m_window->sdl_renderer(), nullptr);
    if (surface == nullptr) {
        return platform::sdl_error("SDL_RenderReadPixels");
    }
    SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (rgba == nullptr) {
        return platform::sdl_error("SDL_ConvertSurface");
    }
    const auto width = static_cast<std::uint32_t>(rgba->w);
    const auto height = static_cast<std::uint32_t>(rgba->h);
    std::vector<std::uint8_t> pixels(std::size_t{width} * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto* row =
            static_cast<const std::uint8_t*>(rgba->pixels) + std::size_t{y} * static_cast<std::size_t>(rgba->pitch);
        std::copy_n(row, std::size_t{width} * 4,
                    pixels.begin() + static_cast<std::ptrdiff_t>(std::size_t{y} * width * 4));
    }
    SDL_DestroySurface(rgba);
    for (std::size_t i = 3; i < pixels.size(); i += 4) {
        pixels[i] = 255;
    }
    return core::write_png_rgba(path, width, height, pixels);
}

}  // namespace o1602::app
