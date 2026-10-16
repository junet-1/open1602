// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/unit_tint.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "o1602/render/flag_tint.hpp"

namespace o1602::render {

std::optional<std::uint32_t> UnitTint::sprite(std::size_t banner, std::uint32_t image) const noexcept {
    if (banner >= banner_base.size() || !banner_base[banner]) {
        return std::nullopt;
    }
    for (const UnitImageGroup& g : groups) {
        if (image < g.first || image >= g.first + g.stride * g.variants) {
            continue;
        }
        return *banner_base[banner] + g.offset + (image - g.first) % g.stride;
    }
    return std::nullopt;
}

namespace {

/// Colour minus its grey part: the direction of its hue.
struct Chroma {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    explicit Chroma(const std::uint8_t* p) noexcept {
        const double mean = (p[0] + p[1] + p[2]) / 3.0;
        r = p[0] - mean;
        g = p[1] - mean;
        b = p[2] - mean;
    }
    [[nodiscard]] double length() const noexcept { return std::sqrt(r * r + g * g + b * b); }
    [[nodiscard]] double dot(const Chroma& o) const noexcept { return r * o.r + g * o.g + b * o.b; }
};

constexpr double kMinColorChroma = 24.0;  // player colours below this have no hue (white)
constexpr double kMinPixelChroma = 12.0;  // pixels below this are grey
constexpr double kMinHueCosine = 0.5;     // within 60 degrees of the player colour

struct Same {
    const std::uint8_t* pixels;
    std::optional<Chroma> hue;  // expected hue of the cloth, none for white
};

/// The pixel of variant `v` shows its own player colour: close to its hue, and closer to it than
/// to the hue of any other variant (brown is near red and yellow, so a brown horse is no cloth).
bool shows_own_hue(const Chroma& pixel, const Same& v, const std::vector<Same>& all) noexcept {
    const double len = pixel.length();
    if (len < kMinPixelChroma) {
        return false;
    }
    const double own = pixel.dot(*v.hue) / (len * v.hue->length());
    if (own < kMinHueCosine) {
        return false;
    }
    for (const Same& o : all) {
        if (&o != &v && o.hue && pixel.dot(*o.hue) / (len * o.hue->length()) > own) {
            return false;
        }
    }
    return true;
}

}  // namespace

void unit_template(std::span<const RgbaView> variants, std::span<const assets::Rgba> colors, std::size_t base,
                   std::vector<std::uint8_t>& out) {
    const RgbaView& b = variants[base];
    const std::size_t n = std::size_t{b.width} * b.height;
    out.assign(b.pixels, b.pixels + n * 4);
    std::vector<Same> same;  // variants with the size of the base
    for (std::size_t v = 0; v < variants.size(); ++v) {
        const RgbaView& view = variants[v];
        if (view.width != b.width || view.height != b.height || view.pixels == nullptr) {
            continue;
        }
        std::optional<Chroma> hue;
        if (v < colors.size()) {
            const std::uint8_t c[3] = {colors[v].r, colors[v].g, colors[v].b};
            if (const Chroma chroma(c); chroma.length() >= kMinColorChroma) {
                hue = chroma;
            }
        }
        same.push_back(Same{view.pixels, hue});
    }
    const auto rgb = [](const std::uint8_t* p) {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16);
    };
    // 1: the variants differ here; 2: they also show their player colours (a seed of cloth).
    std::vector<std::uint8_t> mask(n, 0);
    if (same.size() >= 3) {
        for (std::size_t p = 0; p < n; ++p) {
            const std::uint8_t* o = &out[p * 4];
            if (o[3] == 0) {
                continue;
            }
            std::array<std::uint32_t, 8> seen{};
            std::size_t distinct = 0;
            std::size_t base_count = 0;
            bool opaque = true;
            std::size_t hue_variants = 0;
            std::size_t matches = 0;  // variants that show their own player colour here
            const std::uint32_t base_rgb = rgb(o);
            for (const Same& v : same) {
                const std::uint8_t* q = v.pixels + p * 4;
                if (q[3] == 0) {
                    opaque = false;
                    break;
                }
                if (v.hue) {
                    ++hue_variants;
                    matches += shows_own_hue(Chroma(q), v, same) ? 1U : 0U;
                }
                const std::uint32_t c = rgb(q);
                base_count += c == base_rgb ? 1U : 0U;
                const auto end = seen.begin() + static_cast<std::ptrdiff_t>(distinct);
                if (std::find(seen.begin(), end, c) == end && distinct < seen.size()) {
                    seen[distinct++] = c;
                }
            }
            if (opaque && distinct >= 3 && base_count == 1 && matches >= std::min<std::size_t>(2, hue_variants)) {
                mask[p] = matches == hue_variants ? 2 : 1;
            }
        }
    }
    // Cloth: the seeds and every differing pixel connected to them (8 neighbours). This keeps the
    // dithered edges of sashes and plumes, where single variants stray from their hue, but not the
    // horses, which differ between the variants without showing the player colour anywhere.
    std::vector<std::size_t> stack;
    for (std::size_t p = 0; p < n; ++p) {
        if (mask[p] == 2) {
            stack.push_back(p);
        }
    }
    while (!stack.empty()) {
        const std::size_t p = stack.back();
        stack.pop_back();
        const auto x = static_cast<std::int64_t>(p % b.width);
        const auto y = static_cast<std::int64_t>(p / b.width);
        for (std::int64_t dy = -1; dy <= 1; ++dy) {
            for (std::int64_t dx = -1; dx <= 1; ++dx) {
                const std::int64_t nx = x + dx;
                const std::int64_t ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= b.width || ny >= b.height) {
                    continue;
                }
                const auto q = static_cast<std::size_t>(ny) * b.width + static_cast<std::size_t>(nx);
                if (mask[q] == 1) {
                    mask[q] = 2;
                    stack.push_back(q);
                }
            }
        }
    }
    for (std::size_t p = 0; p < n; ++p) {
        std::uint8_t* o = &out[p * 4];
        if (o[3] == 0) {
            continue;
        }
        if (mask[p] == 2) {
            const double luminance = 0.299 * o[0] + 0.587 * o[1] + 0.114 * o[2];
            const auto shade = static_cast<std::uint8_t>(std::clamp(std::lround(0.48 * luminance + 5.0), 0L, 255L));
            o[0] = o[1] = o[2] = shade;
        } else if (o[0] == o[1] && o[1] == o[2]) {
            o[2] = static_cast<std::uint8_t>(o[2] < 255 ? o[2] + 1 : 254);  // keep it out of the tint
        }
    }
}

