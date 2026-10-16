// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/camera.hpp"

#include <algorithm>
#include <cmath>

namespace o1602::render {

namespace {

/// Size of the world in view space (rotations 1 and 3 swap width and height).
void view_size(const WorldView& view, std::int32_t& w, std::int32_t& h) {
    const bool swapped = (view.rotation & 1U) != 0;
    w = static_cast<std::int32_t>(swapped ? view.world_height : view.world_width);
    h = static_cast<std::int32_t>(swapped ? view.world_width : view.world_height);
}

TilePos view_to_world(const WorldView& view, std::int32_t vx, std::int32_t vy) {
    const auto W = static_cast<std::int32_t>(view.world_width);
    const auto H = static_cast<std::int32_t>(view.world_height);
    switch (view.rotation & 3U) {
        case 0:
            return {static_cast<std::uint32_t>(vx), static_cast<std::uint32_t>(vy)};
        case 1:
            return {static_cast<std::uint32_t>(vy), static_cast<std::uint32_t>(H - 1 - vx)};
        case 2:
            return {static_cast<std::uint32_t>(W - 1 - vx), static_cast<std::uint32_t>(H - 1 - vy)};
        default:
            return {static_cast<std::uint32_t>(W - 1 - vy), static_cast<std::uint32_t>(vx)};
    }
}

}  // namespace

std::optional<TilePos> world_tile_at(const WorldView& view, double px, double py) {
    std::int32_t vw = 0;
    std::int32_t vh = 0;
    view_size(view, vw, vh);
    const double half_w = tile_width(view.zoom) / 2.0;
    const double half_h = tile_height(view.zoom) / 2.0;
    // Offset from the centre of view tile (0, 0) at (vh * half_w, half_h), in half tiles:
    // s = vx - vy, t = vx + vy. The tile diamonds become unit squares after the 45 degree turn.
    const double s = px / half_w - vh;
    const double t = py / half_h - 1.0;
    const auto vx = static_cast<std::int32_t>(std::floor((s + t) / 2.0 + 0.5));
    const auto vy = static_cast<std::int32_t>(std::floor((t - s) / 2.0 + 0.5));
    if (vx < 0 || vy < 0 || vx >= vw || vy >= vh) {
        return std::nullopt;
    }
    return view_to_world(view, vx, vy);
}

void Camera::set_world_size(std::uint32_t width, std::uint32_t height) {
    m_view.world_width = std::max<std::uint32_t>(1, width);
    m_view.world_height = std::max<std::uint32_t>(1, height);
    clamp();
}

void Camera::set_screen(std::int32_t width, std::int32_t height, std::int32_t pixel_scale) {
    m_width = std::max(1, width);
    m_height = std::max(1, height);
    m_scale = std::clamp(pixel_scale, 1, 8);
    clamp();
}

Viewport Camera::viewport() const {
    const std::int32_t w = (m_width + m_scale - 1) / m_scale;
    const std::int32_t h = (m_height + m_scale - 1) / m_scale;
    return Viewport{static_cast<std::int32_t>(std::floor(m_center_x - m_width / (2.0 * m_scale))),
                    static_cast<std::int32_t>(std::floor(m_center_y - m_height / (2.0 * m_scale))), w + 1, h + 1};
}

void Camera::screen_origin(std::int32_t& x, std::int32_t& y) const {
    x = static_cast<std::int32_t>(std::lround(m_center_x * m_scale - m_width / 2.0));
    y = static_cast<std::int32_t>(std::lround(m_center_y * m_scale - m_height / 2.0));
}

void Camera::center_on(TilePos tile) {
    std::int32_t px = 0;
    std::int32_t py = 0;
    world_tile_center(m_view, tile.x, tile.y, px, py);
    m_center_x = px;
    m_center_y = py;
    clamp();
}

void Camera::scroll(double dx, double dy) {
    m_center_x += dx / m_scale;
    m_center_y += dy / m_scale;
    clamp();
}

void Camera::screen_to_world(double sx, double sy, double& px, double& py) const {
    px = m_center_x + (sx - m_width / 2.0) / m_scale;
    py = m_center_y + (sy - m_height / 2.0) / m_scale;
}

void Camera::place(double px, double py, double sx, double sy) {
    m_center_x = px - (sx - m_width / 2.0) / m_scale;
    m_center_y = py - (sy - m_height / 2.0) / m_scale;
}

std::optional<TilePos> Camera::tile_at_screen(double sx, double sy) const {
    double px = 0;
    double py = 0;
    screen_to_world(sx, sy, px, py);
    return world_tile_at(m_view, px, py);
}

template <class Change>
void Camera::change_view(double sx, double sy, Change change) {
    // Remember the anchor as a tile plus the sub-tile offset of the point from the tile centre
    // (in tile units), so zooming keeps the exact spot, not only the tile.
    double px = 0;
    double py = 0;
    screen_to_world(sx, sy, px, py);
    const auto tile = world_tile_at(m_view, px, py);
    const WorldView old_view = m_view;
    change();
    if (!tile) {
        // Outside the map: scale the centre position proportionally.
        std::int32_t ow = 0;
        std::int32_t oh = 0;
        std::int32_t nw = 0;
        std::int32_t nh = 0;
        world_pixel_size(old_view, ow, oh);
        world_pixel_size(m_view, nw, nh);
        m_center_x = m_center_x * nw / ow;
        m_center_y = m_center_y * nh / oh;
        clamp();
        return;
    }
    std::int32_t ocx = 0;
    std::int32_t ocy = 0;
    world_tile_center(old_view, tile->x, tile->y, ocx, ocy);
    double fx = (px - ocx) / tile_width(old_view.zoom);
    double fy = (py - ocy) / tile_height(old_view.zoom);
    if (old_view.rotation != m_view.rotation) {
        fx = 0;  // a rotated view has no meaningful sub-tile offset
        fy = 0;
    }
    std::int32_t ncx = 0;
    std::int32_t ncy = 0;
    world_tile_center(m_view, tile->x, tile->y, ncx, ncy);
    place(ncx + fx * tile_width(m_view.zoom), ncy + fy * tile_height(m_view.zoom), sx, sy);
    clamp();
}

void Camera::set_zoom(Zoom zoom, double sx, double sy) {
    change_view(sx, sy, [&] { m_view.zoom = zoom; });
}

void Camera::set_rotation(std::uint8_t rotation, double sx, double sy) {
    change_view(sx, sy, [&] { m_view.rotation = static_cast<std::uint8_t>(rotation & 3U); });
}

void Camera::set_pixel_scale(std::int32_t scale, double sx, double sy) {
    double px = 0;
    double py = 0;
    screen_to_world(sx, sy, px, py);
    m_scale = std::clamp(scale, 1, 8);
    place(px, py, sx, sy);
    clamp();
}

void Camera::clamp() {
    std::int32_t w = 0;
    std::int32_t h = 0;
    world_pixel_size(m_view, w, h);
    m_center_x = std::clamp(m_center_x, 0.0, static_cast<double>(w));
    m_center_y = std::clamp(m_center_y, 0.0, static_cast<double>(h));
}

}  // namespace o1602::render
