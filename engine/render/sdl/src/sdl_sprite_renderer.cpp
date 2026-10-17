// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/sdl_sprite_renderer.hpp"

#include <SDL3/SDL.h>

namespace o1602::render {

SdlSpriteRenderer::SdlSpriteRenderer(SDL_Renderer* renderer) : m_renderer(renderer) {}

SdlSpriteRenderer::~SdlSpriteRenderer() {
    release();
}

void SdlSpriteRenderer::release() {
    for (SDL_Texture* texture : m_pages) {
        SDL_DestroyTexture(texture);
    }
    m_pages.clear();
    m_atlas = nullptr;
}

core::Status SdlSpriteRenderer::upload(const SpriteAtlas& atlas) {
    release();
    for (const RgbaImage& page : atlas.pages()) {
        SDL_Texture* texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                                 static_cast<int>(page.width), static_cast<int>(page.height));
        if (texture == nullptr) {
            release();
            return core::make_error(core::ErrorCode::PlatformError,
                                    std::string("SDL_CreateTexture: ") + SDL_GetError());
        }
        m_pages.push_back(texture);
        if (!SDL_UpdateTexture(texture, nullptr, page.pixels.data(), static_cast<int>(page.width * 4))) {
            release();
            return core::make_error(core::ErrorCode::PlatformError,
                                    std::string("SDL_UpdateTexture: ") + SDL_GetError());
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    }
    m_atlas = &atlas;
    return core::Ok{};
}

void SdlSpriteRenderer::flush(std::uint16_t page) {
    if (m_indices.empty()) {
        return;
    }
    SDL_Texture* texture = page == kUntextured ? nullptr : m_pages[page];
    if (texture == nullptr) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);  // untextured geometry uses it
    }
    SDL_RenderGeometry(m_renderer, texture, m_vertices.data(), static_cast<int>(m_vertices.size()), m_indices.data(),
                       static_cast<int>(m_indices.size()));
    m_vertices.clear();
    m_indices.clear();
}

std::size_t SdlSpriteRenderer::draw(const std::vector<DrawCommand>& commands, std::int32_t origin_x,
                                    std::int32_t origin_y, std::int32_t scale) {
    if (m_atlas == nullptr || m_pages.empty()) {
        return 0;
    }
    const auto inv_page = 1.0F / static_cast<float>(m_atlas->page_size());
    const SDL_FColor white{1.0F, 1.0F, 1.0F, 1.0F};
    const auto s = static_cast<float>(scale);
    std::size_t drawn = 0;
    std::uint16_t current = 0;
    m_vertices.clear();
    m_indices.clear();
    for (const DrawCommand& c : commands) {
        if (c.sprite == DrawCommand::kOverlayDiamond) {  // untextured translucent tile diamond
            if (current != kUntextured) {
                flush(current);
                current = kUntextured;
            }
            const assets::Rgba rgb = c.tint;
            const SDL_FColor color{rgb.r / 255.0F, rgb.g / 255.0F, rgb.b / 255.0F, rgb.a / 255.0F};
            const float w = static_cast<float>(c.tint_size) * s;
            const float left = static_cast<float>(c.x * scale - origin_x);
            const float bottom = static_cast<float>(c.y * scale - origin_y);
            const float cy = bottom - w / 4.0F;
            const int base = static_cast<int>(m_vertices.size());
            m_vertices.push_back(SDL_Vertex{{left + w / 2.0F, bottom - w / 2.0F}, color, {0, 0}});
            m_vertices.push_back(SDL_Vertex{{left + w, cy}, color, {0, 0}});
            m_vertices.push_back(SDL_Vertex{{left + w / 2.0F, bottom}, color, {0, 0}});
            m_vertices.push_back(SDL_Vertex{{left, cy}, color, {0, 0}});
            m_indices.insert(m_indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
            continue;
        }
        const AtlasRect& r = m_atlas->rect(c.sprite);
        if (r.empty()) {
            continue;
        }
        if (r.page != current) {
            flush(current);
            current = r.page;
        }
        const float x0 = static_cast<float>(c.x * scale - origin_x);
        const float y0 = static_cast<float>((c.y - r.height) * scale - origin_y);
        // Rows cut off at the bottom (figures in crops) are left out of the quad and the texture.
        const std::int32_t height = r.height - std::min<std::int32_t>(c.cut_bottom, r.height);
        if (height <= 0) {
            continue;
        }
        const float x1 = x0 + static_cast<float>(r.width) * s;
        const float y1 = y0 + static_cast<float>(height) * s;
        const float u0 = static_cast<float>(r.x) * inv_page;
        const float v0 = static_cast<float>(r.y) * inv_page;
        const float u1 = static_cast<float>(r.x + r.width) * inv_page;
        const float v1 = static_cast<float>(r.y + height) * inv_page;
        // Shadows: the texture colour multiplied by the (black, translucent) vertex colour;
        // translucent sprites: white with the opacity as alpha.
        SDL_FColor color = white;
        if (c.mode == DrawCommand::Mode::Shadow) {
            color = SDL_FColor{c.tint.r / 255.0F, c.tint.g / 255.0F, c.tint.b / 255.0F, c.tint.a / 255.0F};
        } else if (c.mode == DrawCommand::Mode::Translucent) {
            color.a = c.tint.a / 255.0F;
        }
        const int base = static_cast<int>(m_vertices.size());
        m_vertices.push_back(SDL_Vertex{{x0, y0}, color, {u0, v0}});
        m_vertices.push_back(SDL_Vertex{{x1, y0}, color, {u1, v0}});
        m_vertices.push_back(SDL_Vertex{{x1, y1}, color, {u1, v1}});
        m_vertices.push_back(SDL_Vertex{{x0, y1}, color, {u0, v1}});
        m_indices.insert(m_indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        ++drawn;
    }
    flush(current);
    return drawn;
}

}  // namespace o1602::render
