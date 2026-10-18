// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/sdl_ui_renderer.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

namespace o1602::render {

namespace {

constexpr int kModeSprite = 0;
constexpr int kModeMask = 1;
constexpr int kModeRaster = 2;
constexpr std::size_t kMaxTexts = 512;  // cached text textures before the cache starts over
constexpr std::int32_t kCheckerSize = 64;

}  // namespace

SdlUiRenderer::SdlUiRenderer(SDL_Renderer* renderer, const assets::GameDirectory& game, const assets::Palette& palette,
                             ui::ArtSize size)
    : m_renderer(renderer), m_palette(palette), m_size(size) {
    for (std::int32_t b = 0; b < static_cast<std::int32_t>(m_sheets.size()); ++b) {
        if (const auto path = game.resolve(ui::sheet_file(b, size))) {
            if (auto sheet = assets::SpriteSheet::load(*path)) {
                m_sheets[static_cast<std::size_t>(b)] = std::move(sheet).value();
            }
        }
    }
    for (std::int32_t f = 0; f < static_cast<std::int32_t>(m_fonts.size()); ++f) {
        if (auto font = ui::Font::load(game, ui::font_file(f, size))) {
            m_fonts[static_cast<std::size_t>(f)] = std::move(font).value();
        }
    }
    for (std::size_t c = 0; c < m_colours.size(); ++c) {
        m_colours[c] = ui::text_recolour(palette, c);
    }
    // A checkerboard (black, every second pixel) for disabled gadgets and 50 % fills.
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kCheckerSize * kCheckerSize * 4), 0);
    for (std::int32_t y = 0; y < kCheckerSize; ++y) {
        for (std::int32_t x = 0; x < kCheckerSize; ++x) {
            rgba[static_cast<std::size_t>((y * kCheckerSize + x) * 4 + 3)] = ((x + y) & 1) == 0 ? 255 : 0;
        }
    }
    m_checker = make(rgba, kCheckerSize, kCheckerSize);
}

SdlUiRenderer::~SdlUiRenderer() {
    const auto destroy = [](auto& map) {
        for (auto& [k, t] : map) {
            SDL_DestroyTexture(t.texture);
        }
    };
    destroy(m_sprites);
    destroy(m_texts);
    destroy(m_images);
    SDL_DestroyTexture(m_checker.texture);
}

std::array<const ui::Font*, 3> SdlUiRenderer::fonts() const noexcept {
    std::array<const ui::Font*, 3> f{};
    for (std::size_t i = 0; i < f.size(); ++i) {
        f[i] = m_fonts[i] ? &*m_fonts[i] : nullptr;
    }
    return f;
}

double SdlUiRenderer::art_scale() const noexcept {
    // The art of the three sizes is drawn for screens 640, 800 and 1024 wide.
    return m_size == ui::ArtSize::Small ? 0.625 : m_size == ui::ArtSize::Medium ? 0.78125 : 1.0;
}

std::optional<std::pair<std::int32_t, std::int32_t>> SdlUiRenderer::sprite_size(std::int32_t block,
                                                                                std::int32_t gfx) const {
    if (block < 0 || block >= static_cast<std::int32_t>(m_sheets.size()) ||
        !m_sheets[static_cast<std::size_t>(block)]) {
        return std::nullopt;
    }
    const auto& sheet = *m_sheets[static_cast<std::size_t>(block)];
    if (gfx < 0 || static_cast<std::size_t>(gfx) >= sheet.size()) {
        return std::nullopt;
    }
    const auto& info = sheet.info(static_cast<std::size_t>(gfx));
    return std::make_pair(static_cast<std::int32_t>(info.width), static_cast<std::int32_t>(info.height));
}

SdlUiRenderer::Texture SdlUiRenderer::make(const std::vector<std::uint8_t>& rgba, std::int32_t w, std::int32_t h) {
    Texture t;
    if (w <= 0 || h <= 0) {
        return t;
    }
    t.texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (t.texture == nullptr) {
        return t;
    }
    SDL_UpdateTexture(t.texture, nullptr, rgba.data(), w * 4);
    SDL_SetTextureBlendMode(t.texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t.texture, SDL_SCALEMODE_NEAREST);
    t.w = w;
    t.h = h;
    return t;
}