TintedUnitSource::TintedUnitSource(const SpriteSource& sheet, std::vector<UnitImageGroup> groups,
                                   std::vector<assets::Rgba> variant_colors, std::vector<assets::Rgba> colors)
    : m_sheet(sheet),
      m_groups(std::move(groups)),
      m_variant_colors(std::move(variant_colors)),
      m_colors(std::move(colors)) {
    for (UnitImageGroup& g : m_groups) {
        g.offset = m_block;
        m_block += g.stride;
    }
}

std::uint32_t TintedUnitSource::count() const {
    return static_cast<std::uint32_t>(m_colors.size()) * m_block;
}

const UnitImageGroup* TintedUnitSource::locate(std::uint32_t i, std::uint32_t& k) const noexcept {
    const std::uint32_t in_block = m_block == 0 ? 0 : i % m_block;
    for (const UnitImageGroup& g : m_groups) {
        if (in_block >= g.offset && in_block < g.offset + g.stride) {
            k = in_block - g.offset;
            return &g;
        }
    }
    return nullptr;
}

void TintedUnitSource::size(std::uint32_t i, std::uint32_t& width, std::uint32_t& height) const {
    std::uint32_t k = 0;
    const UnitImageGroup* g = locate(i, k);
    if (g == nullptr) {
        width = height = 0;
        return;
    }
    m_sheet.size(g->first + g->base * g->stride + k, width, height);
}

bool TintedUnitSource::pixels(std::uint32_t i, std::vector<std::uint8_t>& rgba) const {
    std::uint32_t k = 0;
    const UnitImageGroup* g = locate(i, k);
    if (g == nullptr || g->variants == 0) {
        return false;
    }
    std::vector<std::vector<std::uint8_t>> images(g->variants);
    std::vector<RgbaView> views(g->variants);
    for (std::uint8_t v = 0; v < g->variants; ++v) {
        const std::uint32_t image = g->first + v * g->stride + k;
        std::uint32_t w = 0;
        std::uint32_t h = 0;
        m_sheet.size(image, w, h);
        if (w > 0 && h > 0 && m_sheet.pixels(image, images[v])) {
            views[v] = RgbaView{images[v].data(), w, h};
        }
    }
    if (views[g->base].pixels == nullptr) {
        return false;
    }
    unit_template(views, m_variant_colors, g->base, rgba);
    tint_rgba(rgba, m_colors[i / m_block]);
    return true;
}

}  // namespace o1602::render
