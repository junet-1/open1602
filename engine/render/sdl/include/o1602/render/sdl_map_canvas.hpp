// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where the map is drawn at a scale that is no whole number (render::RenderPlan): a texture the
// sprites are drawn into at the next whole scale, then shrunk onto the screen with linear
// filtering. Each sprite pixel keeps hard edges; only the screen pixels on its border mix the two
// colours, which keeps the pixel art sharp without uneven pixels while zooming. At a whole scale
// the sprites go straight to the screen and the canvas costs nothing.
#pragma once

#include <cstdint>

#include "o1602/render/camera.hpp"

struct SDL_Renderer;
struct SDL_Texture;

namespace o1602::render {

class SdlMapCanvas {
public:
    /// Its texture is `renderer`'s: it goes before the renderer does (or release() is called).
    explicit SdlMapCanvas(SDL_Renderer* renderer) : m_renderer(renderer) {}
    ~SdlMapCanvas();
    SdlMapCanvas(const SdlMapCanvas&) = delete;
    SdlMapCanvas& operator=(const SdlMapCanvas&) = delete;

    /// Starts a frame of the plan: the renderer draws into the canvas from now on (cleared to
    /// black), or onto the screen if the plan is direct. False if the canvas could not be made;
    /// the caller then draws straight onto the screen at the plan's scale.
    bool begin(const RenderPlan& plan);
    /// Back to the screen and the canvas stretched onto it (nothing if begin() drew directly).
    void end();

    /// The texture goes (the render device was reset or lost; made again when needed).
    void release();
    /// The renderer was made again: the canvas draws with the new one (release() went before).
    void set_renderer(SDL_Renderer* renderer) noexcept { m_renderer = renderer; }

    /// Whether the last frame went through the canvas (for the developer readout).
    [[nodiscard]] bool used() const noexcept { return m_used; }

private:
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture = nullptr;
    std::int32_t m_width = 0;  ///< of the texture
    std::int32_t m_height = 0;
    RenderPlan m_plan;
    bool m_active = false;  ///< between begin() and end(), drawing into the texture
    bool m_used = false;
};

}  // namespace o1602::render
