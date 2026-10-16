// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/render/world_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace o1602::render {

namespace {

constexpr std::int32_t floor_div(std::int32_t a, std::int32_t b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

struct RotatedWorld {
    std::uint32_t width;  ///< size in view space
    std::uint32_t height;
    std::uint8_t rotation;

    explicit RotatedWorld(std::uint8_t r)
        : width((r & 1U) != 0 ? sim::World::kHeight : sim::World::kWidth),
          height((r & 1U) != 0 ? sim::World::kWidth : sim::World::kHeight),
          rotation(static_cast<std::uint8_t>(r & 3U)) {}

    /// View space -> world coordinates (same convention as the island renderer).
    void to_world(std::uint32_t vx, std::uint32_t vy, std::uint32_t& wx, std::uint32_t& wy) const {
        constexpr std::uint32_t W = sim::World::kWidth;
        constexpr std::uint32_t H = sim::World::kHeight;
        switch (rotation) {
            case 0:
                wx = vx;
                wy = vy;
                break;
            case 1:
                wx = vy;
                wy = H - 1 - vx;
                break;
            case 2:
                wx = W - 1 - vx;
                wy = H - 1 - vy;
                break;
            default:
                wx = W - 1 - vy;
                wy = vx;
                break;
        }
    }

    void to_view(std::uint32_t wx, std::uint32_t wy, std::uint32_t& vx, std::uint32_t& vy) const {
        constexpr std::uint32_t W = sim::World::kWidth;
        constexpr std::uint32_t H = sim::World::kHeight;
        switch (rotation) {
            case 0:
                vx = wx;
                vy = wy;
                break;
            case 1:
                vx = H - 1 - wy;
                vy = wx;
                break;
            case 2:
                vx = W - 1 - wx;
                vy = H - 1 - wy;
                break;
            default:
                vx = wy;
                vy = W - 1 - wx;
                break;
        }
    }
};

/// World point (continuous tile coordinates) -> world pixel position of the current view
/// (0x0044D940: x = (fx - fy) * w/2, y = (fx + fy) * (h+1)/2 after the view rotation).
void project_point(const WorldView& view, double wx, double wy, double& sx, double& sy) {
    const RotatedWorld rw(view.rotation);
    constexpr double W = sim::World::kWidth;
    constexpr double H = sim::World::kHeight;
    double fx = wx;
    double fy = wy;
    switch (rw.rotation) {
        case 1:
            fx = H - wy;
            fy = wx;
            break;
        case 2:
            fx = W - wx;
            fy = H - wy;
            break;
        case 3:
            fx = wy;
            fy = W - wx;
            break;
        default:
            break;
    }
    sx = (fx - fy + static_cast<double>(rw.height)) * (tile_width(view.zoom) / 2.0);
    sy = (fx + fy) * (tile_height(view.zoom) / 2.0);
}

constexpr std::uint64_t kFlagFrameMs = 90;  // AnimSpeed of the flag figures

}  // namespace

void world_pixel_size(const WorldView& view, std::int32_t& width, std::int32_t& height) {
    const RotatedWorld rw(view.rotation);
    const auto diag = static_cast<std::int32_t>(rw.width + rw.height);
    width = diag * (tile_width(view.zoom) / 2);
    height = diag * (tile_height(view.zoom) / 2);
}

void world_tile_center(const WorldView& view, std::uint32_t x, std::uint32_t y, std::int32_t& px, std::int32_t& py) {
    const RotatedWorld rw(view.rotation);
    std::uint32_t vx = 0;
    std::uint32_t vy = 0;
    rw.to_view(x, y, vx, vy);
    const std::int32_t tw = tile_width(view.zoom);
    const std::int32_t th = tile_height(view.zoom);
    px = (static_cast<std::int32_t>(vx) - static_cast<std::int32_t>(vy) + static_cast<std::int32_t>(rw.height) - 1) *
             (tw / 2) +
         tw / 2;
    py = static_cast<std::int32_t>(vx + vy) * (th / 2) + th / 2;
}

std::uint32_t sea_sprite(const assets::BuildingType& sea, std::uint32_t frame, std::uint32_t x, std::uint32_t y) {
    std::uint32_t sprite = sea.gfx;
    if (sea.anim_count > 1) {
        sprite += ((frame + x * 3 + y) % sea.anim_count) * sea.anim_add;
    }
    return sprite;
}

std::vector<DrawCommand> build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings,
                                               const AnimationClock& clock, const WorldView& view,
                                               const Viewport& viewport, const SpriteExtent& extent) {
    std::vector<DrawCommand> commands;
    build_world_draw_list(world, buildings, clock, view, viewport, extent, commands);
    return commands;
}

