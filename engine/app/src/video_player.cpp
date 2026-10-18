// SPDX-License-Identifier: GPL-3.0-or-later
#include "video_player.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include "o1602/core/file.hpp"

namespace o1602::app {

namespace {

std::string video_path(std::uint16_t number) {
    return "VideoSmk/" + std::to_string(number) + ".smk";
}

}  // namespace

VideoPlayer::VideoPlayer(SDL_Renderer* renderer, const assets::GameDirectory& game)
    : m_renderer(renderer), m_game(&game) {}

VideoPlayer::~VideoPlayer() {
    if (m_texture != nullptr) {
        SDL_DestroyTexture(m_texture);
    }
}

bool VideoPlayer::available(std::uint16_t number) const {
    const auto it = m_available.find(number);
    if (it != m_available.end()) {
        return it->second;
    }
    const bool found = m_game->resolve(video_path(number)).has_value();
    m_available.emplace(number, found);
    return found;
}

bool VideoPlayer::start(std::uint16_t number, bool loop, std::uint64_t now_ms) {
    stop();
    const auto path = m_game->resolve(video_path(number));
    if (!path) {
        m_available[number] = false;
        return false;
    }
    auto bytes = core::read_file(*path);
    if (!bytes) {
        std::fprintf(stderr, "video %u: %s\n", static_cast<unsigned>(number), bytes.error().message.c_str());
        return false;
    }
    auto video = video::Smacker::open(std::move(*bytes));
    if (!video) {
        std::fprintf(stderr, "video %u: %s\n", static_cast<unsigned>(number), video.error().message.c_str());
        return false;
    }
    m_video.emplace(std::move(*video));
    m_number = number;
    m_loop = loop;
    m_start_ms = now_ms;
    const auto w = static_cast<std::int32_t>(m_video->info().width);
    const auto h = static_cast<std::int32_t>(m_video->info().display_height());
    if (m_texture == nullptr || m_texture_w != w || m_texture_h != h) {
        if (m_texture != nullptr) {
            SDL_DestroyTexture(m_texture);
        }
        m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (m_texture != nullptr) {
            // DirectDraw doubled the frames without smoothing.
            SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);
        }
        m_texture_w = w;
        m_texture_h = h;
    }
    if (m_video->next()) {
        m_dirty = true;
    }
    return true;
}

void VideoPlayer::stop() {
    m_video.reset();
    m_number = 0;
}

VideoPlayer::Step VideoPlayer::update(std::uint64_t now_ms) {
    if (!m_video) {
        return Step::Ended;
    }
    const auto& info = m_video->info();
    const std::uint64_t elapsed_us = (now_ms - m_start_ms) * 1000;
    const std::uint64_t length_us = std::uint64_t{info.frames} * info.frame_us;
    if (elapsed_us >= length_us) {
        if (!m_loop) {
            stop();
            return Step::Ended;
        }
        // The pause video starts again from its first frame.
        m_video->rewind();
        m_start_ms = now_ms;
        if (m_video->next()) {
            m_dirty = true;
        }
        return Step::Restarted;
    }
    // Late frames are decoded (each builds on the one before) but only the last is shown.
    const auto due = static_cast<std::uint32_t>(std::min<std::uint64_t>(elapsed_us / info.frame_us + 1, info.frames));
    while (m_video->position() < due) {
        if (!m_video->next()) {
            std::fprintf(stderr, "video %u: damaged frame %u\n", static_cast<unsigned>(m_number), m_video->position());
            stop();
            return Step::Ended;
        }
        m_dirty = true;
    }
    return Step::Playing;
}

void VideoPlayer::draw(std::int32_t width, std::int32_t height) {
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    if (!m_video || m_texture == nullptr) {
        return;
    }
    if (m_dirty) {
        m_video->to_rgba(m_rgba);
        SDL_UpdateTexture(m_texture, nullptr, m_rgba.data(), m_texture_w * 4);
        m_dirty = false;
    }
    // Twice the size at 1024 pixels width (the original's stretch blit), growing with the window,
    // within it.
    const double vw = m_texture_w;
    const double vh = m_texture_h;
    double scale = 2.0 * static_cast<double>(width) / 1024.0;
    scale = std::min({scale, static_cast<double>(width) / vw, static_cast<double>(height) / vh});
    const double w = std::floor(vw * scale);
    const double h = std::floor(vh * scale);
    const SDL_FRect dst{static_cast<float>(std::floor((width - w) / 2.0)),
                        static_cast<float>(std::floor((height - h) / 2.0)), static_cast<float>(w),
                        static_cast<float>(h)};
    SDL_RenderTexture(m_renderer, m_texture, nullptr, &dst);
}

}  // namespace o1602::app
