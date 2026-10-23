// SPDX-License-Identifier: GPL-3.0-or-later
#include "goods_window.hpp"

#include <algorithm>
#include <cstdlib>
#include <string_view>
#include <utility>

#include "o1602/assets/game_data.hpp"
#include "o1602/assets/savegame.hpp"
#include "o1602/sim/goods_statistics.hpp"
#include "o1602/ui/localization.hpp"

namespace o1602::app {

namespace {

using Id = ui::Form::Id;
using Tone = ui::Form::Tone;
using sim::GoodsFlow;
using ui::Align;
using ui::Rect;

constexpr Id kIsland = 1;
constexpr Id kWindow = 2;
constexpr Id kAllGoods = 3;
constexpr Id kClose = 4;
constexpr Id kBack = 5;
constexpr Id kChart = 6;
constexpr Id kPageUp = 7;
constexpr Id kPageDown = 8;
constexpr Id kRow = 100;  // + ware

constexpr std::int32_t kMargin = 24;
constexpr std::int32_t kControlsY = 52;
constexpr std::int32_t kControlH = 32;
constexpr std::int32_t kHeaderY = 96;
constexpr std::int32_t kTableY = 120;
constexpr std::int32_t kRowH = 24;
constexpr std::int32_t kBottomY = GoodsWindow::kHeight - 52;

constexpr std::int32_t kFontNormal = 0;
constexpr std::int32_t kFontSmall = 1;

/// The table's columns: where the numbers end (right aligned) and what heads them.
struct Column {
    std::int32_t right;
    std::string_view heading;
};
constexpr std::int32_t kNameX = 52;
constexpr std::array<Column, 7> kColumns = {{
    {266, "stats.col.stock"},
    {336, "stats.col.produced"},
    {406, "stats.col.imported"},
    {476, "stats.col.consumed"},
    {546, "stats.col.exported"},
    {616, "stats.col.balance"},
    {682, "stats.col.lasts"},
}};
constexpr std::int32_t kSparkX = 692;
constexpr std::int32_t kSparkW = 46;
constexpr std::int32_t kScrollX = GoodsWindow::kWidth - 18;
constexpr std::int32_t kScrollW = 8;

/// The groups of the table, in the order of the original's Kontor window.
struct Group {
    std::string_view title;
    std::vector<std::uint8_t> wares;
};
const std::array<Group, 4>& groups() {
    namespace w = assets::ware;
    static const std::array<Group, 4> list = {{
        {"stats.group.consumer",
         {w::kFood, w::kTobaccoGoods, w::kSpices, w::kCocoa, w::kAlcohol, w::kCloth, w::kClothes, w::kJewellery}},
        {"stats.group.building", {w::kTools, w::kWood, w::kBricks}},
        {"stats.group.raw",
         {w::kOre, w::kGold, w::kWool, w::kSugar, w::kTobacco, w::kMeat, w::kGrain, w::kFlour, w::kIron}},
        {"stats.group.weapons", {w::kSwords, w::kMuskets, w::kCannons}},
    }};
    return list;
}

/// What came in from outside the production: bought, own ships, given back.
std::uint64_t imported(const sim::FlowTotals& t) {
    return t.of(GoodsFlow::Bought) + t.of(GoodsFlow::ShippedIn) + t.of(GoodsFlow::Returned);
}
/// What was used up in the cities.
std::uint64_t consumed(const sim::FlowTotals& t) {
    return t.of(GoodsFlow::Residents) + t.of(GoodsFlow::Workshops) + t.of(GoodsFlow::Military) +
           t.of(GoodsFlow::Construction);
}
/// What left the cities: sold, own ships, lost to a full warehouse.
std::uint64_t exported(const sim::FlowTotals& t) {
    return t.of(GoodsFlow::Sold) + t.of(GoodsFlow::ShippedOut) + t.of(GoodsFlow::Lost);
}

std::uint64_t tons(std::uint64_t units) {
    return units / assets::kStockUnitsPerTon;
}

/// The flows of a good's page, with their texts.
struct FlowLine {
    GoodsFlow flow;
    std::string_view label;
};
constexpr std::array<FlowLine, 4> kInflows = {{
    {GoodsFlow::Produced, "stats.flow.produced"},
    {GoodsFlow::Bought, "stats.flow.bought"},
    {GoodsFlow::ShippedIn, "stats.flow.shipped_in"},
    {GoodsFlow::Returned, "stats.flow.returned"},
}};
constexpr std::array<FlowLine, 7> kOutflows = {{
    {GoodsFlow::Residents, "stats.flow.residents"},
    {GoodsFlow::Workshops, "stats.flow.workshops"},
    {GoodsFlow::Military, "stats.flow.military"},
    {GoodsFlow::Construction, "stats.flow.construction"},
    {GoodsFlow::Sold, "stats.flow.sold"},
    {GoodsFlow::ShippedOut, "stats.flow.shipped_out"},
    {GoodsFlow::Lost, "stats.flow.lost"},
}};

/// Draw items in design units, turned into pixels of the form's scale.
class Painter {
public:
    Painter(std::vector<ui::DrawItem>& out, double scale, const std::array<const ui::Font*, 3>& fonts)
        : m_out(out), m_scale(scale), m_fonts(fonts) {}

