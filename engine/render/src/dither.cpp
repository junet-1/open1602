// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/dither.hpp"

namespace o1602::render {

void DitheredSource::size(std::uint32_t i, std::uint32_t& width, std::uint32_t& height) const {
    m_source.size(i % m_source.count(), width, height);
}

bool DitheredSource::pixels(std::uint32_t i, std::vector<std::uint8_t>& rgba) const {
    const std::uint32_t n = m_source.count();
    if (!m_source.pixels(i % n, rgba)) {
        return false;
    }
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    m_source.size(i % n, width, height);
    const std::uint32_t phase = i / n;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = (y + phase + 1) & 1U; x < width; x += 2) {
            rgba[(std::size_t{y} * width + x) * 4 + 3] = 0;
        }
    }
    return true;
}

void Dither::apply(DrawCommand& c, const SpriteAtlas& atlas) const noexcept {
    if (c.mode != DrawCommand::Mode::Shadow && c.mode != DrawCommand::Mode::Translucent) {
        return;
    }
    for (const DitheredSheet& s : sheets) {
        if (c.sprite < s.base || c.sprite - s.base >= s.count) {
            continue;
        }
        const AtlasRect& r = atlas.rect(c.sprite);
        // The block whose kept pixels fall on the hatched pixels at the sprite's top left corner.
        const auto phase = static_cast<std::uint32_t>(hatch_pixel(c.x, c.y - r.height) ? 0 : 1);
        c.sprite = s.dithered + phase * s.count + (c.sprite - s.base);
        if (c.mode == DrawCommand::Mode::Shadow) {
            c.tint.a = 255;
        } else {
            c.mode = DrawCommand::Mode::Normal;
        }
        return;
    }
}

}  // namespace o1602::render
