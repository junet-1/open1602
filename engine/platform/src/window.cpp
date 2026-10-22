// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/platform/window.hpp"

#include <SDL3/SDL.h>
#include <cstdio>

namespace o1602::platform {

core::Error sdl_error(const std::string& what) {
    return core::make_error(core::ErrorCode::PlatformError, what + ": " + SDL_GetError());
}

core::Result<std::unique_ptr<SdlSystem>> SdlSystem::init() {
    SDL_SetAppMetadata("Open1602", "0.1.0", "org.open1602.Open1602");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return sdl_error("SDL_Init");
    }
    return std::unique_ptr<SdlSystem>(new SdlSystem());
}

SdlSystem::~SdlSystem() {
    SDL_Quit();
}

core::Result<std::unique_ptr<Window>> Window::create(const WindowConfig& config) {
    std::unique_ptr<Window> window(new Window());
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;  // no mode set: the desktop's resolution
    }
    window->m_window = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
    if (window->m_window == nullptr) {
        return sdl_error("SDL_CreateWindow");
    }
    window->m_vsync = config.vsync;
    if (auto status = window->make_renderer(); !status) {
        return status.error();
    }
    return window;
}

core::Status Window::make_renderer() {
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (m_renderer == nullptr) {
        return sdl_error("SDL_CreateRenderer");
    }
    if (m_vsync) {
        SDL_SetRenderVSync(m_renderer, 1);  // not fatal if unsupported
    }
    return core::Ok{};
}

core::Status Window::recreate_renderer() {
    if (m_renderer != nullptr) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    return make_renderer();
}

RenderReset Window::render_reset(const SDL_Event& event) const {
    if (event.type != SDL_EVENT_RENDER_DEVICE_RESET && event.type != SDL_EVENT_RENDER_DEVICE_LOST) {
        return RenderReset::None;
    }
    if (event.render.windowID != SDL_GetWindowID(m_window)) {
        return RenderReset::None;
    }
    return event.type == SDL_EVENT_RENDER_DEVICE_LOST ? RenderReset::Renderer : RenderReset::Textures;
}

Window::~Window() {
    if (m_renderer != nullptr) {
        SDL_DestroyRenderer(m_renderer);
    }
    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
    }
}

void Window::output_size(std::int32_t& width, std::int32_t& height) const {
    int w = 0;
    int h = 0;
    SDL_GetRenderOutputSize(m_renderer, &w, &h);
    width = w;
    height = h;
}

float Window::pixel_density() const {
    const float density = SDL_GetWindowPixelDensity(m_window);
    return density > 0.0F ? density : 1.0F;
}

float Window::display_scale() const {
    const float scale = SDL_GetWindowDisplayScale(m_window);
    return scale > 0.0F ? scale : pixel_density();
}

void Window::set_title(const std::string& title) {
    SDL_SetWindowTitle(m_window, title.c_str());
}

std::string Window::renderer_name() const {
    const char* name = SDL_GetRendererName(m_renderer);
    return name != nullptr ? name : "unknown";
}

std::int32_t Window::max_texture_size() const {
    const SDL_PropertiesID props = SDL_GetRendererProperties(m_renderer);
    return static_cast<std::int32_t>(SDL_GetNumberProperty(props, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0));
}

std::uint64_t ticks_ms() {
    return SDL_GetTicks();
}

void Window::set_fullscreen(bool on) {
    if (on == fullscreen()) {
        return;
    }
    SDL_SetWindowFullscreenMode(m_window, nullptr);  // borderless at the desktop's resolution
    if (!SDL_SetWindowFullscreen(m_window, on)) {
        std::fprintf(stderr, "full screen: %s\n", SDL_GetError());
    }
}

bool Window::fullscreen() const {
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Window::window_size(std::int32_t& width, std::int32_t& height) const {
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(m_window, &w, &h);
    width = w;
    height = h;
}

void Window::set_window_size(std::int32_t width, std::int32_t height) {
    if (fullscreen() || maximized()) {
        SDL_RestoreWindow(m_window);
    }
    SDL_SetWindowSize(m_window, width, height);
    // Kept on the screen: centred if it would reach beyond the usable area.
    std::int32_t uw = 0;
    std::int32_t uh = 0;
    usable_area(uw, uh);
    if (uw > 0 && uh > 0 && (width >= uw || height >= uh)) {
        const SDL_DisplayID display = SDL_GetDisplayForWindow(m_window);
        const auto centred = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(display));
        SDL_SetWindowPosition(m_window, centred, centred);
    }
}

void Window::usable_area(std::int32_t& width, std::int32_t& height) const {
    width = 0;
    height = 0;
    SDL_Rect r{};
    if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(m_window), &r)) {
        width = r.w;
        height = r.h;
    }
}

bool Window::maximized() const {
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0;
}

void Window::set_vsync(bool on) {
    m_vsync = on;
    if (m_renderer != nullptr) {
        SDL_SetRenderVSync(m_renderer, on ? 1 : 0);
    }
}

}  // namespace o1602::platform
