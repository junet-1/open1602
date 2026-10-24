// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/sdl_map_canvas.hpp"

#include <SDL3/SDL.h>
#include <algorithm>

namespace o1602::render {

SdlMapCanvas::~SdlMapCanvas() {
    release();
}

void SdlMapCanvas::release() {
    if (m_texture != nullptr) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    m_width = 0;
    m_height = 0;
    m_active = false;
}

bool SdlMapCanvas::begin(const RenderPlan& plan) {
    m_plan = plan;
    m_active = false;
    m_used = false;
    if (plan.direct) {
        return true;
    }
    // Grown by an eighth at a time, so that resizing the window or zooming does not make a new
    // texture every frame; made again when it is more than twice as wide and high as needed (a
    // window back from 4K, FB-35).
    if (m_texture == nullptr || plan.width > m_width || plan.height > m_height ||
        (plan.width * 2 < m_width && plan.height * 2 < m_height)) {
        release();
        const std::int32_t w = plan.width + plan.width / 8;
        const std::int32_t h = plan.height + plan.height / 8;
        m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
        if (m_texture == nullptr) {
            return false;
        }
        SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_NONE);
        m_width = w;
        m_height = h;
    }
    if (!SDL_SetRenderTarget(m_renderer, m_texture)) {
        return false;
    }
    const SDL_Rect area{0, 0, plan.width, plan.height};
    SDL_SetRenderViewport(m_renderer, &area);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    m_active = true;
    m_used = true;
    return true;
}

void SdlMapCanvas::end() {
    if (!m_active) {
        return;
    }
    SDL_SetRenderViewport(m_renderer, nullptr);
    SDL_SetRenderTarget(m_renderer, nullptr);
    const SDL_FRect src{0.0F, 0.0F, static_cast<float>(m_plan.width), static_cast<float>(m_plan.height)};
    const auto f = static_cast<float>(m_plan.dst_scale);
    const SDL_FRect dst{static_cast<float>(m_plan.dst_x), static_cast<float>(m_plan.dst_y),
                        static_cast<float>(m_plan.width) * f, static_cast<float>(m_plan.height) * f};
    SDL_RenderTexture(m_renderer, m_texture, &src, &dst);
    m_active = false;
}

}  // namespace o1602::render