    void fill(Rect r, std::uint8_t colour) {
        if (r.w <= 0 || r.h <= 0) {
            return;
        }
        ui::DrawItem d;
        d.kind = ui::DrawItem::Kind::Fill;
        d.x = px(r.x);
        d.y = px(r.y);
        d.w = std::max(1, px(r.x + r.w) - d.x);
        d.h = std::max(1, px(r.y + r.h) - d.y);
        d.colour = colour;
        m_out.push_back(d);
    }
    /// Text in the box, vertically centred and cut to its width.
    void text(Rect r, std::string t, Align align, std::int32_t colour, std::int32_t font = kFontNormal) {
        const ui::Font* f = m_fonts[static_cast<std::size_t>(font)];
        const double height = f != nullptr ? f->height() / f->design_scale() : 18.0;
        ui::DrawItem d;
        d.kind = ui::DrawItem::Kind::Text;
        d.block = font;
        d.colour = colour;
        d.align = align;
        d.text = ui::fit_text(f, std::move(t), std::max(r.w, 0), false);
        const std::int32_t x = align == Align::Left ? r.x : align == Align::Right ? r.x + r.w : r.x + r.w / 2;
        d.x = px(x);
        d.y = px(r.y + static_cast<std::int32_t>((r.h + height) / 2));
        m_out.push_back(std::move(d));
    }
    /// Text wrapped into lines `line` apart, as many as fit the box.
    void paragraph(Rect r, const std::string& t, std::int32_t colour, std::int32_t font, std::int32_t line) {
        const ui::Font* f = m_fonts[static_cast<std::size_t>(font)];
        const std::vector<std::string> lines =
            f != nullptr ? ui::wrap_text(*f, t, static_cast<std::uint32_t>(std::max(0.0, r.w * f->design_scale())))
                         : std::vector<std::string>{t};
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const std::int32_t y = r.y + static_cast<std::int32_t>(i) * line;
            if (y + line > r.y + r.h) {
                break;
            }
            text(Rect{r.x, y, r.w, line}, lines[i], Align::Left, colour, font);
        }
    }
    /// A sprite of TOOLS fitted into the box.
    void sprite(Rect r, std::int32_t gfx) {
        ui::DrawItem d;
        d.kind = ui::DrawItem::Kind::Sprite;
        d.block = 0;
        d.gfx = gfx;
        d.x = px(r.x);
        d.y = px(r.y);
        d.w = px(r.x + r.w) - d.x;
        d.h = px(r.y + r.h) - d.y;
        m_out.push_back(d);
    }

private:
    [[nodiscard]] std::int32_t px(std::int32_t v) const { return ui::to_pixels(v, m_scale); }

    std::vector<ui::DrawItem>& m_out;
    double m_scale;
    const std::array<const ui::Font*, 3>& m_fonts;
};

}  // namespace

GoodsWindow::GoodsWindow(GoodsStyle style, std::vector<std::string> ware_names, std::int32_t ware_icon)
    : m_style(style), m_ware_names(std::move(ware_names)), m_ware_icon(ware_icon), m_form(style.form) {}