SdlUiRenderer::Texture SdlUiRenderer::sprite(std::int32_t block, std::int32_t gfx, int mode, std::int32_t colour) {
    const auto key = std::make_tuple(block, gfx, mode, mode == kModeSprite ? 0 : colour);
    if (const auto it = m_sprites.find(key); it != m_sprites.end()) {
        return it->second;
    }
    Texture t;
    if (block >= 0 && block < static_cast<std::int32_t>(m_sheets.size()) && m_sheets[static_cast<std::size_t>(block)] &&
        gfx >= 0 && static_cast<std::size_t>(gfx) < m_sheets[static_cast<std::size_t>(block)]->size()) {
        if (auto image = m_sheets[static_cast<std::size_t>(block)]->decode(static_cast<std::size_t>(gfx))) {
            std::vector<std::uint8_t> rgba;
            if (mode == kModeSprite) {
                rgba = image->to_rgba(m_palette);
            } else {
                // The shape in one palette colour, solid or as a checkerboard.
                const assets::Rgba c = m_palette[static_cast<std::uint8_t>(std::clamp(colour, 0, 255))];
                rgba.assign(std::size_t{image->width} * image->height * 4, 0);
                for (std::uint32_t y = 0; y < image->height; ++y) {
                    for (std::uint32_t x = 0; x < image->width; ++x) {
                        const std::size_t i = std::size_t{y} * image->width + x;
                        if (image->opaque[i] == 0 || (mode == kModeRaster && ((x + y) & 1U) != 0)) {
                            continue;
                        }
                        rgba[4 * i] = c.r;
                        rgba[4 * i + 1] = c.g;
                        rgba[4 * i + 2] = c.b;
                        rgba[4 * i + 3] = 255;
                    }
                }
            }
            t = make(rgba, static_cast<std::int32_t>(image->width), static_cast<std::int32_t>(image->height));
        }
    }
    m_sprites.emplace(key, t);
    return t;
}

SdlUiRenderer::Texture SdlUiRenderer::text(const std::string& s, std::int32_t font, std::int32_t colour) {
    const auto key = std::make_tuple(s, font, colour);
    if (const auto it = m_texts.find(key); it != m_texts.end()) {
        return it->second;
    }
    if (m_texts.size() >= kMaxTexts) {
        for (auto& [k, t] : m_texts) {
            SDL_DestroyTexture(t.texture);
        }
        m_texts.clear();
    }
    Texture t;
    const ui::Font* f = font >= 0 && font < 3 && m_fonts[static_cast<std::size_t>(font)]
                            ? &*m_fonts[static_cast<std::size_t>(font)]
                            : nullptr;
    if (f != nullptr) {
        assets::IndexedImage canvas;
        canvas.width = f->width(s) + 2;  // glyphs may overhang the last advance
        canvas.height = f->height();
        canvas.indices.assign(std::size_t{canvas.width} * canvas.height, 0);
        canvas.opaque.assign(canvas.indices.size(), 0);
        const ui::Recolour& recolour = m_colours[static_cast<std::size_t>(std::clamp(colour, 0, 9))];
        f->draw(canvas, s, 0, static_cast<std::int32_t>(canvas.height), recolour);
        t = make(canvas.to_rgba(m_palette), static_cast<std::int32_t>(canvas.width),
                 static_cast<std::int32_t>(canvas.height));
    }
    m_texts.emplace(key, t);
    return t;
}

void SdlUiRenderer::fill_with_middle(std::int32_t block, std::int32_t gfx, const ui::Rect& rect, double scale) {
    const Texture t = sprite(block, gfx, kModeSprite, 0);
    if (t.texture == nullptr || t.w < 3) {
        return;
    }
    const double stretch = scale / art_scale();
    const SDL_Rect clip{rect.x, rect.y, rect.w, rect.h};
    SDL_SetRenderClipRect(m_renderer, &clip);
    SDL_SetTextureAlphaMod(t.texture, 255);
    const float third = static_cast<float>(t.w / 3);
    const SDL_FRect src{third, 0.0F, third, static_cast<float>(t.h)};
    const auto step = static_cast<float>(static_cast<double>(third) * stretch);
    for (float x = static_cast<float>(rect.x); x < static_cast<float>(rect.x + rect.w) && step > 0.0F; x += step) {
        const SDL_FRect dst{x, static_cast<float>(rect.y), step, static_cast<float>(t.h * stretch)};
        SDL_RenderTexture(m_renderer, t.texture, &src, &dst);
    }
    SDL_SetRenderClipRect(m_renderer, nullptr);
}