void build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings, const AnimationClock& clock,
                           const WorldView& view, const Viewport& viewport, const SpriteExtent& extent,
                           std::vector<DrawCommand>& commands) {
    WorldLayers layers;
    layers.extent = extent;
    build_world_draw_list(world, buildings, clock, view, viewport, layers, commands);
}

bool ship_sprites(const sim::Ship& ship, const sim::World& world, const WorldView& view, const WorldLayers& layers,
                  std::uint64_t time_ms, ShipSprites& out) {
    if (layers.figures == nullptr || layers.atlas == nullptr) {
        return false;
    }
    const assets::FigureType* fig = layers.figures->get(ship.figure);
    if (fig == nullptr || fig->sheet != assets::FigureSheet::Ship) {
        return false;
    }
    const RotatedWorld rw(view.rotation);
    const double tw = tile_width(view.zoom);
    const double hw = tw / 2.0;
    const double hh = tile_height(view.zoom) / 2.0;

    // Figure position: tile centre, projected like the tiles.
    double screen_x = 0.0;
    double screen_y = 0.0;
    project_point(view, ship.x + 0.5, ship.y + 0.5, screen_x, screen_y);

    // Hull: sprite = Gfx + ((direction + 2 * view rotation) & 7) * Rotate (0x0044FA60), centred,
    // bottom at y + Posoffs[0] * w / 64 (field +0x0C of the figure record).
    const std::uint32_t dir = (ship.direction + 2U * rw.rotation) & 7U;
    out.hull.sprite = layers.ship_base + fig->gfx + dir * fig->rotate;
    const AtlasRect& hull = layers.atlas->rect(out.hull.sprite);
    out.hull.x = static_cast<std::int32_t>(std::lround(screen_x)) - hull.width / 2;
    out.hull.y =
        static_cast<std::int32_t>(std::lround(screen_y)) + fig->posoffs[0] * static_cast<std::int32_t>(tw) / 64;
    // The original sorts figures by screen y into the row loop (0x00489A60). Here the ship follows
    // the last tile row whose diamond starts above the hull's bottom edge, so flat sea tiles never
    // cover the hull while land and buildings in front still do.
    out.row = floor_div(out.hull.y - 1, static_cast<std::int32_t>(hh));

    // Flag in the owner's colour (table 0x00498F2C). Offset: Fahnoffs[1] along the ship axis,
    // rotated by (direction + 2) * 45 degrees (0x00443180), projected; lifted by Fahnoffs[2] * w/2.
    out.has_flag = false;
    const sim::Player* owner = world.player(ship.owner);
    const assets::FigureType* flag =
        owner != nullptr ? layers.figures->flag_for_color(static_cast<std::uint8_t>(owner->color)) : nullptr;
    if (flag != nullptr && flag->sheet == assets::FigureSheet::Ship) {
        out.flag.sprite = layers.ship_base + flag->gfx + static_cast<std::uint32_t>((time_ms / kFlagFrameMs) % 8);
        const AtlasRect& fr = layers.atlas->rect(out.flag.sprite);
        const double angle = static_cast<double>((dir + 2U) & 7U) * std::numbers::pi / 4.0;
        const double ox = fig->fahnoffs[1] * std::sin(angle);
        const double oy = fig->fahnoffs[1] * std::cos(angle);
        const double px = (ox - oy) * hw;
        const double py = (ox + oy) * hh;
        out.flag.x = static_cast<std::int32_t>(std::lround(screen_x + px)) - fr.width / 2;
        const double top = screen_y + flag->posoffs[0] * tw / 64.0 - py - fig->fahnoffs[2] * hw;
        out.flag.y = static_cast<std::int32_t>(std::lround(top)) + fr.height;
        out.has_flag = true;
    }
    return true;
}