std::string GoodsWindow::rate_text(std::int64_t units, std::uint32_t minutes, bool sign) {
    if (minutes == 0) {
        return "-";
    }
    // Tenths of a ton per minute, rounded half away from zero.
    const std::int64_t div = static_cast<std::int64_t>(assets::kStockUnitsPerTon) * minutes;
    const std::int64_t tenths = (std::llabs(units) * 10 + div / 2) / div;
    if (tenths == 0) {
        return "0";
    }
    std::string s = units < 0 ? "-" : (sign ? "+" : "");
    s += std::to_string(tenths / 10);
    s += ui::tr("stats.decimal_point");
    s += std::to_string(tenths % 10);
    return s;
}

std::string GoodsWindow::lasts_text(std::optional<std::uint32_t> minutes) {
    if (!minutes) {
        return {};
    }
    if (*minutes < 100) {
        return ui::tr("stats.minutes", {std::to_string(*minutes)});
    }
    return ui::tr("stats.hours", {std::to_string(*minutes / 60)});
}

std::string GoodsWindow::ware_name(std::uint8_t ware) const {
    return ware < m_ware_names.size() ? m_ware_names[ware] : std::to_string(ware);
}

void GoodsWindow::wheel(std::int32_t notches) noexcept {
    if (m_detail) {
        return;
    }
    const std::size_t last = m_row_count > kRows ? m_row_count - kRows : 0;
    const auto step = static_cast<std::size_t>(std::abs(notches)) * 3;
    m_scroll = notches > 0 ? (m_scroll > step ? m_scroll - step : 0) : std::min(last, m_scroll + step);
}

std::vector<ui::DrawItem> GoodsWindow::draw(const sim::World& world, const assets::BuildingTable& buildings,
                                            sim::PlayerId player, const std::array<const ui::Font*, 3>& fonts) {
    m_islands = sim::goods_islands(world, player);
    if (m_island && std::ranges::find(m_islands, *m_island) == m_islands.end()) {
        m_island.reset();  // the city there was lost
    }
    m_form.clear();
    m_form.panel(Rect{0, 0, kWidth, kHeight});
    std::vector<ui::DrawItem> extra;
    layout_controls(world, player);
    if (m_detail) {
        layout_detail(world, buildings, player, *m_detail, fonts, extra);
    } else {
        layout_table(world, player, fonts, extra);
    }
    m_form.button(kClose, Rect{kWidth - kMargin - 160, kBottomY, 160, 38}, ui::tr("stats.close"));
    std::vector<ui::DrawItem> out = m_form.draw(fonts);
    out.insert(out.end(), std::make_move_iterator(extra.begin()), std::make_move_iterator(extra.end()));
    return out;
}

void GoodsWindow::layout_controls(const sim::World& world, sim::PlayerId player) {
    ui::Form& f = m_form;
    f.label(Rect{kMargin - 8, 12, kWidth - 2 * kMargin, 36}, ui::tr("stats.title"), Align::Left, Tone::Heading);
    std::string island = ui::tr("stats.all_islands", {std::to_string(m_islands.size())});
    if (m_island) {
        for (const sim::City& c : world.cities()) {
            if (c.owner == player && c.island == *m_island) {
                island = c.name;
                break;
            }
        }
    }
    f.cycle_button(kIsland, Rect{kMargin, kControlsY, 280, kControlH}, island, m_islands.size() > 1);
    f.cycle_button(kWindow, Rect{kMargin + 292, kControlsY, 210, kControlH},
                   ui::tr("stats.window", {std::to_string(minutes())}));
    if (m_detail) {
        f.button(kBack, Rect{kWidth - kMargin - 200, kControlsY, 200, kControlH}, ui::tr("stats.back"));
    } else {
        f.check(kAllGoods, Rect{kWidth - kMargin - 200, kControlsY, 200, kControlH}, ui::tr("stats.all_goods"),
                m_all_goods);
    }
}