void SdlUiRenderer::fill(const ui::Rect& rect, std::uint8_t colour) {
    const assets::Rgba c = m_palette[colour];
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, 255);
    const SDL_FRect r{static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.w),
                      static_cast<float>(rect.h)};
    SDL_RenderFillRect(m_renderer, &r);
}

void SdlUiRenderer::image(int key, const std::vector<std::uint8_t>& rgba, std::int32_t width, std::int32_t height,
                          const ui::Rect& at, bool update) {
    auto it = m_images.find(key);
    if (it != m_images.end() && (it->second.w != width || it->second.h != height)) {
        SDL_DestroyTexture(it->second.texture);
        m_images.erase(it);
        it = m_images.end();
    }
    if (it == m_images.end()) {
        it = m_images.emplace(key, make(rgba, width, height)).first;
    } else if (update && it->second.texture != nullptr) {
        SDL_UpdateTexture(it->second.texture, nullptr, rgba.data(), width * 4);
    }
    if (it->second.texture != nullptr) {
        const SDL_FRect dst{static_cast<float>(at.x), static_cast<float>(at.y), static_cast<float>(at.w),
                            static_cast<float>(at.h)};
        SDL_RenderTexture(m_renderer, it->second.texture, nullptr, &dst);
    }
}

void SdlUiRenderer::draw(const std::vector<ui::DrawItem>& items, std::int32_t ox, std::int32_t oy, double scale) {
    const double stretch = scale / art_scale();
    const auto blit = [&](const Texture& t, double x, double y, bool half) {
        if (t.texture == nullptr) {
            return;
        }
        // Alphaflg: the original's 50 % checkerboard as real half transparency.
        SDL_SetTextureAlphaMod(t.texture, half ? 128 : 255);
        const SDL_FRect dst{static_cast<float>(ox + x), static_cast<float>(oy + y),
                            static_cast<float>(std::round(t.w * stretch)),
                            static_cast<float>(std::round(t.h * stretch))};
        SDL_RenderTexture(m_renderer, t.texture, nullptr, &dst);
    };
    for (const ui::DrawItem& d : items) {
        switch (d.kind) {
            case ui::DrawItem::Kind::Sprite:
                blit(sprite(d.block, d.gfx, kModeSprite, 0), d.x, d.y, d.half);
                break;
            case ui::DrawItem::Kind::SpriteMask:
                blit(sprite(d.block, d.gfx, kModeMask, d.colour), d.x, d.y, d.half);
                break;
            case ui::DrawItem::Kind::SpriteRaster:
                blit(sprite(d.block, d.gfx, kModeRaster, d.colour), d.x, d.y, false);
                break;
            case ui::DrawItem::Kind::Text: {
                const Texture t = text(d.text, d.block, d.colour);
                const double w = t.w * stretch;
                const double x = d.align == ui::Align::Centre  ? d.x - std::trunc(w / 2)
                                 : d.align == ui::Align::Right ? d.x - w
                                                               : d.x;
                blit(t, x, d.y - t.h * stretch, false);
                break;
            }
            case ui::DrawItem::Kind::Fill: {
                const assets::Rgba c = m_palette[static_cast<std::uint8_t>(std::clamp(d.colour, 0, 255))];
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, d.half ? 128 : 255);
                const SDL_FRect r{static_cast<float>(ox + d.x), static_cast<float>(oy + d.y), static_cast<float>(d.w),
                                  static_cast<float>(d.h)};
                SDL_RenderFillRect(m_renderer, &r);
                break;
            }
            case ui::DrawItem::Kind::Raster:
                if (m_checker.texture != nullptr && d.w > 0 && d.h > 0) {
                    SDL_SetTextureAlphaMod(m_checker.texture, 255);
                    const SDL_FRect src{0.0F, 0.0F, static_cast<float>(std::min(d.w, kCheckerSize)),
                                        static_cast<float>(std::min(d.h, kCheckerSize))};
                    const SDL_FRect dst{static_cast<float>(ox + d.x), static_cast<float>(oy + d.y),
                                        static_cast<float>(d.w), static_cast<float>(d.h)};
                    SDL_RenderTexture(m_renderer, m_checker.texture, &src, &dst);
                }
                break;
        }
    }
}

}  // namespace o1602::render
