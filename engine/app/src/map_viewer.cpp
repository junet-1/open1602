// SPDX-License-Identifier: GPL-3.0-or-later
#include "map_viewer.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "o1602/assets/chunk_file.hpp"
#include "o1602/assets/sprite_sheet.hpp"
#include "o1602/core/png_writer.hpp"
#include "o1602/render/world_renderer.hpp"

namespace o1602::app {

namespace {

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
    const auto palette_path = game.resolve(render::kTerrainPalettePath);
    if (!palette_path) {
        return core::make_error(core::ErrorCode::NotFound, "toolgfx/stadtfld.col not found");
    }
    auto palette = assets::Palette::load(*palette_path);
    if (!palette) {
        return palette.error();
    }

    std::unique_ptr<MapViewer> viewer(new MapViewer(game, std::move(buildings).value(), std::move(world).value(),
                                                    std::move(palette).value(), options));
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
    viewer->m_camera.set_screen(w, h, std::clamp(scale, 1, kMaxPixelScale));
    viewer->m_camera.set_zoom(options.zoom, w / 2.0, h / 2.0);
    // Start over the first island (or the map centre).
    if (!viewer->m_world.islands().empty()) {
        const auto& island = viewer->m_world.islands().front();
        viewer->m_camera.center_on(
            render::TilePos{island.x + island.map.width() / 2, island.y + island.map.height() / 2});
    } else {
        viewer->m_camera.center_on(render::TilePos{sim::World::kWidth / 2, sim::World::kHeight / 2});
    }
    if (auto status = viewer->ensure_graphics(viewer->m_camera.view().zoom); !status) {
        return status.error();
    }
    return viewer;
}

MapViewer::MapViewer(assets::GameDirectory game, assets::BuildingTable buildings, sim::World world,
                     assets::Palette palette, ViewerOptions options)
    : m_game(std::move(game)),
      m_buildings(std::move(buildings)),
      m_world(std::move(world)),
      m_palette(std::move(palette)),
      m_options(std::move(options)),
      m_clock(m_buildings) {}

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
    const auto path = m_game.resolve(render::terrain_sheet_path(zoom));
    if (!path) {
        return core::make_error(core::ErrorCode::NotFound,
                                std::string(render::terrain_sheet_path(zoom)) + " not found");
    }
    auto sheet = assets::SpriteSheet::load(*path);
    if (!sheet) {
        return sheet.error();
    }
    const std::int32_t max_texture = m_window->max_texture_size();
    const std::uint32_t page_size = max_texture >= 4096 ? 4096U : 2048U;
    const std::uint64_t start = platform::ticks_ms();
    auto atlas = render::SpriteAtlas::build(*sheet, m_palette, page_size);
    if (!atlas) {
        return atlas.error();
    }
    slot.emplace(ZoomGraphics{std::move(atlas).value(), nullptr});
    slot->renderer = std::make_unique<render::SdlSpriteRenderer>(m_window->sdl_renderer());
    if (auto status = slot->renderer->upload(slot->atlas); !status) {
        slot.reset();
        return status;
    }
    slot->atlas.drop_pixels();
    std::fprintf(stderr, "%s: %zu sprites in %zu page(s) of %u px, %llu ms\n", zoom_name(zoom),
                 slot->atlas.sprite_count(), slot->atlas.pages().size(), page_size,
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
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE) {
                m_dragging = false;
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
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
        case SDLK_Q:
            m_running = false;
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
        case SDLK_SPACE:
            m_paused = !m_paused;
            break;
        case SDLK_HOME:
            if (!m_world.islands().empty()) {
                const auto& island = m_world.islands().front();
                m_camera.center_on(
                    render::TilePos{island.x + island.map.width() / 2, island.y + island.map.height() / 2});
            }
            break;
        case SDLK_F12: {
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
    if (!m_paused) {
        m_clock.advance(delta_ms);
    }
}

void MapViewer::render() {
    SDL_Renderer* r = m_window->sdl_renderer();
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    auto& graphics = m_graphics[static_cast<std::size_t>(m_camera.view().zoom)];
    if (graphics) {
        const render::SpriteExtent extent{graphics->atlas.max_sprite_width(), graphics->atlas.max_sprite_height()};
        render::build_world_draw_list(m_world, m_buildings, m_clock, m_camera.view(), m_camera.viewport(), extent,
                                      m_commands);
        std::int32_t ox = 0;
        std::int32_t oy = 0;
        m_camera.screen_origin(ox, oy);
        m_sprites_drawn = graphics->renderer->draw(m_commands, ox, oy, m_camera.pixel_scale());
    }
    SDL_RenderPresent(r);
}

void MapViewer::update_title(double fps) {
    const auto& view = m_camera.view();
    std::string title = "Open1602 - " + m_options.scenario.stem().string() + " | " + zoom_name(view.zoom) + " x" +
                        std::to_string(m_camera.pixel_scale()) + " | rotation " + std::to_string(view.rotation);
    if (const auto tile = m_camera.tile_at_screen(m_mouse_x, m_mouse_y)) {
        title += " | tile " + std::to_string(tile->x) + "," + std::to_string(tile->y);
        const auto& cell = m_world.cell(tile->x, tile->y);
        if (cell.island != sim::World::kSea) {
            title += " (island " + std::to_string(cell.island) + ")";
        }
    }
    char perf[64];
    std::snprintf(perf, sizeof perf, " | %.0f fps, %zu sprites", fps, m_sprites_drawn);
    title += perf;
    if (m_paused) {
        title += " | paused";
    }
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
