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

    std::uint32_t world_width;  ///< size in world space
    std::uint32_t world_height;

    explicit RotatedWorld(const WorldView& view)
        : width((view.rotation & 1U) != 0 ? view.world_height : view.world_width),
          height((view.rotation & 1U) != 0 ? view.world_width : view.world_height),
          rotation(static_cast<std::uint8_t>(view.rotation & 3U)),
          world_width(view.world_width),
          world_height(view.world_height) {}

    /// View space -> world coordinates (same convention as the island renderer).
    void to_world(std::uint32_t vx, std::uint32_t vy, std::uint32_t& wx, std::uint32_t& wy) const {
        const std::uint32_t W = world_width;
        const std::uint32_t H = world_height;
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
        const std::uint32_t W = world_width;
        const std::uint32_t H = world_height;
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
    const RotatedWorld rw(view);
    const double W = rw.world_width;
    const double H = rw.world_height;
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

/// Image `frame` of a banner's flag: the tinted generic flag or the banner's own sprites.
std::optional<std::uint32_t> flag_sprite(const WorldLayers& layers, sim::BannerId id, std::uint64_t frame) {
    const Banner* banner = layers.banners != nullptr ? layers.banners->get(id) : nullptr;
    if (banner == nullptr || banner->flag_frames == 0) {
        return std::nullopt;
    }
    const auto f = static_cast<std::uint32_t>(frame % banner->flag_frames);
    if (banner->flag == FlagStyle::Tinted && layers.banner_flags != nullptr && id < layers.banner_flags->size() &&
        (*layers.banner_flags)[id]) {
        return *(*layers.banner_flags)[id] + f;
    }
    if (banner->flag == FlagStyle::Sprites) {
        return layers.ship_base + banner->flag_gfx + f;
    }
    return std::nullopt;
}

}  // namespace

void world_pixel_size(const WorldView& view, std::int32_t& width, std::int32_t& height) {
    const RotatedWorld rw(view);
    const auto diag = static_cast<std::int32_t>(rw.width + rw.height);
    width = diag * (tile_width(view.zoom) / 2);
    height = diag * (tile_height(view.zoom) / 2);
}