void GoodsWindow::layout_table(const sim::World& world, sim::PlayerId player,
                               const std::array<const ui::Font*, 3>& fonts, std::vector<ui::DrawItem>& extra) {
    Painter paint(extra, scale(), fonts);
    const ui::FormStyle& st = m_style.form;
    const sim::GoodsScope scope{player, m_island};
    const std::uint32_t window = minutes();

    // The rows: group headings and the goods worth showing.
    struct Line {
        std::optional<std::uint8_t> ware;  // nothing: a heading
        std::string_view title;
        sim::FlowTotals totals;
        std::uint64_t stock = 0;
    };
    std::vector<Line> lines;
    std::uint32_t known = 0;  // minutes the figures cover (fewer at the start)
    for (const Group& g : groups()) {
        const std::size_t heading = lines.size();
        lines.push_back(Line{std::nullopt, g.title, {}, 0});
        for (const std::uint8_t w : g.wares) {
            Line l{w, {}, sim::goods_totals(world, scope, w, window), sim::goods_stock(world, scope, w)};
            known = std::max(known, l.totals.minutes);
            if (m_all_goods || l.stock != 0 || l.totals.in() != 0 || l.totals.out() != 0) {
                lines.push_back(l);
            }
        }
        if (lines.size() == heading + 1) {
            lines.pop_back();  // nothing of the group
        }
    }
    m_row_count = lines.size();
    m_scroll = std::min(m_scroll, m_row_count > kRows ? m_row_count - kRows : 0);

    // The heading of the table.
    paint.text(Rect{kNameX, kHeaderY, kColumns[0].right - 70 - kNameX, 22}, ui::tr("stats.col.good"), Align::Left,
               st.dim, kFontSmall);
    for (const Column& c : kColumns) {
        paint.text(Rect{c.right - 70, kHeaderY, 70, 22}, ui::tr(c.heading), Align::Right, st.dim, kFontSmall);
    }
    paint.text(Rect{kSparkX, kHeaderY, kSparkW, 22}, ui::tr("stats.col.trend"), Align::Centre, st.dim, kFontSmall);
    paint.fill(Rect{kMargin, kTableY - 2, kWidth - 2 * kMargin, 1}, st.frame);

    if (lines.empty()) {
        paint.text(Rect{kMargin, kTableY + 20, kWidth - 2 * kMargin, 30}, ui::tr("stats.empty"), Align::Centre, st.dim);
    }
    for (std::size_t i = 0; i < kRows && m_scroll + i < lines.size(); ++i) {
        const Line& l = lines[m_scroll + i];
        const std::int32_t y = kTableY + static_cast<std::int32_t>(i) * kRowH;
        if (!l.ware) {
            paint.text(Rect{kMargin, y, 400, kRowH}, ui::tr(l.title), Align::Left, st.heading);
            continue;
        }
        const std::uint8_t w = *l.ware;
        m_form.row(kRow + w, Rect{kMargin, y, kScrollX - 6 - kMargin, kRowH}, false);
        paint.sprite(Rect{kMargin + 2, y + 1, 22, kRowH - 2}, m_ware_icon + w);
        const bool quiet = l.stock == 0 && l.totals.in() == 0 && l.totals.out() == 0;
        paint.text(Rect{kNameX, y, kColumns[0].right - 70 - kNameX, kRowH}, ware_name(w), Align::Left,
                   quiet ? st.dim : st.text);
        const sim::FlowTotals& t = l.totals;
        const std::uint32_t m = std::max<std::uint32_t>(t.minutes, 1);
        const std::array<std::string, 5> values = {
            std::to_string(tons(l.stock)),
            rate_text(static_cast<std::int64_t>(t.of(GoodsFlow::Produced)), m),
            rate_text(static_cast<std::int64_t>(imported(t)), m),
            rate_text(static_cast<std::int64_t>(consumed(t)), m),
            rate_text(static_cast<std::int64_t>(exported(t)), m),
        };
        for (std::size_t c = 0; c < values.size(); ++c) {
            paint.text(Rect{kColumns[c].right - 70, y, 70, kRowH}, values[c], Align::Right, quiet ? st.dim : st.text);
        }
        const std::int64_t net = t.net();
        paint.text(Rect{kColumns[5].right - 70, y, 70, kRowH}, rate_text(net, m, true), Align::Right,
                   net > 0   ? ui::kTextGreen
                   : net < 0 ? st.warning
                             : st.dim);
        const std::optional<std::uint32_t> left = sim::minutes_left(l.stock, t);
        paint.text(Rect{kColumns[6].right - 66, y, 66, kRowH},
                   left && l.stock == 0 ? ui::tr("stats.gone") : lasts_text(left), Align::Right,
                   left && *left < 10 ? st.warning : st.text);
        // The stock over the last 46 minutes, a column each.
        const std::vector<sim::GoodsSample> history = sim::goods_history(world, scope, w, false);
        const std::size_t n = std::min<std::size_t>(history.size(), kSparkW);
        std::uint64_t top = 1;
        for (std::size_t k = history.size() - n; k < history.size(); ++k) {
            top = std::max(top, history[k].stock);
        }
        const std::int32_t base = y + kRowH - 4;
        const std::int32_t height = kRowH - 8;
        paint.fill(Rect{kSparkX, base, kSparkW, 1}, m_style.chart);
        for (std::size_t k = 0; k < n; ++k) {
            const sim::GoodsSample& s = history[history.size() - n + k];
            const auto h = static_cast<std::int32_t>(s.stock * static_cast<std::uint64_t>(height) / top);
            const std::int32_t x = kSparkX + kSparkW - static_cast<std::int32_t>(n) + static_cast<std::int32_t>(k);
            paint.fill(Rect{x, base - h, 1, h}, s.in() >= s.out() ? m_style.stock : m_style.out);
        }
    }
    // The scroll bar: a click above or below the thumb turns a page.
    if (m_row_count > kRows) {
        const std::int32_t track = static_cast<std::int32_t>(kRows) * kRowH;
        const auto thumb_h = std::max<std::int32_t>(
            20, track * static_cast<std::int32_t>(kRows) / static_cast<std::int32_t>(m_row_count));
        const auto thumb_y = kTableY + (track - thumb_h) * static_cast<std::int32_t>(m_scroll) /
                                           static_cast<std::int32_t>(m_row_count - kRows);
        m_form.row(kPageUp, Rect{kScrollX - 4, kTableY, kScrollW + 8, thumb_y - kTableY}, false);
        m_form.row(kPageDown, Rect{kScrollX - 4, thumb_y + thumb_h, kScrollW + 8, kTableY + track - thumb_y - thumb_h},
                   false);
        paint.fill(Rect{kScrollX, kTableY, kScrollW, track}, m_style.chart);
        paint.fill(Rect{kScrollX, thumb_y, kScrollW, thumb_h}, st.frame);
    }
    // Below the table: what the figures mean.
    const std::string hint = known < window && known != 0
                                 ? ui::tr("stats.hint_short", {std::to_string(known), std::to_string(window)})
                                 : ui::tr("stats.hint", {std::to_string(window)});
    paint.paragraph(Rect{kMargin, kBottomY, kWidth - 2 * kMargin - 180, 40}, hint, st.dim, kFontSmall, 19);
}