std::vector<BuildingFlag> collect_building_flags(const sim::World& world, const assets::BuildingTable& buildings,
                                                 const assets::FigureTable& figures) {
    constexpr std::int32_t kProdMarkt = 7;      // HAUS_PRODTYP Kind MARKT
    constexpr std::int32_t kProdKontor = 8;     // KONTOR
    constexpr std::int32_t kProdWachturm = 16;  // WACHTURM: same branch in 0x00486DA0
    std::vector<BuildingFlag> flags;
    for (const sim::WorldIsland& island : world.islands()) {
        const sim::IslandMap& map = island.map;
        for (std::uint32_t y = 0; y < map.height(); ++y) {
            for (std::uint32_t x = 0; x < map.width(); ++x) {
                const sim::MapCell& cell = map.top(x, y);
                if (cell.empty() || cell.owner == sim::MapCell::kNoOwner) {
                    continue;  // the original only spawns flags for buildings inside a city
                }
                const sim::SubTile sub = map.sub_tile(buildings, x, y);
                if (sub.building == nullptr || sub.origin_x != static_cast<std::int32_t>(x) ||
                    sub.origin_y != static_cast<std::int32_t>(y)) {
                    continue;  // one flag per building, at its origin tile
                }
                const assets::BuildingType& b = *sub.building;
                if (b.prod_kind != kProdMarkt && b.prod_kind != kProdKontor && b.prod_kind != kProdWachturm) {
                    continue;
                }
                // Figure 0 is a real definition in figuren.cod, but 0 here means "none" (the pirate
                // warehouse names an undefined FAHNEKONTOR and gets no flag).
                const assets::FigureType* fig =
                    b.attached_figures[0] == 0 ? nullptr : figures.get(b.attached_figures[0]);
                if (fig == nullptr) {
                    continue;
                }
                // Building centre (0x00463A70) plus Fahnoffs rotated with the building (0x004466A0).
                std::uint32_t w = 0;
                std::uint32_t h = 0;
                sim::rotated_size(b, cell.rotation, w, h);
                double cx = island.x + x + w * 0.5;
                double cy = island.y + y + h * 0.5;
                const double f0 = fig->fahnoffs[0];
                const double f1 = fig->fahnoffs[1];
                switch (cell.rotation & 3U) {
                    case 0:
                        cx += f0;
                        cy += f1;
                        break;
                    case 1:
                        cx -= f1;
                        cy += f0;
                        break;
                    case 2:
                        cx -= f0;
                        cy -= f1;
                        break;
                    default:
                        cx += f1;
                        cy -= f0;
                        break;
                }
                flags.push_back(BuildingFlag{cx, cy, fig->fahnoffs[2], static_cast<std::uint16_t>(fig->number),
                                             cell.owner, static_cast<std::uint8_t>((island.x + x + island.y + y) % 8)});
            }
        }
    }
    return flags;
}

