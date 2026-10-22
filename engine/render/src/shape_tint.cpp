// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/shape_tint.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "o1602/render/flag_tint.hpp"

namespace o1602::render {

namespace {

double luminance(const std::uint8_t* p) noexcept {
    return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
}

using Span = std::optional<std::pair<std::uint32_t, std::uint32_t>>;

/// Leftmost and rightmost opaque pixel of each row of `shape`, `rows` rows (none below it).
std::vector<Span> row_spans(const RgbaView& shape, std::uint32_t rows) {
    std::vector<Span> spans(rows);
    for (std::uint32_t y = 0; y < std::min(rows, shape.height); ++y) {
        const std::uint8_t* row = shape.pixels + std::size_t{y} * shape.width * 4;
        for (std::uint32_t x = 0; x < shape.width; ++x) {
            if (row[x * 4 + 3] != 0) {
                spans[y] = spans[y] ? std::pair{spans[y]->first, x} : std::pair{x, x};
            }
        }
    }
    // A gap in the outline: at least what the rows above and below have in common.
    std::vector<Span> closed = spans;
    for (std::uint32_t y = 1; y + 1 < rows; ++y) {
        const Span& above = spans[y - 1];
        const Span& below = spans[y + 1];
        if (!above || !below) {
            continue;
        }
        const std::uint32_t left = std::max(above->first, below->first);
        const std::uint32_t right = std::min(above->second, below->second);
        if (left > right) {
            continue;
        }
        closed[y] = spans[y] ? std::pair{std::min(spans[y]->first, left), std::max(spans[y]->second, right)}
                             : std::pair{left, right};
    }
    return closed;
}

}  // namespace

void tint_shape(const RgbaView& sprite, const RgbaView& shape, assets::Rgba color, std::vector<std::uint8_t>& out) {
    out.clear();
    if (sprite.pixels == nullptr) {
        return;
    }
    const std::size_t w = sprite.width;
    out.assign(sprite.pixels, sprite.pixels + w * sprite.height * 4);
    if (shape.pixels == nullptr) {
        return;
    }
    const std::vector<Span> spans = row_spans(shape, sprite.height);
    std::vector<std::size_t> cloth;
    for (std::uint32_t y = 0; y < sprite.height; ++y) {
        if (!spans[y]) {
            continue;
        }
        for (std::uint32_t x = spans[y]->first; x <= spans[y]->second && x < sprite.width; ++x) {
            const std::size_t i = y * w + x;
            if (sprite.pixels[i * 4 + 3] != 0) {
                cloth.push_back(i);
            }
        }
    }
    if (cloth.empty()) {
        return;
    }
    std::vector<double> lum;
    lum.reserve(cloth.size());
    for (const std::size_t i : cloth) {
        lum.push_back(luminance(sprite.pixels + i * 4));
    }
    std::vector<double> sorted = lum;
    const auto quarter = sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() * 3 / 4);
    std::nth_element(sorted.begin(), quarter, sorted.end());
    const double reference = std::max(*quarter, 1.0);
    for (std::size_t k = 0; k < cloth.size(); ++k) {
        const auto shade =
            static_cast<std::uint8_t>(std::clamp(std::lround(kShapeTintReference * lum[k] / reference), 0L, 255L));
        const assets::Rgba c = tint_shade(shade, color);
        out[cloth[k] * 4] = c.r;
        out[cloth[k] * 4 + 1] = c.g;
        out[cloth[k] * 4 + 2] = c.b;
    }
}

}  // namespace o1602::render