void GoodsWindow::layout_detail(const sim::World& world, const assets::BuildingTable& buildings, sim::PlayerId player,
                                std::uint8_t ware, const std::array<const ui::Font*, 3>& fonts,
                                std::vector<ui::DrawItem>& extra) {
    Painter paint(extra, scale(), fonts);
    const ui::FormStyle& st = m_style.form;
    const sim::GoodsScope scope{player, m_island};
    const sim::FlowTotals t = sim::goods_totals(world, scope, ware, minutes());
    const std::uint32_t m = std::max<std::uint32_t>(t.minutes, 1);
    const std::uint64_t stock = sim::goods_stock(world, scope, ware);
    const std::uint64_t room = sim::goods_capacity(world, scope);

    // The good, its stock, the supply of the inhabitants and the buildings making it.
    paint.sprite(Rect{kMargin, 98, 48, 46}, m_ware_icon + ware);
    paint.text(Rect{kMargin + 60, 96, 300, 26}, ware_name(ware), Align::Left, st.heading);
    paint.text(Rect{kMargin + 60, 120, 340, 24},
               ui::tr("stats.stock", {std::to_string(tons(stock)), std::to_string(tons(room))}), Align::Left, st.text,
               kFontSmall);
    std::int32_t y = 150;
    if (const std::uint64_t demand = t.of(GoodsFlow::Demand); demand != 0) {
        const std::uint64_t percent = std::min<std::uint64_t>(100, t.of(GoodsFlow::Residents) * 100 / demand);
        paint.text(Rect{kMargin, y, 340, 22}, ui::tr("stats.supply", {std::to_string(percent)}), Align::Left,
                   percent < 90 ? st.warning : st.text, kFontSmall);
        y += 22;
    }
    if (const sim::GoodsProducers p = sim::goods_producers(world, buildings, scope, ware); p.total != 0) {
        const std::string text = p.off != 0
                                     ? ui::tr("stats.producers_off", {std::to_string(p.total), std::to_string(p.off)})
                                     : ui::tr("stats.producers", {std::to_string(p.total)});
        paint.text(Rect{kMargin, y, 340, 22}, text, Align::Left, st.text, kFontSmall);
    }

    // Where it came from and went to, per minute, with bars.
    std::uint64_t top = 1;
    for (const FlowLine& f : kInflows) {
        top = std::max(top, t.of(f.flow));
    }
    for (const FlowLine& f : kOutflows) {
        top = std::max(top, t.of(f.flow));
    }
    constexpr std::int32_t kLabelW = 150;
    constexpr std::int32_t kValueRight = kMargin + kLabelW + 60;
    constexpr std::int32_t kBarX = kValueRight + 8;
    constexpr std::int32_t kBarW = 110;
    constexpr std::int32_t kLineH = 21;
    y = 200;
    const auto flow_line = [&](const FlowLine& f, std::uint8_t colour) {
        const std::uint64_t v = t.of(f.flow);
        const std::int32_t tone = v == 0 ? st.dim : st.text;
        paint.text(Rect{kMargin + 8, y, kLabelW, kLineH}, ui::tr(f.label), Align::Left, tone, kFontSmall);
        paint.text(Rect{kValueRight - 60, y, 60, kLineH}, rate_text(static_cast<std::int64_t>(v), m), Align::Right,
                   tone, kFontSmall);
        paint.fill(Rect{kBarX, y + 6, static_cast<std::int32_t>(v * kBarW / top), kLineH - 12}, colour);
        y += kLineH;
    };
    paint.text(Rect{kMargin, y, 300, kLineH}, ui::tr("stats.inflow"), Align::Left, st.heading, kFontSmall);
    y += kLineH;
    for (const FlowLine& f : kInflows) {
        flow_line(f, m_style.in);
    }
    y += 6;
    paint.text(Rect{kMargin, y, 300, kLineH}, ui::tr("stats.outflow"), Align::Left, st.heading, kFontSmall);
    y += kLineH;
    for (const FlowLine& f : kOutflows) {
        flow_line(f, m_style.out);
    }
    y += 6;
    paint.fill(Rect{kMargin, y, kBarX + kBarW - kMargin, 1}, st.frame);
    const std::int64_t net = t.net();
    paint.text(Rect{kMargin + 8, y + 2, kLabelW, kLineH + 2}, ui::tr("stats.col.balance"), Align::Left, st.text);
    paint.text(Rect{kValueRight - 80, y + 2, 80, kLineH + 2}, rate_text(net, m, true), Align::Right,
               net > 0   ? ui::kTextGreen
               : net < 0 ? st.warning
                         : st.dim);
    const std::optional<std::uint32_t> lasts = sim::minutes_left(stock, t);
    paint.text(Rect{kBarX, y + 2, kBarW + 20, kLineH + 2},
               lasts && stock == 0 ? ui::tr("stats.gone") : lasts_text(lasts), Align::Left, st.text, kFontSmall);
    paint.paragraph(Rect{kMargin, kBottomY, 380, 40}, ui::tr("stats.unit", {std::to_string(minutes())}), st.dim,
                    kFontSmall, 19);

    // The chart: in (green, up) and out (red, down) per sample, the stock as a line of dots.
    constexpr std::int32_t kChartX = 392;
    constexpr std::int32_t kChartY = 196;
    constexpr std::int32_t kChartW = GoodsWindow::kWidth - kMargin - kChartX;
    constexpr std::int32_t kChartH = 270;
    m_form.button(kChart, Rect{kChartX, 150, kChartW, 32},
                  ui::tr(m_long_chart ? "stats.chart_12h" : "stats.chart_60min"));
    paint.fill(Rect{kChartX, kChartY, kChartW, kChartH}, st.frame);
    paint.fill(Rect{kChartX + 1, kChartY + 1, kChartW - 2, kChartH - 2}, m_style.chart);
    const std::vector<sim::GoodsSample> history = sim::goods_history(world, scope, ware, m_long_chart);
    const std::size_t slots = m_long_chart ? sim::GoodsLedger::kQuarters : sim::GoodsLedger::kMinutes;
    const std::int32_t inner = kChartW - 8;
    const std::int32_t slot_w = std::max<std::int32_t>(1, inner / static_cast<std::int32_t>(slots));
    const std::int32_t left = kChartX + 4 + (inner - slot_w * static_cast<std::int32_t>(slots));
    const std::int32_t mid = kChartY + kChartH / 2;
    const std::int32_t half = kChartH / 2 - 6;
    std::uint64_t flow_top = 1;
    std::uint64_t stock_top = 1;
    for (const sim::GoodsSample& s : history) {
        flow_top = std::max({flow_top, s.in(), s.out()});
        stock_top = std::max(stock_top, s.stock);
    }
    paint.fill(Rect{kChartX + 2, mid, kChartW - 4, 1}, st.frame);
    const std::size_t first = slots - std::min(slots, history.size());
    for (std::size_t k = 0; k < history.size() && k < slots; ++k) {
        const sim::GoodsSample& s = history[history.size() - std::min(slots, history.size()) + k];
        const std::int32_t x = left + static_cast<std::int32_t>(first + k) * slot_w;
        const std::int32_t bar = std::max(1, slot_w - 1);
        const auto up = static_cast<std::int32_t>(s.in() * static_cast<std::uint64_t>(half) / flow_top);
        const auto down = static_cast<std::int32_t>(s.out() * static_cast<std::uint64_t>(half) / flow_top);
        paint.fill(Rect{x, mid - up, bar, up}, m_style.in);
        paint.fill(Rect{x, mid + 1, bar, down}, m_style.out);
        const auto level = static_cast<std::int32_t>(s.stock * static_cast<std::uint64_t>(kChartH - 12) / stock_top);
        paint.fill(Rect{x, kChartY + kChartH - 6 - level - 1, std::max(2, slot_w), 3}, m_style.stock);
    }
    if (history.empty()) {
        paint.text(Rect{kChartX, mid - 30, kChartW, 24}, ui::tr("stats.no_data"), Align::Centre, st.dim, kFontSmall);
    }
    // Below it: the time axis and the scale.
    const std::int32_t below = kChartY + kChartH + 2;
    paint.text(Rect{kChartX, below, 120, 20}, ui::tr(m_long_chart ? "stats.axis_12h" : "stats.axis_60min"), Align::Left,
               st.dim, kFontSmall);
    paint.text(Rect{kChartX + kChartW - 120, below, 120, 20}, ui::tr("stats.axis_now"), Align::Right, st.dim,
               kFontSmall);
    // A sample is a minute, or the average minute of a quarter of an hour.
    paint.text(Rect{kChartX, below + 20, kChartW, 20},
               ui::tr("stats.chart_scale",
                      {rate_text(static_cast<std::int64_t>(flow_top), 1), std::to_string(tons(stock_top))}),
               Align::Left, st.dim, kFontSmall);
}