bool building_flag_sprite(const BuildingFlag& flag, const sim::World& world, const WorldView& view,
                          const WorldLayers& layers, std::uint64_t time_ms, ShipSprites& out) {
    if (layers.figures == nullptr || layers.atlas == nullptr) {
        return false;
    }
    const assets::FigureType* fig = layers.figures->get(flag.figure);
    const sim::Player* owner = world.player(flag.owner);
    if (fig == nullptr || owner == nullptr || fig->sheet != assets::FigureSheet::Ship) {
        return false;
    }
    // Flag figures have one 8-frame ANIM per colour (FAHNEMARKT: AnimOffs 0, 8, ... 40);
    // the free trader (colour 6) has none.
    const auto color = static_cast<std::uint32_t>(owner->color);
    if (color > 5) {
        return false;
    }
    const auto frame = static_cast<std::uint32_t>((time_ms / kFlagFrameMs + flag.phase) % 8);
    out.hull.sprite = layers.ship_base + fig->gfx + 8U * color + frame;
    out.hull.tint = DrawCommand::kNoTint;
    out.has_flag = false;
    const AtlasRect& r = layers.atlas->rect(out.hull.sprite);
    double sx = 0.0;
    double sy = 0.0;
    project_point(view, flag.x, flag.y, sx, sy);
    const double tw = tile_width(view.zoom);
    // Drawn like a standing figure (0x0044FA90, kind 0x1D): centred, bottom at y - height.
    // The disassembly also adds Posoffs[0] * w/64 (-40 for the flag figures), but a screenshot of
    // the original matches pixel for pixel without it (docs/re/rendering.md section 6).
    out.hull.x = static_cast<std::int32_t>(std::lround(sx)) - r.width / 2;
    out.hull.y = static_cast<std::int32_t>(std::lround(sy - flag.z * tw / 2.0));
    // Depth: the original sorts display figures by y + Posoffs[1] * w/64 (0x00451F74; +40 for the
    // flags), i.e. behind the rows of the building they belong to - otherwise the market hall
    // would cover its own flag.
    const auto key = static_cast<std::int32_t>(std::lround(sy + fig->posoffs[1] * tw / 64.0));
    out.row = floor_div(key - 1, tile_height(view.zoom) / 2);
    return true;
}