void world_tile_center(const WorldView& view, std::uint32_t x, std::uint32_t y, std::int32_t& px, std::int32_t& py) {
    const RotatedWorld rw(view);
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

bool ship_sprites(const sim::Ship& ship, const sim::World& world, const WorldView& view_in, const WorldLayers& layers,
                  std::uint64_t time_ms, ShipSprites& out) {
    const WorldView view = fit_view(view_in, world);
    if (layers.figures == nullptr || layers.atlas == nullptr) {
        return false;
    }
    const assets::FigureType* fig = layers.figures->get(ship.figure);
    if (fig == nullptr || fig->sheet != assets::FigureSheet::Ship) {
        return false;
    }
    const RotatedWorld rw(view);
    const double tw = tile_width(view.zoom);
    const double hw = tw / 2.0;
    const double hh = tile_height(view.zoom) / 2.0;

    // Figure position: tile centre, projected like the tiles.
    double screen_x = 0.0;
    double screen_y = 0.0;
    project_point(view, ship.x + 0.5, ship.y + 0.5, screen_x, screen_y);

    // The original keeps figures at Y = y + Posoffs[1] * w/64 (0x00451CC5) and sorts them by Y into
    // the row loop (0x00489A60).
    const double depth = screen_y + fig->posoffs[1] * tw / 64.0;
    out.row = floor_div(static_cast<std::int32_t>(std::lround(depth)) - 1, static_cast<std::int32_t>(hh));

    // Hull: sprite = Gfx + ((direction + 2 * view rotation) & 7) * Rotate (0x0044FA60), centred,
    // bottom at Y + Posoffs[0] * w/64 (field +0x0C of the figure record).
    const std::uint32_t dir = (ship.direction + 2U * rw.rotation) & 7U;
    out.hull.sprite = layers.ship_base + fig->gfx + dir * fig->rotate;
    const AtlasRect& hull = layers.atlas->rect(out.hull.sprite);
    out.hull.x = static_cast<std::int32_t>(std::lround(screen_x)) - hull.width / 2;
    out.hull.y = static_cast<std::int32_t>(std::lround(depth + fig->posoffs[0] * tw / 64.0));

    // Flag of the owner's banner (original: figure from table 0x00498F2C). Offset: Fahnoffs[1]
    // along the ship axis, rotated by (direction + 2) * 45 degrees (0x00443180), projected; lifted
    // by Fahnoffs[2] * w/2 from Y and hanging down from there. (The original also adds the flag
    // figure's Posoffs[0], which is 0 for all flags.)
    out.has_flag = false;
    const sim::Player* owner = world.player(ship.owner);
    const auto flag = owner != nullptr ? flag_sprite(layers, owner->banner, time_ms / kFlagFrameMs) : std::nullopt;
    if (flag) {
        out.flag.sprite = *flag;
        const AtlasRect& fr = layers.atlas->rect(out.flag.sprite);
        const double angle = static_cast<double>((dir + 2U) & 7U) * std::numbers::pi / 4.0;
        const double ox = fig->fahnoffs[1] * std::sin(angle);
        const double oy = fig->fahnoffs[1] * std::cos(angle);
        const double px = (ox - oy) * hw;
        const double py = (ox + oy) * hh;
        out.flag.x = static_cast<std::int32_t>(std::lround(screen_x + px)) - fr.width / 2;
        const double top = depth - py - fig->fahnoffs[2] * hw;
        out.flag.y = static_cast<std::int32_t>(std::lround(top)) + fr.height;
        out.has_flag = true;
    }
    return true;
}

namespace {

constexpr std::uint16_t kFigureNoAktiv = 46;  // NOAKTIV: status icon of production buildings

/// Building centre (0x00463A70) plus the figure's Fahnoffs turned with the building (0x004466A0,
/// the same for flags, smoke, icons and jugglers).
void attach_point(const sim::WorldIsland& island, const assets::BuildingType& b, std::uint32_t x, std::uint32_t y,
                  std::uint8_t rotation, const assets::FigureType& fig, double& cx, double& cy) {
    std::uint32_t w = 0;
    std::uint32_t h = 0;
    sim::rotated_size(b, rotation, w, h);
    cx = island.x + x + w * 0.5;
    cy = island.y + y + h * 0.5;
    const double f0 = fig.fahnoffs[0];
    const double f1 = fig.fahnoffs[1];
    switch (rotation & 3U) {
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
}

}  // namespace

std::vector<BuildingFigure> collect_building_figures(const sim::World& world, const assets::BuildingTable& buildings,
                                                     const assets::FigureTable& figures) {
    constexpr std::int32_t kProdMarkt = 7;      // HAUS_PRODTYP Kind MARKT
    constexpr std::int32_t kProdKontor = 8;     // KONTOR
    constexpr std::int32_t kProdWachturm = 16;  // WACHTURM: same branch in 0x00486B60
    std::vector<BuildingFigure> out;
    const auto& islands = world.islands();
    for (std::size_t index = 0; index < islands.size(); ++index) {
        const sim::WorldIsland& island = islands[index];
        const sim::IslandMap& map = island.map;
        for (std::uint32_t y = 0; y < map.height(); ++y) {
            for (std::uint32_t x = 0; x < map.width(); ++x) {
                const sim::MapCell& cell = map.top(x, y);
                if (cell.empty() || cell.owner == sim::kNoPlayer) {
                    continue;  // the original only spawns figures for buildings inside a city
                }
                const sim::SubTile sub = map.sub_tile(buildings, x, y);
                if (sub.building == nullptr || sub.origin_x != static_cast<std::int32_t>(x) ||
                    sub.origin_y != static_cast<std::int32_t>(y)) {
                    continue;  // one set of figures per building, at its origin tile
                }
                const assets::BuildingType& b = *sub.building;
                const auto add = [&](BuildingFigureRole role, std::uint16_t number) {
                    // Figure 0 is a real definition in figuren.cod, but 0 here means "none" (the
                    // pirate warehouse names an undefined FAHNEKONTOR and gets no flag, D-002).
                    const assets::FigureType* fig = number == 0 ? nullptr : figures.get(number);
                    if (fig == nullptr) {
                        return;
                    }
                    BuildingFigure f;
                    f.role = role;
                    attach_point(island, b, x, y, cell.rotation, *fig, f.x, f.y);
                    f.z = fig->fahnoffs[2];
                    f.figure = number;
                    f.owner = cell.owner;
                    f.phase = static_cast<std::uint8_t>((island.x + x) * 7 + (island.y + y) * 13);
                    f.island = static_cast<std::uint16_t>(index);
                    f.origin_x = static_cast<std::uint16_t>(x);
                    f.origin_y = static_cast<std::uint16_t>(y);
                    out.push_back(f);
                };
                if (b.prod_kind == kProdMarkt || b.prod_kind == kProdKontor || b.prod_kind == kProdWachturm) {
                    add(BuildingFigureRole::Flag, b.attached_figures[0]);
                } else if (sim::is_producer(b)) {
                    add(BuildingFigureRole::Status, kFigureNoAktiv);
                    add(BuildingFigureRole::Smoke, b.attached_figures[0]);
                }
            }
        }
    }
    return out;
}

namespace {

/// Body of a display figure at world point (x, y) with height z (0x0044FA90): centred, bottom at
/// Y + Posoffs[0] * w/64 - z * w/2, where Y = y + Posoffs[1] * w/64 is also its depth (0x00451F74).
/// For flags and smoke both offsets cancel out (Posoffs -40, 40 / -20, 20 ...).
void place_display_figure(const WorldView& view, const AtlasRect& r, const assets::FigureType& fig, double x, double y,
                          double z, DrawCommand& c, std::int32_t& row) {
    double sx = 0.0;
    double sy = 0.0;
    project_point(view, x, y, sx, sy);
    const double tw = tile_width(view.zoom);
    const double depth = sy + fig.posoffs[1] * tw / 64.0;
    c.x = static_cast<std::int32_t>(std::lround(sx)) - r.width / 2;
    c.y = static_cast<std::int32_t>(std::lround(depth + fig.posoffs[0] * tw / 64.0 - z * tw / 2.0));
    row = floor_div(static_cast<std::int32_t>(std::lround(depth)) - 1, tile_height(view.zoom) / 2);
}

}  // namespace

bool building_figure_sprites(const BuildingFigure& figure, const sim::World& world,
                             const assets::BuildingTable& buildings, const WorldView& view_in,
                             const WorldLayers& layers, std::uint64_t time_ms, FigureSprites& out) {
    out.count = 0;
    if (layers.figures == nullptr || layers.atlas == nullptr) {
        return false;
    }
    const assets::FigureType* fig = layers.figures->get(figure.figure);
    if (fig == nullptr) {
        return false;
    }
    const WorldView view = fit_view(view_in, world);
    DrawCommand& c = out.parts[0];
    c = DrawCommand{};
    switch (figure.role) {
        case BuildingFigureRole::Flag: {
            // Flag figures have one 8-frame ANIM per original colour (FAHNEMARKT: AnimOffs 0, 8, ...
            // 40 = the same sprites as the ship flags), so the banner's flag images are used directly.
            const sim::Player* owner = world.player(figure.owner);
            if (owner == nullptr || fig->sheet != assets::FigureSheet::Ship) {
                return false;
            }
            const auto sprite = flag_sprite(layers, owner->banner, time_ms / kFlagFrameMs + figure.phase);
            if (!sprite) {
                return false;
            }
            c.sprite = *sprite;
            break;
        }
        case BuildingFigureRole::Smoke:
        case BuildingFigureRole::Status: {
            const sim::Production* state = world.production(figure.island, figure.origin_x, figure.origin_y);
            // The draw kinds pick their sheet (0x0044FA90: kinds 0x16/0x17 always EFFEKTE.BSH), not
            // the figure's Blocknr - the smoke and icon figures have none.
            const auto base = layers.sheet_base[static_cast<std::size_t>(assets::FigureSheet::Effekte)];
            if (state == nullptr || !base) {
                return false;
            }
            std::uint8_t anim = 0;
            if (figure.role == BuildingFigureRole::Smoke) {
                if (!sim::smoke_visible(*state)) {  // 0x0047CF30
                    return false;
                }
                // Drawn every second pixel (0x00401D50 -> 0x004048E0): 50 % translucent.
                c.mode = DrawCommand::Mode::Translucent;
                c.tint = assets::Rgba{255, 255, 255, kSmokeAlpha};
            } else {
                // Icon of the local player's buildings: anim 0 switched off, 1 no raw material
                // (0x00451E8D with 0x0047CDD0).
                if (figure.owner != layers.local_player || figure.owner == sim::kNoPlayer) {
                    return false;
                }
                const sim::MapCell& cell = world.islands()[figure.island].map.top(figure.origin_x, figure.origin_y);
                const assets::BuildingType* b = buildings.by_sprite(cell.sprite);
                const sim::ProductionStatus status =
                    b != nullptr ? sim::production_status(*state, *b) : sim::ProductionStatus::Ok;
                if (status == sim::ProductionStatus::Ok) {
                    return false;
                }
                anim = status == sim::ProductionStatus::Disabled ? 0 : 1;
            }
            // Endless animations: the original starts them at a random frame (0x0044670B); here the
            // building's phase stands in for it so the picture is reproducible.
            const assets::FigureAnim* a = fig->anim(anim);
            std::uint32_t frame = 0;
            if (a != nullptr && a->count > 0) {
                const std::uint64_t speed = a->speed_ms > 0 ? a->speed_ms : 1;
                frame = static_cast<std::uint32_t>((time_ms / speed + figure.phase) % a->count);
            }
            c.sprite = *base + assets::figure_sprite(*fig, anim, frame, 0, view.rotation);
            break;
        }
    }
    const AtlasRect& r = layers.atlas->rect(c.sprite);
    if (r.empty()) {
        return false;
    }
    place_display_figure(view, r, *fig, figure.x, figure.y, figure.z, c, out.row);
    out.count = 1;
    return true;
}

bool land_figure_sprites(const FigureInstance& figure, const sim::World& world, const WorldView& view_in,
                         const WorldLayers& layers, FigureSprites& out) {
    out.count = 0;
    if (layers.figures == nullptr || layers.atlas == nullptr) {
        return false;
    }
    const assets::FigureType* type = layers.figures->get(figure.figure);
    if (type == nullptr) {
        return false;
    }
    const auto base = layers.sheet_base[static_cast<std::size_t>(type->sheet)];
    if (!base) {
        return false;  // no graphics for this figure at this zoom level
    }
    const WorldView view = fit_view(view_in, world);
    const std::int32_t tw = tile_width(view.zoom);
    double fx = 0.0;
    double fy = 0.0;
    project_point(view, figure.x, figure.y, fx, fy);
    const auto px = static_cast<std::int32_t>(std::lround(fx));
    // The original stores every figure at Y = y + Posoffs[1] * w/64 (0x00451CC5); Y is its depth and
    // the reference for body and shadow.
    const std::int32_t depth = static_cast<std::int32_t>(std::lround(fy)) + type->posoffs[1] * tw / 64;

    // Shadow first (drawn below the body), centred on Y - Posoffs[1] * w/64, i.e. the ground point.
    const assets::FigureType* shadow = layers.figures->get(type->shadow);
    const auto shadow_base = layers.sheet_base[static_cast<std::size_t>(assets::FigureSheet::Schatten)];
    if (type->shadow != 0 && shadow != nullptr && shadow_base) {
        DrawCommand& c = out.parts[out.count];
        c.sprite =
            *shadow_base + assets::figure_sprite(*shadow, figure.anim, figure.frame, figure.direction, view.rotation);
        const AtlasRect& r = layers.atlas->rect(c.sprite);
        if (!r.empty()) {
            c.x = px - r.width / 2;
            c.y = depth - type->posoffs[1] * tw / 64 - r.height / 2 + r.height;
            c.mode = DrawCommand::Mode::Shadow;
            c.tint = assets::Rgba{0, 0, 0, kShadowAlpha};
            ++out.count;
        }
    }
    DrawCommand& body = out.parts[out.count];
    body = DrawCommand{};
    body.sprite = *base + assets::figure_sprite(*type, figure.anim, figure.frame, figure.direction, view.rotation);
    const AtlasRect& r = layers.atlas->rect(body.sprite);
    if (r.empty()) {
        out.count = 0;
        return false;
    }
    body.x = px - r.width / 2;
    body.y = depth + type->posoffs[0] * tw / 64;
    ++out.count;
    out.row = floor_div(depth - 1, tile_height(view.zoom) / 2);
    return true;
}

void build_world_draw_list(const sim::World& world, const assets::BuildingTable& buildings, const AnimationClock& clock,
                           const WorldView& view_in, const Viewport& viewport, const WorldLayers& layers,
                           std::vector<DrawCommand>& commands) {
    const WorldView view = fit_view(view_in, world);
    const SpriteExtent& extent = layers.extent;
    commands.clear();
    if (viewport.width <= 0 || viewport.height <= 0) {
        return;
    }
    const RotatedWorld rw(view);
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

    // Figures of the visible rows (ships, building flags, land figures), sorted by row
    // (stable: within a row in this order).
    std::vector<FigureSprites> figures;
    const auto add_figure = [&](const FigureSprites& f) {
        if (f.row >= d_begin && f.row <= d_end) {
            figures.push_back(f);
        }
    };
    if (layers.figures != nullptr && layers.atlas != nullptr) {
        for (const sim::Ship& ship : world.ships()) {
            ShipSprites s;
            if (ship_sprites(ship, world, view, layers, clock.elapsed_ms(), s)) {
                FigureSprites f;
                f.row = s.row;
                f.parts[f.count++] = s.hull;
                if (s.has_flag) {
                    f.parts[f.count++] = s.flag;
                }
                add_figure(f);
            }
        }
        if (layers.building_figures != nullptr) {
            for (const BuildingFigure& bf : *layers.building_figures) {
                FigureSprites f;
                if (building_figure_sprites(bf, world, buildings, view, layers, clock.elapsed_ms(), f)) {
                    add_figure(f);
                }
            }
        }
        if (layers.land_figures != nullptr) {
            for (const FigureInstance& instance : *layers.land_figures) {
                FigureSprites f;
                if (land_figure_sprites(instance, world, view, layers, f)) {
                    add_figure(f);
                }
            }
        }
        std::stable_sort(figures.begin(), figures.end(),
                         [](const FigureSprites& a, const FigureSprites& b) { return a.row < b.row; });
    }
    std::size_t next_figure = 0;

    // Ownership view: a translucent diamond on the tile surface, after the flat pass of the row.
    std::vector<DrawCommand> overlays;
    const auto add_overlay = [&](const sim::MapCell& cell, std::int32_t sx, std::int32_t row_bottom) {
        const sim::Player* p = world.player(cell.owner);
        const Banner* banner = p != nullptr && layers.banners != nullptr ? layers.banners->get(p->banner) : nullptr;
        if (!layers.owner_tint || banner == nullptr) {
            return;
        }
        const assets::BuildingType* b = buildings.by_sprite(cell.sprite);
        const std::int32_t elevation = b != nullptr ? b->posoffs * tw / 64 : 0;
        const assets::Rgba tint{banner->color.r, banner->color.g, banner->color.b, kOverlayAlpha};
        overlays.push_back(
            DrawCommand{DrawCommand::kOverlayDiamond, sx, row_bottom - elevation, tint, static_cast<std::uint8_t>(tw)});
    };
    // Tiles of production buildings take their frame from the building's state (0x00489C90);
    // `island` is kSea for tiles without such state (the ground layer).
    const auto emit = [&](const sim::MapCell& cell, std::int32_t sx, std::int32_t row_bottom, std::uint16_t island,
                          const sim::IslandMap* map, std::uint16_t lx, std::uint16_t ly) {
        const assets::BuildingType* b = buildings.by_sprite(cell.sprite);
        if (b == nullptr) {
            commands.push_back(DrawCommand{cell.sprite, sx, row_bottom});
            return;
        }
        std::uint32_t frame = 0;
        if (island != sim::World::kSea && has_production_animation(*b)) {
            const sim::SubTile sub = map->sub_tile(buildings, lx, ly);
            const sim::Production* state = world.production(island, static_cast<std::uint16_t>(sub.origin_x),
                                                            static_cast<std::uint16_t>(sub.origin_y));
            frame = production_frame(*b, clock.frame(*b), state);
        } else {
            frame = default_frame(cell, *b, clock.frame(*b));
        }
        commands.push_back(
            DrawCommand{building_sprite(cell, *b, frame, view.rotation), sx, row_bottom - b->posoffs * tw / 64});
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
                        emit(ground, sx, row_bottom, sim::World::kSea, map, 0, 0);
                    }
                }
                emit(*cell, sx, row_bottom, wc.island, map, wc.local_x, wc.local_y);
            }
            if (!high_pass) {
                commands.insert(commands.end(), overlays.begin(), overlays.end());
                overlays.clear();
            }
            if (!high_pass) {  // figures of this row between the two passes
                for (; next_figure < figures.size() && figures[next_figure].row <= d; ++next_figure) {
                    const FigureSprites& f = figures[next_figure];
                    commands.insert(commands.end(), f.parts.begin(), f.parts.begin() + f.count);
                }
            }
        }
    }
}

}  // namespace o1602::render