GoodsWindow::Result GoodsWindow::mouse(std::int32_t x, std::int32_t y, bool down, ui::MouseButton button) {
    const auto id = m_form.mouse(x, y, down, button);
    if (!id) {
        return Result::None;
    }
    const bool forward = m_form.clicked_with() == ui::MouseButton::Left;
    switch (*id) {
        case kClose:
            return Result::Close;
        case kBack:
            m_detail.reset();
            break;
        case kIsland: {
            // All islands, then each of them in turn.
            const std::size_t count = m_islands.size() + 1;
            std::size_t at = 0;
            if (m_island) {
                at = static_cast<std::size_t>(std::ranges::find(m_islands, *m_island) - m_islands.begin()) + 1;
            }
            at = forward ? (at + 1) % count : (at + count - 1) % count;
            m_island = at == 0 ? std::nullopt : std::optional<std::uint16_t>(m_islands[at - 1]);
            m_scroll = 0;
            break;
        }
        case kWindow:
            m_window = forward ? (m_window + 1) % kWindows.size() : (m_window + kWindows.size() - 1) % kWindows.size();
            break;
        case kAllGoods:
            m_all_goods = !m_all_goods;
            break;
        case kChart:
            m_long_chart = !m_long_chart;
            break;
        case kPageUp:
            m_scroll = m_scroll > kRows ? m_scroll - kRows : 0;
            break;
        case kPageDown:
            m_scroll += kRows;  // clamped when laid out
            break;
        default:
            if (*id >= kRow && *id < kRow + 256) {
                m_detail = static_cast<std::uint8_t>(*id - kRow);
            }
            break;
    }
    return Result::None;
}

}  // namespace o1602::app