void build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings, const AnimationClock& clock,
                           const WorldView& view, const Viewport& viewport, const WorldLayers& layers,
                           std::vector<DrawCommand>& commands) {
    const SpriteExtent& extent = layers.extent;
    commands.clear();
    if (viewport.width <= 0 || viewport.height <= 0) {
        return;
    }
    const RotatedWorld rw(view.rotation);
    const std::int32_t tw = tile_width(view.zoom);
    const std::int32_t th = tile_height(view.zoom);
    const std::int32_t half_w = tw / 2;
    const std::int32_t half_h = th / 2;
    const auto view_h = static_cast<std::int32_t>(rw.height);
    const auto view_w = static_cast<std::int32_t>(rw.width);
    const std::int32_t rows = view_w + view_h - 1;
    const std::int32_t max_elevation = tw;  // Posoffs is at most 64 * tw / 64 (land uses 20)

    // Rows: the bottom of row d is d * half_h + th - 1; its sprites extend upwards by at most
    // max_height + max_elevation pixels.
    const std::int32_t d_begin = std::max(0, floor_div(viewport.top - (th - 1), half_h));
    const std::int32_t d_end = std::min(
        rows - 1, floor_div(viewport.top + viewport.height + extent.height + max_elevation - (th - 1), half_h));
    if (d_begin > d_end) {
        return;
    }
    commands.reserve(static_cast<std::size_t>((viewport.width + extent.width) / half_w + 2) *
                     static_cast<std::size_t>(d_end - d_begin + 1) * 5 / 8);

    const assets::BuildingType* sea = buildings.open_sea();
    const std::uint32_t sea_frame = sea != nullptr ? clock.frame(*sea) : 0;
    const std::int32_t sea_elevation = sea != nullptr ? sea->posoffs * tw / 64 : 0;

    // Ships of the visible rows, sorted by row (stable: file order within a row).
    std::vector<ShipSprites> ships;
    if (layers.figures != nullptr && layers.atlas != nullptr) {
        for (const sim::Ship& ship : world.ships()) {
            ShipSprites s;
            if (ship_sprites(ship, world, view, layers, clock.elapsed_ms(), s) && s.row >= d_begin && s.row <= d_end) {
                ships.push_back(s);
            }
        }
        if (layers.building_flags != nullptr) {
            for (const BuildingFlag& flag : *layers.building_flags) {
                ShipSprites f;
                if (building_flag_sprite(flag, world, view, layers, clock.elapsed_ms(), f) && f.row >= d_begin &&
                    f.row <= d_end) {
                    ships.push_back(f);
                }
            }
        }
        std::stable_sort(ships.begin(), ships.end(),
                         [](const ShipSprites& a, const ShipSprites& b) { return a.row < b.row; });
    }
    std::size_t next_ship = 0;

    // Ownership view: a translucent diamond on the tile surface, after the flat pass of the row.
    std::vector<DrawCommand> overlays;
    const auto add_overlay = [&](const sim::MapCell& cell, std::int32_t sx, std::int32_t row_bottom) {
        const sim::Player* p = world.player(cell.owner);
        if (!layers.owner_tint || p == nullptr) {
            return;
        }
        const assets::BuildingType* b = buildings.by_sprite(cell.sprite);
        const std::int32_t elevation = b != nullptr ? b->posoffs * tw / 64 : 0;
        overlays.push_back(DrawCommand{DrawCommand::kOverlayDiamond, sx, row_bottom - elevation,
                                       static_cast<std::uint8_t>(p->color), static_cast<std::uint8_t>(tw)});
    };
    const auto emit = [&](const sim::MapCell& cell, std::int32_t sx, std::int32_t row_bottom) {
        const assets::BuildingType* b = buildings.by_sprite(cell.sprite);
        if (b == nullptr) {
            commands.push_back(DrawCommand{cell.sprite, sx, row_bottom});
            return;
        }
        commands.push_back(DrawCommand{animated_sprite(cell, b, clock.frame(*b), view.rotation), sx,
                                       row_bottom - b->posoffs * tw / 64});
    };

    for (std::int32_t d = d_begin; d <= d_end; ++d) {
        const std::int32_t row_bottom = d * half_h + th - 1;
        // Left edge of tile vx in this row: X = (2 * vx - d + view_h - 1) * half_w.
        // Visible if X + max_width > left and X < left + width (conservative bounds).
        const std::int32_t k_lo = floor_div(viewport.left - extent.width, half_w) + d - view_h + 1;
        const std::int32_t k_hi = floor_div(viewport.left + viewport.width, half_w) + d - view_h + 1;
        const std::int32_t vx_lo = std::max({0, d - view_h + 1, -floor_div(-k_lo, 2)});
        const std::int32_t vx_hi = std::min({view_w - 1, d, floor_div(k_hi, 2)});
        for (int pass = 0; pass < 2; ++pass) {
            const bool high_pass = pass == 1;
            for (std::int32_t vx = vx_lo; vx <= vx_hi; ++vx) {
                const std::int32_t vy = d - vx;
                const std::int32_t sx = (vx - vy + view_h - 1) * half_w;
                std::uint32_t wx = 0;
                std::uint32_t wy = 0;
                rw.to_world(static_cast<std::uint32_t>(vx), static_cast<std::uint32_t>(vy), wx, wy);
                const sim::World::Cell& wc = world.cell(wx, wy);
                const sim::IslandMap* map = nullptr;
                const sim::MapCell* cell = nullptr;
                if (wc.island != sim::World::kSea) {
                    map = &world.islands()[wc.island].map;
                    cell = &map->top(wc.local_x, wc.local_y);
                }
                if (cell == nullptr || cell->empty()) {  // open sea, always flat
                    if (!high_pass && sea != nullptr) {
                        commands.push_back(
                            DrawCommand{sea_sprite(*sea, sea_frame, wx, wy), sx, row_bottom - sea_elevation});
                    }
                    continue;
                }
                if (!high_pass) {
                    add_overlay(*cell, sx, row_bottom);
                }
                const assets::BuildingType* b = buildings.by_sprite(cell->sprite);
                if ((b != nullptr && b->high) != high_pass) {
                    continue;
                }
                if (b != nullptr && b->draw_ground) {
                    const sim::MapCell& ground = map->ground(wc.local_x, wc.local_y);
                    if (!ground.empty()) {
                        emit(ground, sx, row_bottom);
                    }
                }
                emit(*cell, sx, row_bottom);
            }
            if (!high_pass) {
                commands.insert(commands.end(), overlays.begin(), overlays.end());
                overlays.clear();
            }
            if (!high_pass) {  // figures of this row between the two passes
                for (; next_ship < ships.size() && ships[next_ship].row <= d; ++next_ship) {
                    commands.push_back(ships[next_ship].hull);
                    if (ships[next_ship].has_flag) {
                        commands.push_back(ships[next_ship].flag);
                    }
                }
            }
        }
    }
}

}  // namespace o1602::render
