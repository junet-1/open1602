// SPDX-License-Identifier: GPL-3.0-or-later
#include "selection_panels.hpp"

#include <algorithm>
#include <cmath>

#include "o1602/sim/consumption.hpp"
#include "o1602/sim/houses.hpp"
#include "o1602/sim/mission.hpp"
#include "o1602/sim/ships.hpp"
#include "o1602/ui/build_menu.hpp"
#include "o1602/ui/text_format.hpp"

namespace o1602::app {

std::string PanelContext::text(std::string_view section, std::size_t line) const {
    if (texts == nullptr) {
        return {};
    }
    const auto& lines = texts->section(section);
    return line < lines.size() ? lines[line] : std::string();
}

namespace {

// LEISTE lines.
constexpr std::size_t kHelpCityName = 11;
constexpr std::size_t kHelpProduct = 146;
constexpr std::size_t kHelpStop = 147;    // + 1: switch on again
constexpr std::size_t kHelpPickup = 149;  // + 1: allow again
// [GAME] lines.
constexpr std::size_t kGameUtilisation = 11;
constexpr std::size_t kGameRunningCost = 18;
constexpr std::size_t kGameStock = 21;
constexpr std::size_t kGamePercent = 48;
constexpr std::size_t kGameTons = 49;

std::string help_line(const PanelContext& c, std::size_t line) {
    return c.text("LEISTE", line);
}

/// The building at a selection, nothing if it is gone.
const assets::BuildingType* building_at(const Selection& s, const PanelContext& c) {
    if (s.kind != Selection::Kind::Building || s.island >= c.world().islands().size()) {
        return nullptr;
    }
    const sim::IslandMap& map = c.world().islands()[s.island].map;
    if (!map.contains(s.x, s.y)) {
        return nullptr;
    }
    const sim::MapCell& top = map.top(s.x, s.y);
    return top.empty() ? nullptr : c.buildings.by_sprite(top.sprite);
}

const sim::City* city_at(const Selection& s, const PanelContext& c) {
    if (s.island >= c.world().islands().size()) {
        return nullptr;
    }
    const sim::IslandMap& map = c.world().islands()[s.island].map;
    return map.contains(s.x, s.y) ? c.world().city(s.island, map.top(s.x, s.y).city) : nullptr;
}

/// The name of a building as the build menu calls it (Haus_GetMenuName 0x0042D230).
std::string building_name(const assets::BuildingType& b, const PanelContext& c) {
    const ui::BuildMenuDefinition& menu = ui::original_build_menu();
    if (const ui::BuildCategory* cat = menu.category_of(b.id)) {
        return c.text(cat->names, cat->index_of(b.id).value_or(0));
    }
    return {};
}

/// Productivity in percent (Prod_Utilisation 0x0047D290): goods made in the statistics window
/// against what the building makes at full speed, for farms, hunters and fishers with the walk of
/// their worker.
std::int64_t utilisation(const sim::Production& p, const assets::BuildingType& b, const PanelContext& c) {
    const std::int64_t window = p.elapsed;
    std::int64_t n = 0;
    switch (b.prod_kind) {
        case 1:  // HANDWERK
        case 4:  // WEIDETIER
            n = b.interval != 0 ? window * 32 / b.interval : 0;
            break;
        case 2:    // PLANTAGE
        case 5:    // JAGDHAUS
        case 6: {  // FISCHEREI
            const double reach = b.radius <= 1 ? 2.2 : b.radius == 2 ? 3.0 : b.radius == 3 ? 4.0 : 5.0;
            const assets::FigureType* worker = c.figures.get(b.worker_figure);
            const double speed = worker != nullptr ? worker->speed * 0.0001 : 0.0;
            const double work = worker != nullptr ? worker->work_time_ms / 1000.0 : 0.0;
            const auto walk = speed > 0.0 ? static_cast<std::int64_t>(reach / (speed * 50.0) + work) : 0;
            const std::int64_t cycle = ((walk * b.raw_amount) >> 5) + b.interval;
            n = cycle != 0 ? window * 32 / cycle : 0;
            break;
        }
        default:
            return 100;
    }
    return n == 0 ? 100 : std::min<std::int64_t>(100, std::int64_t{p.produced} * 100 / n);
}

/// What remains of a raw material beside the share the running cycle already took (0x0047D540).
std::int64_t free_stock(std::uint16_t stock, std::uint32_t per_cycle, std::uint8_t rate) {
    const std::int64_t used = (std::int64_t{per_cycle} * rate) >> 7;
    return std::max<std::int64_t>(0, std::int64_t{stock} - used);
}

// ---- Production buildings ------------------------------------------------------------------------

/// Gadget Ids of the three production windows (PROD, PLANTAGE, BERGWERK); 0 = not in this one.
struct ProductionIds {
    std::int32_t title = 0;
    std::int32_t name = 0;
    std::int32_t util_label = 0;
    std::int32_t util_value = 0;
    std::int32_t cost_label = 0;
    std::int32_t cost_value = 0;
    std::int32_t stop = 0;
    std::int32_t pickup = 0;
    // One raw material: icon, amount, frame, arrow; the product: icon, amount, frame.
    std::array<std::int32_t, 4> raw{};
    std::array<std::int32_t, 3> product{};
    // Two raw materials (PROD only): second icon, amount, frame, "+"; first icon, amount, frame,
    // arrow; product icon, amount, frame.
    std::array<std::int32_t, 4> raw2_b{};
    std::array<std::int32_t, 4> raw_b{};
    std::array<std::int32_t, 3> product_b{};
};

constexpr ProductionIds kProd{37002,
                              37008,
                              37003,
                              37004,
                              37005,
                              37006,
                              37009,
                              37010,
                              {37021, 37022, 37023, 37024},
                              {37025, 37026, 37027},
                              {37031, 37032, 37033, 37034},
                              {37035, 37036, 37037, 37038},
                              {37039, 37040, 37041}};
constexpr ProductionIds kPlantage{41002, 41008, 41003, 41004, 41005, 41006, 41009, 41010, {}, {41021, 41022, 41023}};
constexpr ProductionIds kMine{46002, 46006, 0, 0, 46003, 46004, 46007, 46008, {}, {46021, 46022, 46023}};

class ProductionPanel final : public SimplePanel {
public:
    ProductionPanel(assets::GadgetLayout layout, const ProductionIds& ids, std::int32_t gfx_ware)
        : SimplePanel(std::move(layout)), m_ids(ids), m_gfx_ware(gfx_ware) {}

    void refresh(const Selection& s, const PanelContext& c) override {
        const assets::BuildingType* b = building_at(s, c);
        const sim::Production* p = c.world().production(s.island, s.x, s.y);
        if (b == nullptr || p == nullptr) {
            return;
        }
        const sim::City* city = city_at(s, c);
        m_panel.set_text(m_ids.title, city != nullptr ? city->name : std::string());
        m_panel.set_text(m_ids.name, building_name(*b, c));
        m_panel.set_text(m_ids.util_label, c.text("GAME", kGameUtilisation));
        m_panel.set_text(m_ids.util_value, ui::format_text(c.text("GAME", kGamePercent), {utilisation(*p, *b, c)}));
        m_panel.set_text(m_ids.cost_label, c.text("GAME", kGameRunningCost));
        m_panel.set_text(m_ids.cost_value, std::to_string(p->enabled ? b->running_cost : b->idle_cost));
        m_panel.set_selected(m_ids.stop, !p->enabled);
        m_panel.set_selected(m_ids.pickup, p->no_pickup());

        const std::string tons = c.text("GAME", kGameTons);
        const auto show = [&](const auto& group, bool on) {
            for (const std::int32_t id : group) {
                m_panel.set_visible(id, on);
            }
        };
        const auto ware = [&](std::int32_t icon, std::int32_t amount, std::uint8_t w, std::int64_t stock) {
            m_panel.set_graphic(icon, m_gfx_ware + w, 0);
            m_panel.set_text(amount, ui::format_text(tons, {stock >> 5}));
        };
        const bool two = b->raw2_ware != 0 && b->raw2_amount != 0 && m_ids.raw2_b[0] != 0;
        const bool raw = b->raw_ware != 0 && m_ids.raw[0] != 0;
        show(m_ids.raw, raw && !two);
        show(m_ids.product, !two);
        show(m_ids.raw2_b, two);
        show(m_ids.raw_b, two);
        show(m_ids.product_b, two);
        const std::int64_t raw_free = free_stock(p->raw, b->raw_amount, p->rate);
        const std::int64_t raw2_free = free_stock(p->raw2, b->raw2_amount, p->rate);
        if (two) {
            ware(m_ids.raw2_b[0], m_ids.raw2_b[1], b->raw2_ware, raw2_free);
            ware(m_ids.raw_b[0], m_ids.raw_b[1], b->raw_ware, raw_free);
            ware(m_ids.product_b[0], m_ids.product_b[1], b->ware, p->stock);
        } else {
            if (raw) {
                ware(m_ids.raw[0], m_ids.raw[1], b->raw_ware, raw_free);
            }
            ware(m_ids.product[0], m_ids.product[1], b->ware, p->stock);
        }
    }

    void clicked(const ui::PanelEvent& e, const Selection& s, const PanelContext& c,
                 std::vector<sim::Command>& out) override {
        if (e.kind != ui::PanelEvent::Kind::Clicked) {
            return;
        }
        const std::int32_t id = e.id;
        const sim::Production* p = c.world().production(s.island, s.x, s.y);
        if (p == nullptr) {
            return;
        }
        sim::ProductionSwitch sw;
        sw.player = c.local;
        sw.island = s.island;
        sw.x = s.x;
        sw.y = s.y;
        if (id == m_ids.stop) {
            sw.op = sim::ProductionSwitch::Op::Enable;
            sw.on = !p->enabled;
            out.emplace_back(sw);
        } else if (id == m_ids.pickup) {
            sw.op = sim::ProductionSwitch::Op::Pickup;
            sw.on = p->no_pickup();
            out.emplace_back(sw);
        }
    }

    [[nodiscard]] std::optional<std::string> help(std::int32_t id, const Selection& s,
                                                  const PanelContext& c) const override {
        const sim::Production* p = c.world().production(s.island, s.x, s.y);
        const assets::BuildingType* b = building_at(s, c);
        if (p == nullptr || b == nullptr) {
            return std::nullopt;
        }
        if (id == m_ids.title) {
            return help_line(c, kHelpCityName);
        }
        if (id == m_ids.stop) {
            return help_line(c, kHelpStop + (p->enabled ? 0 : 1));
        }
        if (id == m_ids.pickup) {
            return help_line(c, kHelpPickup + (p->no_pickup() ? 1 : 0));
        }
        if (id == m_ids.product[2] || id == m_ids.product_b[2]) {
            return help_line(c, kHelpProduct);
        }
        // An input frame: "Wollevorrat (reicht für weitere 1.5t Stoffe)" - whole tons in the
        // building against the input of one ton of product.
        const auto stock_text = [&](std::uint8_t in, std::uint16_t stock, std::uint32_t per_cycle) {
            const std::int64_t free = free_stock(stock, per_cycle, p->rate);
            const double enough =
                per_cycle != 0 ? static_cast<double>(free - free % 32) / static_cast<double>(per_cycle) : 0.0;
            return ui::format_text(c.text("GAME", kGameStock), {std::string_view(c.text("WARE", in)), enough,
                                                                std::string_view(c.text("WARE", b->ware))});
        };
        if (id == m_ids.raw[2] || id == m_ids.raw_b[2]) {
            return stock_text(b->raw_ware, p->raw, b->raw_amount);
        }
        if (id == m_ids.raw2_b[2]) {
            return stock_text(b->raw2_ware, p->raw2, b->raw2_amount);
        }
        return std::nullopt;
    }

private:
    ProductionIds m_ids;
    std::int32_t m_gfx_ware;
};

// ---- Public buildings ----------------------------------------------------------------------------

class InfraPanel final : public SimplePanel {
public:
    InfraPanel(assets::GadgetLayout layout, std::int32_t gfx_bauhaus)
        : SimplePanel(std::move(layout)), m_gfx_bauhaus(gfx_bauhaus) {}

    void refresh(const Selection& s, const PanelContext& c) override {
        const assets::BuildingType* b = building_at(s, c);
        if (b == nullptr) {
            return;
        }
        const sim::City* city = city_at(s, c);
        m_panel.set_text(62002, city != nullptr ? city->name : std::string());
        m_panel.set_text(62006, building_name(*b, c));
        m_panel.set_text(62003, c.text("GAME", kGameRunningCost));
        m_panel.set_text(62004, std::to_string(b->running_cost));
        // The building's icon of the build menu (Haus_GetMenuIcon 0x0042D3A0).
        const ui::BuildMenuDefinition& menu = ui::original_build_menu();
        if (const ui::BuildCategory* cat = menu.category_of(b->id)) {
            const auto index = static_cast<std::int32_t>(cat->index_of(b->id).value_or(0));
            m_panel.set_graphic(62021, m_gfx_bauhaus + cat->icon_offset + index, 0);
            m_panel.set_visible(62021, true);
        } else {
            m_panel.set_visible(62021, false);
        }
    }
    void clicked(const ui::PanelEvent&, const Selection&, const PanelContext&, std::vector<sim::Command>&) override {}
    [[nodiscard]] std::optional<std::string> help(std::int32_t id, const Selection&,
                                                  const PanelContext& c) const override {
        return id == 62002 ? std::optional<std::string>(help_line(c, kHelpCityName)) : std::nullopt;
    }

private:
    std::int32_t m_gfx_bauhaus;
};

// ---- Own Kontor or market: city, trade lists, stock -----------------------------------------------

namespace city_ids {
// STADT.GAD
constexpr std::int32_t kTitle = 43002;
constexpr std::int32_t kSell = 43003;
constexpr std::int32_t kBuy = 43004;
constexpr std::int32_t kKontorPicture = 43005;
constexpr std::int32_t kStock = 43006;
constexpr std::int32_t kMaterialLock = 43025;
constexpr std::int32_t kTrader = 43026;
constexpr std::int32_t kBalanceIcon = 43024;
// KONTOR.GAD
constexpr std::int32_t kKTitle = 34002;
constexpr std::int32_t kKMode = 34003;
constexpr std::int32_t kKLimit = 34005;
constexpr std::int32_t kKStockBar = 34007;
constexpr std::int32_t kKLimitMarker = 34008;
constexpr std::int32_t kKName = 34012;
constexpr std::int32_t kKPrice = 34013;
constexpr std::int32_t kKStockLabel = 34014;
constexpr std::int32_t kKStockValue = 34015;
constexpr std::int32_t kKStop = 34016;
constexpr std::int32_t kKPriceTrack = 34021;
constexpr std::int32_t kKPriceDown = 34022;
constexpr std::int32_t kKPriceUp = 34023;
constexpr std::int32_t kKPriceSlider = 34024;
constexpr std::int32_t kKIcon = 34031;   // + slot, 11
constexpr std::int32_t kKBar = 34051;    // + slot
constexpr std::int32_t kKFrame = 34071;  // + slot
constexpr std::int32_t kKSlots = 11;
// LAGER.GAD: + ware number
constexpr std::int32_t kLIcon = 44019;
constexpr std::int32_t kLBar = 44049;
constexpr std::int32_t kLFrame = 44079;
constexpr std::int32_t kLMarker = 44109;
constexpr std::int32_t kLCorner = 44139;
constexpr std::int32_t kLDeadSlider = 44171;
}  // namespace city_ids

constexpr std::uint8_t kFirstTradeWare = 2;
constexpr std::uint8_t kLastTradeWare = 24;
constexpr std::int32_t kPriceSteps = 1024;

/// STADT (the city), KONTOR (the buy or sell list, with the LAGER layout as the ware picker) and
/// LAGER (the stock) (CityPanel_Refresh 0x00434640, KontorPanel_Refresh 0x00434B40,
/// StockPanel_Refresh 0x00434810).
class CityPanel final : public SelectionPanel {
public:
    CityPanel(assets::GadgetLayout stadt, assets::GadgetLayout kontor, assets::GadgetLayout lager)
        : m_stadt(std::move(stadt), 1.0), m_kontor(std::move(kontor), 1.0), m_lager(std::move(lager), 1.0) {
        const assets::GadgetLayout& k = m_kontor.layout();
        m_gfx_ware = k.constant("GFXWARE").value_or(192);
        m_gfx_empty = k.constant("GFXWARELEER").value_or(188) + 1;
        if (const assets::Gadget* g = m_stadt.layout().find(city_ids::kBalanceIcon)) {
            m_balance_icon = g->gfx;
        }
        if (const assets::Gadget* g = m_lager.layout().find(city_ids::kLCorner + 2)) {
            m_corner = g->gfx;
        }
        // No handler in the original: the Kontor picture. The free trader can not be called yet.
        m_stadt.set_disabled(city_ids::kKontorPicture, true);
        m_stadt.set_visible(city_ids::kTrader, false);
        m_lager.set_visible(city_ids::kLDeadSlider, false);
        m_lager.set_visible(44134, false);  // marker and corner of the 24th place: never shown
        m_lager.set_visible(44164, false);
    }

    void set_scale(double scale) override {
        m_stadt.set_scale(scale);
        m_kontor.set_scale(scale);
        m_lager.set_scale(scale);
    }
    ui::Panel& panel() override {
        switch (m_page) {
            case Page::Kontor:
                return m_picker ? m_lager : m_kontor;
            case Page::Stock:
                return m_lager;
            default:
                return m_stadt;
        }
    }

    void refresh(const Selection& s, const PanelContext& c) override {
        // A new selection starts with the city; the same Kontor clicked again shows the stock.
        Selection key = s;
        key.again = false;
        if (key != m_last) {
            m_last = key;
            m_page = s.again ? Page::Stock : Page::City;
            m_picker = false;
            m_slot = 0;
        } else if (s.again && !m_seen_again) {
            m_page = Page::Stock;
            m_picker = false;
        }
        m_seen_again = s.again;
        const sim::City* city = city_at(s, c);
        if (city == nullptr) {
            return;
        }
        switch (m_page) {
            case Page::City:
                refresh_city(*city, c);
                break;
            case Page::Kontor:
                if (m_picker) {
                    refresh_stock(*city, true);
                } else {
                    refresh_kontor(*city, c);
                }
                break;
            case Page::Stock:
                refresh_stock(*city, false);
                break;
        }
    }

    void clicked(const ui::PanelEvent& e, const Selection& s, const PanelContext& c,
                 std::vector<sim::Command>& out) override {
        const sim::City* city = city_at(s, c);
        if (city == nullptr) {
            return;
        }
        if (m_page == Page::City) {
            if (e.kind != ui::PanelEvent::Kind::Clicked) {
                return;
            }
            if (e.id == city_ids::kSell || e.id == city_ids::kBuy) {
                m_page = Page::Kontor;
                m_sell = e.id == city_ids::kSell;
                m_picker = false;
                m_slot = 0;
            } else if (e.id == city_ids::kStock) {
                m_page = Page::Stock;
            } else if (e.id == city_ids::kMaterialLock) {
                sim::CityOrder o;
                o.player = c.local;
                o.island = city->island;
                o.city = city->index;
                o.op = sim::CityOrder::Op::MaterialLock;
                o.on = !city->material_lock;
                out.emplace_back(o);
            }
            return;
        }
        if (m_page == Page::Kontor && m_picker) {
            // The ware picker: a ware goes into the list (and out of the other one), the empty
            // frame closes the picker.
            if (e.kind != ui::PanelEvent::Kind::Clicked) {
                return;
            }
            const std::int32_t w = e.id - city_ids::kLFrame;
            if (w >= kFirstTradeWare && w <= kLastTradeWare) {
                out.emplace_back(setting(*city, c, static_cast<std::uint8_t>(w), mode()));
                // The new ware's place in the sorted list.
                std::int32_t before = 0;
                for (std::uint8_t v = kFirstTradeWare; v < w; ++v) {
                    before += listed(*city, v) ? 1 : 0;
                }
                m_slot = before;
            }
            if (w >= kFirstTradeWare && w <= kLastTradeWare + 1) {
                m_picker = false;
            }
            return;
        }
        if (m_page != Page::Kontor) {
            return;  // the stock list only shows
        }
        const std::vector<std::uint8_t> list = wares(*city);
        const std::int32_t slots = slot_count(*city, c);
        if (e.kind == ui::PanelEvent::Kind::Clicked && e.id >= city_ids::kKFrame &&
            e.id < city_ids::kKFrame + city_ids::kKSlots) {
            m_slot = e.id - city_ids::kKFrame;
            if (m_slot >= static_cast<std::int32_t>(list.size()) && m_slot < slots) {
                m_picker = true;  // an empty slot: choose a ware
            }
            return;
        }
        if (m_slot >= static_cast<std::int32_t>(list.size())) {
            return;
        }
        const std::uint8_t w = list[static_cast<std::size_t>(m_slot)];
        const sim::City::Trade* t = city->trade_of(w);
        if (t == nullptr) {
            return;
        }
        const std::uint16_t base = sim::base_price(w);
        const std::uint16_t min = static_cast<std::uint16_t>(base * 300U >> 10U);
        const std::uint16_t price = m_sell ? t->sell_price : t->buy_price;
        sim::TradeSetting o = setting(*city, c, w, mode());
        if (e.kind == ui::PanelEvent::Kind::Clicked && e.id == city_ids::kKStop) {
            o.mode = sim::TradeSetting::Mode::None;
            out.emplace_back(o);
        } else if (e.kind == ui::PanelEvent::Kind::Clicked &&
                   (e.id == city_ids::kKPriceDown || e.id == city_ids::kKPriceUp)) {
            const int step = e.id == city_ids::kKPriceUp ? 1 : -1;
            o.price = static_cast<std::uint16_t>(std::clamp<int>(price + step, min, base));
            out.emplace_back(o);
        } else if (e.kind == ui::PanelEvent::Kind::Slid && e.id == city_ids::kKPriceSlider) {
            // ((v + 300) * base) >> 10 over v = 0..724 (0x0043D9xx); sent only when it changes.
            const std::int32_t v = m_kontor.slider(e.id, 0, 724);
            const auto p = static_cast<std::uint16_t>(((v + 300) * base) >> 10);
            if (p != price) {
                o.price = p;
                out.emplace_back(o);
            }
        } else if (e.kind == ui::PanelEvent::Kind::Slid && e.id == city_ids::kKLimit) {
            const auto cap = static_cast<std::int32_t>(city->capacity);
            const auto v = static_cast<std::uint32_t>(m_kontor.slider(e.id, 0, std::max(cap, 1)));
            if (v != (m_sell ? t->keep : t->limit)) {
                o.amount = v;
                out.emplace_back(o);
            }
        }
    }

    [[nodiscard]] std::optional<std::string> help(std::int32_t id, const Selection& s,
                                                  const PanelContext& c) const override {
        const sim::City* city = city_at(s, c);
        if (city == nullptr) {
            return std::nullopt;
        }
        const auto line = [&](std::size_t n) { return std::optional<std::string>(help_line(c, n)); };
        const auto stock_text = [&](std::uint8_t w) {
            const std::uint32_t t = city->stock(w) >> 5U;
            return t >= 1 ? ui::format_text(c.text("GAME", 50), {std::int64_t{t}, std::string_view(c.text("WARE", w))})
                          : c.text("WARE", w);
        };
        const std::size_t m = m_sell ? 1 : 0;
        if (m_page == Page::City) {
            switch (id) {
                case city_ids::kTitle:
                    return line(kHelpCityName);
                case city_ids::kSell:
                    return line(151);
                case city_ids::kBuy:
                    return line(152);
                case city_ids::kStock:
                    return line(153);
                case city_ids::kMaterialLock:
                    return line(154 + (city->material_lock ? 1 : 0));
                default:
                    return std::nullopt;
            }
        }
        if (m_page == Page::Stock || m_picker) {
            const std::int32_t w = id - city_ids::kLFrame;
            if (w >= kFirstTradeWare && w <= kLastTradeWare) {
                return stock_text(static_cast<std::uint8_t>(w));
            }
            return std::nullopt;
        }
        const std::vector<std::uint8_t> list = wares(*city);
        if (id >= city_ids::kKFrame && id < city_ids::kKFrame + city_ids::kKSlots) {
            const auto k = static_cast<std::size_t>(id - city_ids::kKFrame);
            return k < list.size() ? std::optional<std::string>(stock_text(list[k])) : line(41 + m);
        }
        switch (id) {
            case city_ids::kKTitle:
                return line(kHelpCityName);
            case city_ids::kKStop:
                return line(35 + m);
            case city_ids::kKPriceDown:
                return line(37 + m);
            case city_ids::kKPriceUp:
                return line(39 + m);
            case city_ids::kKPriceSlider:
                return line(43 + m);
            case city_ids::kKLimit:
                if (m_slot < static_cast<std::int32_t>(list.size())) {
                    const std::uint8_t w = list[static_cast<std::size_t>(m_slot)];
                    const sim::City::Trade* t = city->trade_of(w);
                    const std::uint32_t limit = t == nullptr ? 0 : (m_sell ? t->keep : t->limit);
                    return ui::format_text(c.text("GAME", 59 + m),
                                           {std::string_view(c.text("WARE", w)), std::int64_t{limit >> 5U}});
                }
                return std::nullopt;
            default:
                return std::nullopt;
        }
    }

private:
    enum class Page : std::uint8_t { City, Kontor, Stock };

    [[nodiscard]] sim::TradeSetting::Mode mode() const noexcept {
        return m_sell ? sim::TradeSetting::Mode::Sell : sim::TradeSetting::Mode::Buy;
    }
    [[nodiscard]] bool listed(const sim::City& city, std::uint8_t w) const {
        const sim::City::Trade* t = city.trade_of(w);
        return t != nullptr && (m_sell ? t->sell : t->buy);
    }
    /// The wares of the list, by ware number.
    [[nodiscard]] std::vector<std::uint8_t> wares(const sim::City& city) const {
        std::vector<std::uint8_t> list;
        for (std::uint8_t w = kFirstTradeWare; w <= kLastTradeWare; ++w) {
            if (listed(city, w)) {
                list.push_back(w);
            }
        }
        return list;
    }
    /// Places of the list: the Kontor's Maxware (sell) or its second value (buy).
    [[nodiscard]] std::int32_t slot_count(const sim::City& city, const PanelContext& c) const {
        const assets::BuildingType* k = sim::kontor_of(c.world(), c.buildings, city);
        const std::int32_t n = k == nullptr ? 0 : (m_sell ? k->max_wares : k->max_wares2);
        return std::clamp(n, 0, city_ids::kKSlots);
    }
    static sim::TradeSetting setting(const sim::City& city, const PanelContext& c, std::uint8_t w,
                                     sim::TradeSetting::Mode m) {
        sim::TradeSetting o;
        o.player = c.local;
        o.island = city.island;
        o.city = city.index;
        o.ware = w;
        o.mode = m;
        return o;
    }

    void refresh_city(const sim::City& city, const PanelContext& c) {
        const auto& cities = c.world().cities();
        std::size_t index = 0;
        for (std::size_t i = 0; i < cities.size(); ++i) {
            if (&cities[i] == &city) {
                index = i;
            }
        }
        const std::vector<std::uint32_t> workers = sim::city_workers(c.world(), c.buildings);
        const std::vector<std::int32_t> costs = sim::running_costs(c.world(), c.buildings);
        const std::int64_t people = sim::city_einwohner(city, index < workers.size() ? workers[index] : 0);
        const std::int64_t tax = sim::tax_income(city, c.buildings);
        const std::int64_t cost = index < costs.size() ? costs[index] : 0;
        const std::int64_t sales = city.sales / 480;
        const std::int64_t purchases = city.purchases / 480;
        const std::int64_t balance = tax - cost + sales - purchases;
        m_stadt.set_text(city_ids::kTitle, city.name);
        const std::array<std::pair<std::size_t, std::int64_t>, 6> rows{
            {{15, people}, {19, tax}, {18, cost}, {64, sales}, {65, purchases}, {80, balance}}};
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto label = static_cast<std::int32_t>(43007 + 3 * i);
            m_stadt.set_text(label, c.text("GAME", rows[i].first));
            m_stadt.set_text(label + 1, std::to_string(rows[i].second));
        }
        m_stadt.set_graphic(city_ids::kBalanceIcon, m_balance_icon + (balance < 0 ? 1 : 0), 0);
        m_stadt.set_selected(city_ids::kMaterialLock, city.material_lock);
    }

    void refresh_kontor(const sim::City& city, const PanelContext& c) {
        m_kontor.set_text(city_ids::kKTitle, city.name);
        m_kontor.set_selected(city_ids::kKMode, m_sell);
        const std::vector<std::uint8_t> list = wares(city);
        const std::int32_t slots = slot_count(city, c);
        const auto cap = static_cast<std::int32_t>(std::max<std::uint32_t>(city.capacity, 1));
        if (m_slot >= std::max<std::int32_t>(slots, static_cast<std::int32_t>(list.size()))) {
            m_slot = 0;  // the list shrank
        }
        for (std::int32_t k = 0; k < city_ids::kKSlots; ++k) {
            const bool ware = k < static_cast<std::int32_t>(list.size());
            const bool slot = ware || k < slots;
            m_kontor.set_visible(city_ids::kKIcon + k, slot);
            m_kontor.set_visible(city_ids::kKFrame + k, slot);
            m_kontor.set_visible(city_ids::kKBar + k, ware);
            m_kontor.set_selected(city_ids::kKFrame + k, k == m_slot);
            if (ware) {
                const std::uint8_t w = list[static_cast<std::size_t>(k)];
                m_kontor.set_graphic(city_ids::kKIcon + k, m_gfx_ware + w, 0);
                m_kontor.set_slider(city_ids::kKBar + k, 1, static_cast<std::int32_t>(city.stock(w)), cap);
            } else if (slot) {
                m_kontor.set_graphic(city_ids::kKIcon + k, m_gfx_empty, 0);
            }
        }
        const bool chosen = m_slot < static_cast<std::int32_t>(list.size());
        for (const std::int32_t id : {city_ids::kKName, city_ids::kKPrice}) {
            m_kontor.set_visible(id, chosen);
        }
        for (const std::int32_t id : {city_ids::kKPriceDown, city_ids::kKPriceUp, city_ids::kKPriceSlider,
                                      city_ids::kKLimit, city_ids::kKStop}) {
            m_kontor.set_disabled(id, !chosen);
        }
        m_kontor.set_text(city_ids::kKStockLabel, c.text("GAME", 0));
        if (!chosen) {
            m_kontor.set_slider(city_ids::kKPriceSlider, 0, kPriceSteps / 2, kPriceSteps);
            m_kontor.set_slider(city_ids::kKLimit, 0, 0, cap);
            m_kontor.set_slider(city_ids::kKLimitMarker, 0, 0, cap);
            m_kontor.set_slider(city_ids::kKStockBar, 0, 0, cap);
            m_kontor.set_text(city_ids::kKStockValue, ui::format_text(c.text("GAME", 49), {std::int64_t{0}}));
            return;
        }
        const std::uint8_t w = list[static_cast<std::size_t>(m_slot)];
        const sim::City::Trade* t = city.trade_of(w);
        const std::uint16_t base = sim::base_price(w);
        const auto min = static_cast<std::int32_t>(base * 300U >> 10U);
        const std::int32_t price = t == nullptr ? 0 : (m_sell ? t->sell_price : t->buy_price);
        const std::uint32_t limit = t == nullptr ? 0 : (m_sell ? t->keep : t->limit);
        m_kontor.set_text(city_ids::kKName, c.text("WARE", w));
        m_kontor.set_text(city_ids::kKPrice, std::to_string(price));
        const std::int32_t span = std::max(1, base - min);
        // A slider the mouse holds shows the mouse, not the game (its command comes a tick later).
        if (m_kontor.held() != city_ids::kKPriceSlider) {
            m_kontor.set_slider(city_ids::kKPriceSlider, 0,
                                std::clamp((price - min) * kPriceSteps / span, 0, kPriceSteps), kPriceSteps);
        }
        if (m_kontor.held() != city_ids::kKLimit) {
            m_kontor.set_slider(city_ids::kKLimit, 0, static_cast<std::int32_t>(limit), cap);
        }
        m_kontor.set_slider(city_ids::kKLimitMarker, 0, static_cast<std::int32_t>(limit), cap);
        m_kontor.set_slider(city_ids::kKStockBar, 0, static_cast<std::int32_t>(city.stock(w)), cap);
        m_kontor.set_text(city_ids::kKStockValue,
                          ui::format_text(c.text("GAME", 49), {std::int64_t{city.stock(w) >> 5U}}));
    }

    void refresh_stock(const sim::City& city, bool picker) {
        m_lager.set_text(44002, city.name);
        const auto cap = static_cast<std::int32_t>(std::max<std::uint32_t>(city.capacity, 1));
        for (std::uint8_t w = kFirstTradeWare; w <= kLastTradeWare; ++w) {
            const sim::City::Trade* t = city.trade_of(w);
            m_lager.set_graphic(city_ids::kLIcon + w, m_gfx_ware + w, 0);
            m_lager.set_slider(city_ids::kLBar + w, 1, static_cast<std::int32_t>(city.stock(w)), cap);
            const bool buy = t != nullptr && t->buy;
            const bool sell = t != nullptr && t->sell;
            m_lager.set_visible(city_ids::kLMarker + w, !(!buy && !sell));
            m_lager.set_visible(city_ids::kLCorner + w, !(!buy && !sell));
            if (buy || sell) {
                m_lager.set_slider(city_ids::kLMarker + w, 1, static_cast<std::int32_t>(buy ? t->limit : t->keep), cap);
                // Price level 0 (cheapest) .. 4 (base price).
                const std::int32_t base = sim::base_price(w);
                const auto min = static_cast<std::int32_t>(static_cast<std::uint32_t>(base) * 300U >> 10U);
                const std::int32_t price = buy ? t->buy_price : t->sell_price;
                const std::int32_t level = std::clamp(((price - min) * 5 - 1) / std::max(1, base - min), 0, 4);
                m_lager.set_graphic(city_ids::kLCorner + w, m_corner + (buy ? 0 : 5) + level, 0);
            }
        }
        // The 24th place: the empty frame that closes the picker.
        const std::int32_t last = kLastTradeWare + 1;
        m_lager.set_visible(city_ids::kLIcon + last, picker);
        m_lager.set_visible(city_ids::kLFrame + last, picker);
        if (picker) {
            m_lager.set_graphic(city_ids::kLIcon + last, m_gfx_empty, 0);
        }
    }

    ui::Panel m_stadt;
    ui::Panel m_kontor;
    ui::Panel m_lager;
    std::int32_t m_gfx_ware = 192;
    std::int32_t m_gfx_empty = 189;
    std::int32_t m_balance_icon = 0;
    std::int32_t m_corner = 634;
    Page m_page = Page::City;
    bool m_sell = true;
    bool m_picker = false;
    std::int32_t m_slot = 0;
    Selection m_last;
    bool m_seen_again = false;
};

// ---- Houses --------------------------------------------------------------------------------------

/// SIEDLER: the class of a house, its city's supply and the public buildings it lacks
/// (HousePanel_Refresh 0x00438A30).
class SettlerPanel final : public SimplePanel {
public:
    explicit SettlerPanel(assets::GadgetLayout layout) : SimplePanel(std::move(layout)) {
        const assets::GadgetLayout& l = m_panel.layout();
        m_gfx_ware = l.constant("GFXWARE").value_or(192);
        if (const assets::Gadget* g = l.find(kFace)) {
            m_face = g->gfx;
        }
        for (std::size_t k = 0; k < kServices; ++k) {
            if (const assets::Gadget* g = l.find(kService + static_cast<std::int32_t>(k))) {
                m_service_gfx[k] = g->gfx;
                m_service_x[k] = g->pos[0];
                m_service_w[k] = g->size[0];
            }
        }
    }

    void refresh(const Selection& s, const PanelContext& c) override {
        const assets::BuildingType* b = building_at(s, c);
        const sim::House* house = c.world().house(s.island, s.x, s.y);
        const sim::City* city = city_at(s, c);
        const auto& classes = c.buildings.resident_classes();
        if (b == nullptr || house == nullptr || city == nullptr || house->resident_class >= classes.size()) {
            return;
        }
        const std::uint8_t cls = house->resident_class;
        const assets::ResidentClass& rc = classes[cls];
        const int state = growth_state(*city, *house);
        m_panel.set_text(kTitle, city->name);
        m_panel.set_graphic(kFace, m_face + 4 * cls + (1 - state), 0);
        m_panel.set_text(kClassName, c.text("GAME", 3 + std::size_t{cls}));
        m_panel.set_text(kClassCount, std::to_string(cls < city->inhabitants.size() ? city->inhabitants[cls] : 0));
        const std::uint8_t tax = tax_of(*city, b->resident_class);
        m_panel.set_text(kTaxLabel, ui::format_text(c.text("GAME", 2), {std::int64_t{tax / 4}}));
        m_panel.set_text(kTaxIncome, std::to_string(class_tax(*city, rc, cls, tax)));
        if (m_panel.held() != kTaxSlider) {
            m_panel.set_slider(kTaxSlider, 0, tax - sim::CityOrder::kMinTax, 128);
        }

        // Goods: those of the class that are short, then (while the house grows) what the next
        // class will ask for.
        const auto ratio = [&](std::uint8_t w) -> std::int32_t {
            return w < city->supply.size() ? city->supply[w].ratio : 128;
        };
        const auto wants = [&](const assets::ResidentClass& r, std::uint8_t w) {
            return std::any_of(r.needs.begin(), r.needs.end(),
                               [&](const assets::ResidentClass::Need& n) { return n.ware == w && n.amount != 0; });
        };
        m_demand.clear();
        for (std::uint8_t w = 15; w <= 21; ++w) {
            if (wants(rc, w) && ratio(w) < 128) {
                m_demand.push_back({w, ratio(w)});
            }
        }
        const sim::Player* owner = c.world().player(city->owner);
        const std::uint32_t unlocked = owner != nullptr ? owner->infra_mask : 0;
        if (cls + 1U < classes.size() && cls < 4 && state > 0) {
            for (std::uint8_t w = 15; w <= 21; ++w) {
                if (!wants(classes[cls + 1U], w) || wants(rc, w) || !ware_unlocked(w, unlocked)) {
                    continue;
                }
                if (ratio(w) < 128) {
                    m_demand.push_back({w, ratio(w)});
                } else if (w < city->supply.size() && city->supply[w].demand == 0 && city->stock(w) < 32) {
                    m_demand.push_back({w, 0});
                }
            }
        }
        for (std::size_t k = 0; k < kDemandSlots; ++k) {
            const auto id = static_cast<std::int32_t>(k);
            const bool on = k < m_demand.size();
            m_panel.set_visible(kDemandIcon + id, on);
            m_panel.set_visible(kDemandBar + id, on);
            m_panel.set_visible(kDemandFrame + id, on);
            if (on) {
                m_panel.set_graphic(kDemandIcon + id, m_gfx_ware + m_demand[k].first, 0);
                m_panel.set_slider(kDemandBar + id, 1, m_demand[k].second, 128);
            }
        }
        // Overall supply of the goods the class wants against what it asks for at this tax.
        std::int32_t sum = 0;
        std::int32_t count = 0;
        for (const auto& n : rc.needs) {
            if (n.ware >= 15 && n.ware <= 21 && n.amount != 0) {
                sum += ratio(n.ware);
                ++count;
            }
        }
        const std::int32_t avg = count == 0 ? 128 : sum / count;
        const std::uint32_t idx = ((128U - (city->satisfaction_bonus >> 5U)) * tax) >> 7U;
        const auto threshold =
            static_cast<std::int32_t>(std::min<std::uint32_t>(128, (sim::tax_curve(idx) * rc.share) >> 7U));
        m_panel.set_slider(kGoodsBar, 1, avg, 128);
        m_panel.set_slider(kGoodsMarker, 1, threshold, 128);
        m_panel.set_text(kFoodLabel, c.text("GAME", 8));
        m_panel.set_slider(kFoodBar, 0, ratio(kFood) & ~7, 128);
        m_panel.set_slider(kFoodMarker, 0, 88, 128);

        // Public buildings the house lacks: for its class, or (growing) for the next.
        std::uint32_t mask = required(cls);
        if (cls < 4 && state > 0) {
            mask = required(cls + 1U) & unlocked;
        }
        const std::uint32_t missing = mask & ~std::uint32_t{house->services};
        m_missing.clear();
        for (std::size_t k = 0; k < kServices; ++k) {
            if ((missing & kServiceOrder[k].bit) != 0) {
                m_missing.push_back(k);
            }
        }
        m_lacks_now = state < 1;
        std::int32_t x = m_service_x[0];
        for (std::size_t k = 0; k < kServices; ++k) {
            const auto id = kService + static_cast<std::int32_t>(k);
            const bool on = k < m_missing.size();
            m_panel.set_visible(id, on);
            m_panel.set_visible(kServiceHover + static_cast<std::int32_t>(k), on);
            if (!on) {
                continue;
            }
            // Packed from the left by the widths of the pictures; drawn red (selected).
            const std::size_t which = kServiceOrder[m_missing[k]].gadget;
            const assets::Gadget* g = m_panel.layout().find(id);
            m_panel.set_graphic(id, m_service_gfx[which], 8);
            m_panel.set_selected(id, true);
            if (g != nullptr) {
                m_panel.set_offset(id, x - m_service_x[k], 0);
            }
            x += m_service_w[which];
        }
    }

    void clicked(const ui::PanelEvent& e, const Selection& s, const PanelContext& c,
                 std::vector<sim::Command>& out) override {
        const assets::BuildingType* b = building_at(s, c);
        const sim::City* city = city_at(s, c);
        if (b == nullptr || city == nullptr) {
            return;
        }
        const std::uint8_t tax = tax_of(*city, b->resident_class);
        std::int32_t rate = tax;
        if (e.kind == ui::PanelEvent::Kind::Slid && e.id == kTaxSlider) {
            rate = m_panel.slider(kTaxSlider, 0, 128) + sim::CityOrder::kMinTax;
        } else if (e.kind == ui::PanelEvent::Kind::Clicked && e.id == kTaxDown) {
            rate = tax - 4;
        } else if (e.kind == ui::PanelEvent::Kind::Clicked && e.id == kTaxUp) {
            rate = tax + 4;
        } else {
            return;
        }
        rate = std::clamp<std::int32_t>(rate, sim::CityOrder::kMinTax, sim::CityOrder::kMaxTax);
        if (rate == tax) {
            return;  // the slider reports every frame: a command only for a change
        }
        sim::CityOrder o;
        o.player = c.local;
        o.island = city->island;
        o.city = city->index;
        o.op = sim::CityOrder::Op::TaxRate;
        o.resident_class = b->resident_class;
        o.rate = static_cast<std::uint8_t>(rate);
        out.emplace_back(o);
    }

    [[nodiscard]] std::optional<std::string> help(std::int32_t id, const Selection& s,
                                                  const PanelContext& c) const override {
        const auto line = [&](std::size_t n) { return std::optional<std::string>(help_line(c, n)); };
        switch (id) {
            case kTitle:
                return line(kHelpCityName);
            case kTaxSlider:
                return line(57);
            case kTaxDown:
                return line(55);
            case kTaxUp:
                return line(56);
            case kTaxLabel:
                return line(58);
            case kClassCount:
                return line(53);
            case kClassName:
                return line(54);
            case kGoodsHover:
                return line(60);
            case kFoodLabel:
                return line(61);
            case kFace: {
                const sim::House* house = c.world().house(s.island, s.x, s.y);
                const sim::City* city = city_at(s, c);
                if (house == nullptr || city == nullptr) {
                    return std::nullopt;
                }
                return line(static_cast<std::size_t>(78 + (1 - growth_state(*city, *house))));
            }
            default:
                break;
        }
        if (id >= kDemandFrame && id < kDemandFrame + static_cast<std::int32_t>(kDemandSlots)) {
            const auto k = static_cast<std::size_t>(id - kDemandFrame);
            const sim::City* city = city_at(s, c);
            if (k >= m_demand.size() || city == nullptr) {
                return std::nullopt;
            }
            const std::uint8_t w = m_demand[k].first;
            const std::string name = c.text("WAREM", w);
            if (w >= city->supply.size() || city->supply[w].demand == 0) {
                return ui::format_text(c.text("GAME", 63), {std::string_view(name)});
            }
            const std::int64_t p = std::int64_t{m_demand[k].second} * 100 / 128;
            return p == 0 ? ui::format_text(c.text("GAME", 62), {std::string_view(name)})
                          : ui::format_text(c.text("GAME", 61), {std::string_view(name), p});
        }
        if (id >= kServiceHover && id < kServiceHover + static_cast<std::int32_t>(kServices)) {
            const auto k = static_cast<std::size_t>(id - kServiceHover);
            if (k >= m_missing.size()) {
                return std::nullopt;
            }
            return line(kServiceOrder[m_missing[k]].help + (m_lacks_now ? 8U : 0U));
        }
        return std::nullopt;
    }

private:
    static constexpr std::int32_t kTitle = 36002;
    static constexpr std::int32_t kTaxSlider = 36004;
    static constexpr std::int32_t kTaxDown = 36005;
    static constexpr std::int32_t kTaxUp = 36006;
    static constexpr std::int32_t kGoodsBar = 36008;
    static constexpr std::int32_t kGoodsMarker = 36009;
    static constexpr std::int32_t kFace = 36010;
    static constexpr std::int32_t kTaxLabel = 36011;
    static constexpr std::int32_t kTaxIncome = 36012;
    static constexpr std::int32_t kClassCount = 36013;
    static constexpr std::int32_t kClassName = 36014;
    static constexpr std::int32_t kGoodsHover = 36015;
    static constexpr std::int32_t kFoodBar = 36017;
    static constexpr std::int32_t kFoodMarker = 36018;
    static constexpr std::int32_t kFoodLabel = 36019;
    static constexpr std::int32_t kService = 36021;
    static constexpr std::int32_t kDemandIcon = 36031;
    static constexpr std::int32_t kDemandBar = 36051;
    static constexpr std::int32_t kDemandFrame = 36071;
    static constexpr std::int32_t kServiceHover = 36091;
    static constexpr std::size_t kDemandSlots = 6;
    static constexpr std::size_t kServices = 8;
    static constexpr std::uint8_t kFood = 14;

    /// The public buildings in the order of the icons: coverage bit, which of the eight gadgets
    /// carries its picture (sprites 339 Kapelle, 340 Markt, 341 Wirtshaus, 342 Schule, 343 Theater,
    /// 344 Badehaus, 345 Hochschule, 346 Kirche) and its LEISTE line ("verlangen nach").
    struct Service {
        std::uint32_t bit;
        std::size_t gadget;
        std::size_t help;
    };
    static constexpr std::array<Service, kServices> kServiceOrder{{
        {0x01, 1, 64},  // Markt
        {0x02, 0, 62},  // Kapelle
        {0x10, 7, 63},  // Kirche
        {0x08, 2, 65},  // Wirtshaus
        {0x04, 3, 66},  // Schule
        {0x80, 6, 67},  // Hochschule
        {0x20, 5, 69},  // Badehaus
        {0x40, 4, 68},  // Theater
    }};

    /// What a class needs of public buildings (Class_RequiredServices 0x0047F320).
    static std::uint32_t required(std::uint32_t cls) {
        constexpr std::array<std::uint32_t, 5> kRequired{0x01, 0x03, 0x0F, 0x3D, 0xF9};
        return cls < kRequired.size() ? kRequired[cls] : 0;
    }
    /// Goods a next class asks for only once their level is reached (exe constants: Tabakwaren,
    /// Gewürze, Alkohol from INFRA 2A, Kakao and Kleidung from 3D, Schmuck from 4A; Stoffe always).
    static bool ware_unlocked(std::uint8_t w, std::uint32_t mask) {
        switch (w) {
            case 15:
            case 16:
            case 18:
                return (mask & 0x8000U) != 0;
            case 17:
            case 20:
                return (mask & 0x2000000U) != 0;
            case 21:
                return (mask & 0x10000000U) != 0;
            default:
                return true;
        }
    }
    /// Sign of the growth rate: 1 grows, 0 stays, -1 shrinks a little, -2 a lot (0x0047B3B0).
    static int growth_state(const sim::City& city, const sim::House& house) {
        const std::int32_t rate = sim::growth_rate(city, house);
        return rate > 0 ? 1 : rate == 0 ? 0 : rate >= -64 ? -1 : -2;
    }
    static std::uint8_t tax_of(const sim::City& city, std::uint8_t cls) {
        return cls < city.tax_rate.size() ? city.tax_rate[cls] : std::uint8_t{128};
    }
    /// Taxes of one class per minute (City_ClassTax 0x0047F370).
    static std::int64_t class_tax(const sim::City& city, const assets::ResidentClass& rc, std::uint8_t cls,
                                  std::uint8_t tax) {
        const std::int64_t n = cls < city.inhabitants.size() ? city.inhabitants[cls] : 0;
        return (std::int64_t{rc.tax} * n * 6 / 32) * tax / 128;
    }

    std::int32_t m_gfx_ware = 192;
    std::int32_t m_face = 319;
    std::array<std::int32_t, kServices> m_service_gfx{};
    std::array<std::int32_t, kServices> m_service_x{};
    std::array<std::int32_t, kServices> m_service_w{};
    std::vector<std::pair<std::uint8_t, std::int32_t>> m_demand;  ///< ware, supply 0..128
    std::vector<std::size_t> m_missing;                           ///< into kServiceOrder
    bool m_lacks_now = false;
};

// ---- Shipyards -----------------------------------------------------------------------------------

const sim::CostBuilding* cost_building_at(const Selection& s, const PanelContext& c, sim::CostBuilding::Kind kind) {
    for (const sim::CostBuilding& b : c.world().cost_buildings()) {
        if (b.kind == kind && b.island == s.island && b.x == s.x && b.y == s.y) {
            return &b;
        }
    }
    return nullptr;
}

constexpr std::uint16_t kFirstShipFigure = 21;  // HANDEL1; the four types are 21, 23, 25, 27

/// WERFT (the yard and its order) and BAUSHIP (the choice of the ship type)
/// (ShipyardPanel_Refresh 0x00435B40, ShipTypePanel_Refresh 0x00435EF0).
class ShipyardPanel final : public SelectionPanel {
public:
    ShipyardPanel(assets::GadgetLayout werft, assets::GadgetLayout bauship)
        : m_werft(std::move(werft), 1.0), m_bauship(std::move(bauship), 1.0) {
        m_gfx_ware = m_werft.layout().constant("GFXWARE").value_or(192);
        for (std::int32_t k = 0; k < 4; ++k) {
            if (const assets::Gadget* g = m_bauship.layout().find(kType + k)) {
                m_type_x[static_cast<std::size_t>(k)] = g->offset[0];
            }
        }
    }
    void set_scale(double scale) override {
        m_werft.set_scale(scale);
        m_bauship.set_scale(scale);
    }
    ui::Panel& panel() override { return m_choose ? m_bauship : m_werft; }

    void refresh(const Selection& s, const PanelContext& c) override {
        Selection key = s;
        key.again = false;
        if (key != m_last) {
            m_last = key;
            m_choose = false;
        }
        const assets::BuildingType* b = building_at(s, c);
        const sim::CostBuilding* yard = cost_building_at(s, c, sim::CostBuilding::Kind::Shipyard);
        const sim::City* city = city_at(s, c);
        if (b == nullptr || yard == nullptr) {
            return;
        }
        const std::string tons = c.text("GAME", kGameTons);
        const auto ware_row = [&](ui::Panel& p, std::int32_t icon, std::int32_t amount, std::uint8_t w,
                                  std::uint32_t stock) {
            p.set_graphic(icon, m_gfx_ware + w, 0);
            p.set_text(amount, ui::format_text(tons, {std::int64_t{stock >> 5U}}));
        };
        if (m_choose) {
            m_bauship.set_text(60002, city != nullptr ? city->name : std::string());
            // A small yard (three tiles wide) builds the small types only, moved to the middle.
            const bool small = b->size_x == 3;
            for (std::int32_t k = 0; k < 4; ++k) {
                const bool big = k == 1 || k == 3;
                m_bauship.set_visible(kType + k, !(small && big));
                m_bauship.set_offset(kType + k, m_type_x[static_cast<std::size_t>(k)] + (small && !big ? 40 : 0), 0);
            }
            if (small && (m_type == 1 || m_type == 3)) {
                m_type = static_cast<std::uint8_t>(m_type - 1);
            }
            for (std::int32_t k = 0; k < 4; ++k) {
                m_bauship.set_selected(kType + k, k == m_type);
            }
            if (const assets::FigureType* f = c.figures.get(kFirstShipFigure + 2U * m_type)) {
                m_bauship.set_text(60015, ui::format_text(tons, {std::int64_t{f->ship.cargo_slots} * 50}));
                m_bauship.set_text(60017, std::to_string(f->ship.max_cannons));
                m_bauship.set_text(60019, ui::format_text(c.text("GAME", 47), {std::int64_t{f->speed / 20}}));
                const std::uint32_t wood = f->ship.build_amount;
                m_bauship.set_text(60020, std::to_string(wood >> 5U));
                const std::uint64_t cloth =
                    b->raw_amount != 0 ? std::uint64_t{b->raw2_amount} * wood / (std::uint64_t{b->raw_amount} * 32) : 0;
                m_bauship.set_text(60021, std::to_string(cloth));
            }
            ware_row(m_bauship, 60031, 60041, b->raw_ware, yard->wood);
            ware_row(m_bauship, 60032, 60042, b->raw2_ware, yard->cloth);
            return;
        }
        m_werft.set_text(39002, city != nullptr ? city->name : std::string());
        m_werft.set_text(39006, building_name(*b, c));
        m_werft.set_text(39003, c.text("GAME", kGameRunningCost));
        m_werft.set_text(39004, std::to_string(yard->enabled ? b->running_cost : b->idle_cost));
        ware_row(m_werft, 39021, 39031, b->raw_ware, yard->wood);
        ware_row(m_werft, 39022, 39032, b->raw2_ware, yard->cloth);
        const bool order = yard->order != sim::CostBuilding::kNoOrder;
        for (const std::int32_t id : {39011, 39012, 39014, 39015}) {
            m_werft.set_visible(id, order);
        }
        m_werft.set_visible(39016, !(order));
        m_werft.set_visible(39017, order);
        m_werft.set_selected(39018, !yard->enabled);
        if (order) {
            const assets::FigureType* f = c.figures.get(kFirstShipFigure + 2U * yard->order);
            const std::uint32_t need = f != nullptr ? std::max<std::uint32_t>(f->ship.build_amount, 1) : 1;
            m_werft.set_slider(
                39012, 1,
                static_cast<std::int32_t>(std::min<std::uint64_t>(std::uint64_t{yard->progress} * 1024 / need, 1024)),
                1024);
            for (const std::int32_t id : {39014, 39015}) {
                m_werft.set_graphic(id, yard->order, 0);
            }
        }
    }

    void clicked(const ui::PanelEvent& e, const Selection& s, const PanelContext& c,
                 std::vector<sim::Command>& out) override {
        if (e.kind != ui::PanelEvent::Kind::Clicked) {
            return;
        }
        const sim::CostBuilding* yard = cost_building_at(s, c, sim::CostBuilding::Kind::Shipyard);
        if (yard == nullptr) {
            return;
        }
        sim::ShipyardOrder o;
        o.player = c.local;
        o.island = s.island;
        o.x = s.x;
        o.y = s.y;
        if (m_choose) {
            if (e.id >= kType && e.id < kType + 4) {
                m_type = static_cast<std::uint8_t>(e.id - kType);
            } else if (e.id == 60022) {
                o.op = sim::ShipyardOrder::Op::Build;
                o.ship_type = m_type;
                out.emplace_back(o);
                m_choose = false;
            }
            return;
        }
        switch (e.id) {
            case 39016:  // order a ship: the type choice, the first type picked
                m_choose = true;
                m_type = 0;
                break;
            case 39017:
                o.op = sim::ShipyardOrder::Op::Cancel;
                out.emplace_back(o);
                break;
            case 39018:
                o.op = sim::ShipyardOrder::Op::Enable;
                o.on = !yard->enabled;
                out.emplace_back(o);
                break;
            default:
                break;
        }
    }

    [[nodiscard]] std::optional<std::string> help(std::int32_t id, const Selection& s,
                                                  const PanelContext& c) const override {
        const auto line = [&](std::size_t n) { return std::optional<std::string>(help_line(c, n)); };
        const sim::CostBuilding* yard = cost_building_at(s, c, sim::CostBuilding::Kind::Shipyard);
        if (m_choose) {
            if (id >= kType && id < kType + 4) {
                return line(static_cast<std::size_t>(95 + id - kType));
            }
            switch (id) {
                case 60002:
                    return line(kHelpCityName);
                case 60015:
                    return line(89);
                case 60017:
                    return line(90);
                case 60019:
                    return line(91);
                case 60020:
                    return line(93);
                case 60021:
                    return line(92);
                case 60022:
                    return line(87);
                case 60051:
                    return line(101);
                case 60052:
                    return line(102);
                default:
                    return std::nullopt;
            }
        }
        switch (id) {
            case 39002:
                return line(kHelpCityName);
            case 39013:
                return line(94);
            case 39015:
                return yard != nullptr && yard->order < 4 ? line(95U + yard->order) : std::nullopt;
            case 39016:
                return line(87);
            case 39017:
                return line(88);
            case 39018:
                return line(yard != nullptr && yard->enabled ? 99 : 100);
            case 39041:
                return line(101);
            case 39042:
                return line(102);
            default:
                return std::nullopt;
        }
    }

    bool back() override {
        if (!m_choose) {
            return false;
        }
        m_choose = false;
        return true;
    }

private:
    static constexpr std::int32_t kType = 60003;
    ui::Panel m_werft;
    ui::Panel m_bauship;
    std::int32_t m_gfx_ware = 192;
    std::array<std::int32_t, 4> m_type_x{};
    bool m_choose = false;
    std::uint8_t m_type = 0;
    Selection m_last;
};

// ---- Castles -------------------------------------------------------------------------------------

/// MILITAR: training places, the unit to train, its pay and weapons (CastlePanel_Refresh 0x004385E0).
class CastlePanel final : public SimplePanel {
public:
    explicit CastlePanel(assets::GadgetLayout layout) : SimplePanel(std::move(layout)) {
        const assets::GadgetLayout& l = m_panel.layout();
        m_gfx_ware = l.constant("GFXWARE").value_or(192);
        m_gfx_empty = l.constant("GFXWARELEER").value_or(188) + 1;
        if (const assets::Gadget* g = l.find(40007)) {
            m_weapon_icon = g->gfx;
        }
    }

    void refresh(const Selection& s, const PanelContext& c) override {
        const assets::BuildingType* b = building_at(s, c);
        const sim::CostBuilding* castle = cost_building_at(s, c, sim::CostBuilding::Kind::Castle);
        const sim::City* city = city_at(s, c);
        const sim::Player* owner = city != nullptr ? c.world().player(city->owner) : nullptr;
        if (b == nullptr || castle == nullptr || owner == nullptr) {
            return;
        }
        m_panel.set_text(40002, city->name);
        const std::int32_t places = std::min<std::int32_t>(b->max_wares, 9);
        const std::int32_t colour = std::min<std::int32_t>(owner->banner, 3);
        for (std::int32_t k = 0; k < 9; ++k) {
            const bool place = k < places;
            const bool used = place && static_cast<std::size_t>(k) < castle->slots.size();
            m_panel.set_visible(40091 + k, place);
            m_panel.set_visible(40031 + k, place);
            m_panel.set_visible(40071 + k, used);
            m_panel.set_visible(40051 + k, used);
            m_panel.set_selected(40091 + k, k == m_slot);
            if (!place) {
                continue;
            }
            if (!used) {
                m_panel.set_graphic(40031 + k, m_gfx_empty, 0);
                m_panel.set_disabled(40031 + k, false);
                continue;
            }
            const sim::CastleSlot& slot = castle->slots[static_cast<std::size_t>(k)];
            const assets::FigureType* f = c.figures.get(slot.figure);
            const std::int32_t kind = f != nullptr ? f->kind : 1;
            // The soldier icons follow the wares: four colours per kind of unit.
            m_panel.set_graphic(40031 + k, m_gfx_ware + 21 + colour + 4 * kind, 0);
            m_panel.set_disabled(40031 + k, slot.training);
            m_panel.set_text(40071 + k, std::to_string(std::min<std::uint32_t>(slot.progress, 320) * 10 / 320));
            const std::uint32_t max = f != nullptr ? std::max<std::uint32_t>(f->combat.max_health, 1) : 1;
            m_panel.set_slider(40051 + k, 1, static_cast<std::int32_t>(std::min(slot.health, max)),
                               static_cast<std::int32_t>(max));
        }
        for (std::int32_t u = 0; u < 4; ++u) {
            m_panel.set_selected(40021 + u, u == m_unit);
        }
        const assets::FigureType* unit = c.figures.get(unit_figure(colour));
        if (unit != nullptr) {
            m_panel.set_text(40006, std::to_string(c.figures.unit_upkeep(unit->kind)));
            m_panel.set_graphic(40007, m_weapon_icon + unit->combat.weapon_ware - 11, 0);
            m_panel.set_text(40008, std::to_string(unit->combat.weapon_amount >> 5U));
        }
        for (std::int32_t w = 0; w < 3; ++w) {
            m_panel.set_graphic(40111 + w, m_gfx_ware + 11 + w, 0);
            m_panel.set_text(40121 + w,
                             ui::format_text(c.text("GAME", kGameTons),
                                             {std::int64_t{castle->weapons[static_cast<std::size_t>(w)] >> 5U}}));
        }
        const bool free = static_cast<std::int32_t>(castle->slots.size()) < places;
        m_panel.set_disabled(40009, !free);
        const bool chosen = static_cast<std::size_t>(m_slot) < castle->slots.size();
        m_panel.set_visible(40003, chosen);
        m_panel.set_visible(40004, !(!chosen || castle->slots[static_cast<std::size_t>(std::max(m_slot, 0))].training));
    }

    void clicked(const ui::PanelEvent& e, const Selection& s, const PanelContext& c,
                 std::vector<sim::Command>& out) override {
        if (e.kind != ui::PanelEvent::Kind::Clicked) {
            return;
        }
        const sim::CostBuilding* castle = cost_building_at(s, c, sim::CostBuilding::Kind::Castle);
        const sim::City* city = city_at(s, c);
        const sim::Player* owner = city != nullptr ? c.world().player(city->owner) : nullptr;
        if (castle == nullptr || owner == nullptr) {
            return;
        }
        if (e.id >= 40091 && e.id < 40100) {
            m_slot = e.id - 40091;
            return;
        }
        if (e.id >= 40021 && e.id <= 40024) {
            m_unit = e.id - 40021;
            return;
        }
        sim::CastleOrder o;
        o.player = c.local;
        o.island = s.island;
        o.x = s.x;
        o.y = s.y;
        o.slot = static_cast<std::uint8_t>(std::max(m_slot, 0));
        switch (e.id) {
            case 40009:
                o.op = sim::CastleOrder::Op::Recruit;
                o.figure = unit_figure(std::min<std::int32_t>(owner->banner, 3));
                out.emplace_back(o);
                break;
            case 40003:
                o.op = sim::CastleOrder::Op::Cancel;
                out.emplace_back(o);
                break;
            case 40004:
                m_picking = true;  // the unit's destination: the next click on the map
                break;
            default:
                break;
        }
    }

    [[nodiscard]] bool picking() const override { return m_picking; }
    void picked(std::uint32_t x, std::uint32_t y, const Selection& s, const PanelContext& c,
                std::vector<sim::Command>& out) override {
        m_picking = false;
        sim::CastleOrder o;
        o.player = c.local;
        o.island = s.island;
        o.x = s.x;
        o.y = s.y;
        o.op = sim::CastleOrder::Op::Release;
        o.slot = static_cast<std::uint8_t>(std::max(m_slot, 0));
        o.spot = sim::Target::spot(2 * x + 1, 2 * y + 1);
        out.emplace_back(o);
    }
    bool back() override {
        const bool was = m_picking;
        m_picking = false;
        return was;
    }

    [[nodiscard]] std::optional<std::string> help(std::int32_t id, const Selection& s,
                                                  const PanelContext& c) const override {
        const auto line = [&](std::size_t n) { return std::optional<std::string>(help_line(c, n)); };
        switch (id) {
            case 40003:
                return line(85);
            case 40004:
                return line(84);
            case 40006:
                return line(83);
            case 40009:
                return line(86);
            default:
                break;
        }
        if (id >= 40021 && id <= 40024) {
            return line(82);
        }
        const sim::CostBuilding* castle = cost_building_at(s, c, sim::CostBuilding::Kind::Castle);
        if (castle == nullptr) {
            return std::nullopt;
        }
        if (id == 40008) {
            const sim::City* city = city_at(s, c);
            const sim::Player* owner = city != nullptr ? c.world().player(city->owner) : nullptr;
            const assets::FigureType* unit =
                owner != nullptr ? c.figures.get(unit_figure(std::min<std::int32_t>(owner->banner, 3))) : nullptr;
            if (unit == nullptr) {
                return std::nullopt;
            }
            return ui::format_text(c.text("GAME", 73), {std::int64_t{unit->combat.weapon_amount >> 5U},
                                                        std::string_view(c.text("WARE", unit->combat.weapon_ware))});
        }
        if (id >= 40091 && id < 40100) {
            const auto k = static_cast<std::size_t>(id - 40091);
            if (k >= castle->slots.size()) {
                return std::nullopt;
            }
            const sim::CastleSlot& slot = castle->slots[k];
            std::string text = slot.progress >= 320
                                   ? c.text("GAME", 75)
                                   : ui::format_text(c.text("GAME", 74), {std::int64_t{slot.progress * 100 / 320}});
            if (!slot.armed) {
                text += c.text("GAME", 76);
            }
            return text;
        }
        if (id >= 40131 && id <= 40133) {
            const auto w = static_cast<std::uint8_t>(11 + id - 40131);
            return ui::format_text(c.text("GAME", 50),
                                   {std::int64_t{castle->weapons[static_cast<std::size_t>(w - 11)] >> 5U},
                                    std::string_view(c.text("WARE", w))});
        }
        return std::nullopt;
    }

private:
    /// SOLDAT1, KAVALERIE1, MUSKETIER1, KANONIER1 of a colour: 1 + colour + 4 * unit.
    [[nodiscard]] std::uint16_t unit_figure(std::int32_t colour) const noexcept {
        return static_cast<std::uint16_t>(1 + colour + 4 * m_unit);
    }
    std::int32_t m_gfx_ware = 192;
    std::int32_t m_gfx_empty = 189;
    std::int32_t m_weapon_icon = 432;
    std::int32_t m_slot = 0;
    std::int32_t m_unit = 0;
    bool m_picking = false;
};

// ---- A foreign Kontor --------------------------------------------------------------------------

/// HANDEL: what a foreign Kontor with a trade treaty wants and offers (ForeignKontorPanel_Refresh
/// 0x004357D0).
class ForeignKontorPanel final : public SimplePanel {
public:
    explicit ForeignKontorPanel(assets::GadgetLayout layout) : SimplePanel(std::move(layout)) {
        m_gfx_ware = m_panel.layout().constant("GFXWARE").value_or(192);
        m_gfx_empty = m_panel.layout().constant("GFXWARELEER").value_or(188) + 1;
    }

    void refresh(const Selection& s, const PanelContext& c) override {
        const sim::City* city = city_at(s, c);
        if (city == nullptr) {
            return;
        }
        m_panel.set_text(53002, city->name);
        m_panel.set_text(53003, c.text("GAME", 36));
        m_panel.set_text(53004, c.text("GAME", 35));
        const assets::BuildingType* k = sim::kontor_of(c.world(), c.buildings, *city);
        fill(*city, true, 53011, 53031, 53051, 11, k != nullptr ? k->max_wares2 : 0);
        fill(*city, false, 53071, 53081, 53091, 8, k != nullptr ? k->max_wares : 0);
    }
    void clicked(const ui::PanelEvent&, const Selection&, const PanelContext&, std::vector<sim::Command>&) override {}

    [[nodiscard]] std::optional<std::string> help(std::int32_t id, const Selection& s,
                                                  const PanelContext& c) const override {
        const sim::City* city = city_at(s, c);
        if (city == nullptr) {
            return std::nullopt;
        }
        const auto row = [&](bool buy, std::size_t k) -> std::optional<std::string> {
            const auto& list = buy ? m_buy : m_sell;
            if (k >= list.size()) {
                return std::nullopt;
            }
            const std::uint8_t w = list[k];
            const sim::City::Trade* t = city->trade_of(w);
            const std::int64_t price = t == nullptr ? 0 : (buy ? t->buy_price : t->sell_price);
            return ui::format_text(c.text("GAME", 58), {std::int64_t{amount(*city, w, buy) >> 5U},
                                                        std::string_view(c.text("WARE", w)), price});
        };
        if (id >= 53051 && id <= 53061) {
            return row(true, static_cast<std::size_t>(id - 53051));
        }
        if (id >= 53091 && id <= 53098) {
            return row(false, static_cast<std::size_t>(id - 53091));
        }
        return std::nullopt;
    }

private:
    /// What the city still buys (up to its limit) or sells (above what it keeps), 0 below 1 t.
    static std::uint32_t amount(const sim::City& city, std::uint8_t w, bool buy) {
        const sim::City::Trade* t = city.trade_of(w);
        if (t == nullptr) {
            return 0;
        }
        const std::int64_t stock = city.stock(w);
        const std::int64_t reserved = city.reserved_of(w);
        const std::int64_t n =
            buy ? std::int64_t{t->limit} - stock + reserved : stock - std::int64_t{t->keep} - reserved;
        return n < 32 ? 0 : static_cast<std::uint32_t>(n);
    }
    void fill(const sim::City& city, bool buy, std::int32_t icon, std::int32_t bar, std::int32_t frame,
              std::int32_t gadgets, std::int32_t places) {
        auto& list = buy ? m_buy : m_sell;
        list.clear();
        for (std::uint8_t w = kFirstTradeWare; w <= kLastTradeWare; ++w) {
            const sim::City::Trade* t = city.trade_of(w);
            if (t != nullptr && (buy ? t->buy : t->sell)) {
                list.push_back(w);
            }
        }
        const auto cap = static_cast<std::int32_t>(std::max<std::uint32_t>(city.capacity, 1));
        for (std::int32_t k = 0; k < gadgets; ++k) {
            const bool ware = k < static_cast<std::int32_t>(list.size());
            const bool place = ware || k < places;
            m_panel.set_visible(icon + k, place);
            m_panel.set_visible(frame + k, place);
            m_panel.set_visible(bar + k, ware);
            if (ware) {
                const std::uint8_t w = list[static_cast<std::size_t>(k)];
                m_panel.set_graphic(icon + k, m_gfx_ware + w, 0);
                m_panel.set_slider(
                    bar + k, 1, static_cast<std::int32_t>(std::min<std::uint32_t>(amount(city, w, buy), city.capacity)),
                    cap);
            } else if (place) {
                m_panel.set_graphic(icon + k, m_gfx_empty, 0);
            }
        }
    }
    std::int32_t m_gfx_ware = 192;
    std::int32_t m_gfx_empty = 189;
    std::vector<std::uint8_t> m_buy;
    std::vector<std::uint8_t> m_sell;
};

// ---- Ships ---------------------------------------------------------------------------------------

constexpr std::uint8_t kCannonWare = 13;  // KANONEN
constexpr std::uint8_t kFirstSoldierWare = 25;
constexpr std::uint8_t kLastSoldierWare = 44;
constexpr std::array<std::uint32_t, 4> kSteps{32, 5 * 32, 10 * 32, 50 * 32};  // 1, 5, 10, 50 t

/// SHIP (the ship), TRANSFER (with the own Kontor) and TRADE (with a foreign one)
/// (ShipPanel_Refresh 0x00436170, TransferPanel_Refresh 0x004372C0, TradePanel_Refresh 0x004377F0).
class ShipPanel final : public SelectionPanel {
public:
    ShipPanel(assets::GadgetLayout ship, assets::GadgetLayout transfer, assets::GadgetLayout trade)
        : m_ship(std::move(ship), 1.0), m_transfer(std::move(transfer), 1.0), m_trade(std::move(trade), 1.0) {
        const assets::GadgetLayout& l = m_ship.layout();
        m_gfx_ware = l.constant("GFXWARE").value_or(192);
        m_gfx_empty = l.constant("GFXWARELEER").value_or(188) + 1;
        m_gfx_ship = l.constant("GFXSHIP").value_or(363);
        // Not in Open1602 yet: the route editor, scuttling, the pirates' offers.
        for (const std::int32_t id : {kRouteEditor, kScuttle, kPirates, kDeadSlider}) {
            m_ship.set_visible(id, false);
        }
        m_transfer.set_selected(45003, true);
        m_transfer.set_selected(45141, true);
        m_trade.set_selected(33003, true);
    }
    void set_scale(double scale) override {
        m_ship.set_scale(scale);
        m_transfer.set_scale(scale);
        m_trade.set_scale(scale);
    }
    ui::Panel& panel() override {
        return m_page == Page::Transfer ? m_transfer : m_page == Page::Trade ? m_trade : m_ship;
    }

    void refresh(const Selection& s, const PanelContext& c) override {
        if (s.ship != m_last_ship) {
            m_last_ship = s.ship;
            m_page = Page::Root;
            m_hold = 0;
            m_offer = 0;
        }
        const sim::Ship* ship = c.world().ship(s.ship);
        if (ship == nullptr) {
            return;
        }
        const Context k = context(*ship, c);
        // Transfer and trade end when the ship sails off (0x0043AA00).
        if (k.moving || (m_page == Page::Transfer && !k.own_kontor) || (m_page == Page::Trade && !k.foreign_kontor)) {
            m_page = Page::Root;
        }
        switch (m_page) {
            case Page::Root:
                refresh_root(*ship, k, c);
                break;
            case Page::Transfer:
                refresh_transfer(*ship, k, c);
                break;
            case Page::Trade:
                refresh_trade(*ship, k, c);
                break;
        }
    }

    void clicked(const ui::PanelEvent& e, const Selection& s, const PanelContext& c,
                 std::vector<sim::Command>& out) override {
        const sim::Ship* ship = c.world().ship(s.ship);
        if (ship == nullptr || ship->owner != c.local) {
            // Somebody else's ship: only buying it, if it is offered.
            if (ship != nullptr && e.kind == ui::PanelEvent::Kind::Clicked && e.id == kSale && ship->for_sale) {
                out.emplace_back(ship_command(*ship, c, sim::ShipCommand::Op::Buy));
            }
            return;
        }
        if (m_page == Page::Transfer) {
            clicked_transfer(e, *ship, c, out);
        } else if (m_page == Page::Trade) {
            clicked_trade(e, *ship, c, out);
        } else {
            clicked_root(e, *ship, c, out);
        }
    }

    [[nodiscard]] std::optional<std::string> help(std::int32_t id, const Selection& s,
                                                  const PanelContext& c) const override {
        const sim::Ship* ship = c.world().ship(s.ship);
        if (ship == nullptr) {
            return std::nullopt;
        }
        const auto line = [&](std::size_t n) { return std::optional<std::string>(help_line(c, n)); };
        // Holds: their goods, the soldiers' kind, or empty.
        const auto hold_text = [&](std::size_t h) -> std::optional<std::string> {
            if (h >= ship->cargo.size()) {
                return std::nullopt;
            }
            const sim::CargoSlot& slot = ship->cargo[h];
            if (slot.ware == 0 || slot.amount == 0) {
                return c.text("WARE", 0);
            }
            if (slot.ware >= kFirstSoldierWare && slot.ware <= kLastSoldierWare) {
                return c.text("WARE", slot.ware);
            }
            return ui::format_text(c.text("GAME", 50),
                                   {std::int64_t{slot.amount >> 5U}, std::string_view(c.text("WARE", slot.ware))});
        };
        if (m_page == Page::Root) {
            switch (id) {
                case kName:
                    return line(10);
                case kRepair:
                    return line(103);
                case kSale:
                    return line(ship->owner != c.local ? 107 : ship->for_sale ? 106 : 105);
                case kTrade:
                    return line(108);
                case kCannonsDown:
                    return line(119);
                case kCannons:
                    return ship->cannons > 0 ? ui::format_text(c.text("GAME", 23), {std::int64_t{ship->cannons}})
                                             : c.text("GAME", 24);
                case kHealthHover:
                    return line(121);
                case kPriceSlider:
                    return line(118);
                case kPriceDown:
                    return line(116);
                case kPriceUp:
                    return line(117);
                case kFound:
                    return line(122);
                case kTransfer:
                    return line(123);
                case kRoute:
                    return line(ship->route.active ? 110 : 111);
                case kStop:
                    return line(127);
                case kExplore:
                    return line(125);
                default:
                    break;
            }
            if (id >= kHoldButton && id < kHoldButton + 8) {
                return hold_text(static_cast<std::size_t>(id - kHoldButton));
            }
            return std::nullopt;
        }
        const sim::City* city = m_city ? c.world().city(m_city->first, m_city->second) : nullptr;
        if (m_page == Page::Transfer) {
            switch (id) {
                case 45001:
                    return line(158);
                case 45002:
                    return line(160);
                case 45007:
                case 45008:
                    return line(165);
                case 45122:
                    return line(10);
                case 45124:
                    return line(11);
                default:
                    break;
            }
            if (id >= 45003 && id <= 45006) {
                return line(static_cast<std::size_t>(49 + id - 45003));
            }
            if (id >= 45141 && id <= 45144) {
                return line(static_cast<std::size_t>(161 + id - 45141));
            }
            if (id >= 45071 && id <= 45078) {
                return hold_text(static_cast<std::size_t>(id - 45071));
            }
            if (id >= 45031 && id <= 45034 && city != nullptr) {
                const std::uint8_t w = kontor_ware(static_cast<std::int32_t>(id - 45031));
                if (w == 0) {
                    return std::nullopt;
                }
                const std::uint32_t t = city->stock(w) >> 5U;
                return t >= 1
                           ? ui::format_text(c.text("GAME", 50), {std::int64_t{t}, std::string_view(c.text("WARE", w))})
                           : c.text("WARE", w);
            }
            return std::nullopt;
        }
        // Trade.
        const bool natives = city != nullptr && is_natives(*city, c);
        switch (id) {
            case 33001:
                return line(natives ? 47 : 45);
            case 33002:
                return line(natives ? 48 : 46);
            case 33122:
                return line(10);
            case 33124:
                return line(11);
            default:
                break;
        }
        if (id >= 33003 && id <= 33006) {
            return line(static_cast<std::size_t>(49 + id - 33003));
        }
        if (id >= 33031 && id <= 33038 && city != nullptr) {
            const auto k = static_cast<std::size_t>(id - 33031);
            if (k >= m_offers.size()) {
                return std::nullopt;
            }
            const std::uint8_t w = m_offers[k];
            return ui::format_text(c.text("GAME", 50),
                                   {std::int64_t{sellable(*city, w) >> 5U}, std::string_view(c.text("WARE", w))});
        }
        if (id >= 33071 && id <= 33078 && city != nullptr) {
            const auto h = static_cast<std::size_t>(id - 33071);
            if (h >= ship->cargo.size()) {
                return std::nullopt;
            }
            const sim::CargoSlot& slot = ship->cargo[h];
            if (slot.ware == 0 || slot.amount == 0 ||
                (slot.ware >= kFirstSoldierWare && slot.ware <= kLastSoldierWare)) {
                return hold_text(h);
            }
            const std::uint32_t wanted = buyable(*city, slot.ware);
            const sim::City::Trade* t = city->trade_of(slot.ware);
            if (wanted == 0) {
                return ui::format_text(c.text("GAME", t != nullptr && t->buy ? 67 : 68),
                                       {std::int64_t{slot.amount >> 5U}, std::string_view(c.text("WARE", slot.ware))});
            }
            return ui::format_text(c.text("GAME", natives ? 69 : 70),
                                   {std::int64_t{slot.amount >> 5U}, std::string_view(c.text("WARE", slot.ware)),
                                    std::int64_t{wanted >> 5U}, std::int64_t{t != nullptr ? t->buy_price : 0}});
        }
        return std::nullopt;
    }

    bool back() override {
        if (m_page == Page::Root) {
            return false;
        }
        m_page = Page::Root;
        return true;
    }
    std::optional<Request> take_request() override { return std::exchange(m_request, std::nullopt); }

private:
    enum class Page : std::uint8_t { Root, Transfer, Trade };
    static constexpr std::int32_t kName = 38002;
    static constexpr std::int32_t kRepair = 38003;
    static constexpr std::int32_t kSale = 38004;
    static constexpr std::int32_t kTrade = 38005;
    static constexpr std::int32_t kCannonsDown = 38006;
    static constexpr std::int32_t kCannons = 38007;
    static constexpr std::int32_t kHealthFrame = 38008;
    static constexpr std::int32_t kHealth = 38009;
    static constexpr std::int32_t kHealthHover = 38010;
    static constexpr std::int32_t kPriceTrack = 38011;
    static constexpr std::int32_t kPriceSlider = 38012;
    static constexpr std::int32_t kPriceDown = 38013;
    static constexpr std::int32_t kPriceUp = 38014;
    static constexpr std::int32_t kPriceLabel = 38015;
    static constexpr std::int32_t kPrice = 38016;
    static constexpr std::int32_t kFound = 38017;
    static constexpr std::int32_t kTransfer = 38018;
    static constexpr std::int32_t kPirates = 38019;
    static constexpr std::int32_t kRoute = 38020;
    static constexpr std::int32_t kRouteEditor = 38021;
    static constexpr std::int32_t kStop = 38022;
    static constexpr std::int32_t kExplore = 38023;
    static constexpr std::int32_t kExploreRing = 38024;
    static constexpr std::int32_t kScuttle = 38025;
    static constexpr std::int32_t kHoldIcon = 38051;
    static constexpr std::int32_t kHoldBar = 38071;
    static constexpr std::int32_t kHoldButton = 38091;
    static constexpr std::int32_t kDeadSlider = 38111;

    /// What the ship can do where it lies (Ship_ContextActions 0x0044E630).
    struct Context {
        bool foreign = true;  ///< not the local player's ship
        bool moving = false;
        bool own_kontor = false;                ///< at an own Kontor: transfer
        bool foreign_kontor = false;            ///< at another's Kontor with a trade treaty: trade
        bool found = false;                     ///< an island within 4 tiles without an own city
        std::optional<sim::CostBuilding> yard;  ///< an own shipyard in reach
        sim::ExploreState explore = sim::ExploreState::NotAllowed;
        std::optional<std::pair<std::uint16_t, std::uint8_t>> city;  ///< the Kontor's city
    };

    Context context(const sim::Ship& ship, const PanelContext& c) {
        Context k;
        const sim::World& w = c.world();
        k.foreign = ship.owner != c.local;
        const sim::ShipMotion& m = ship.motion;
        k.moving = m.phase == sim::ShipMotion::Phase::Run || m.phase == sim::ShipMotion::Phase::Turn ||
                   (m.path_index < m.path.size());
        k.explore = c.simulation.fleet().explore_state(ship, c.local);
        if (k.foreign || k.moving) {
            return k;
        }
        if (const auto index = sim::kontor_in_reach(w, c.buildings, ship.x, ship.y)) {
            const sim::City& city = w.cities()[*index];
            k.city = std::make_pair(city.island, city.index);
            if (city.owner == ship.owner) {
                k.own_kontor = true;
            } else if (w.diplomacy().may_trade(ship.owner, city.owner)) {  // natives too: by treaty
                k.foreign_kontor = true;
            }
        } else {
            // An island within 4 tiles where the player has no city yet.
            for (int dy = -4; dy <= 4 && !k.found; ++dy) {
                for (int dx = -4; dx <= 4 && !k.found; ++dx) {
                    const std::int32_t x = ship.x + dx;
                    const std::int32_t y = ship.y + dy;
                    if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(w.width()) ||
                        y >= static_cast<std::int32_t>(w.height())) {
                        continue;
                    }
                    const sim::World::Cell& cell = w.cell(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                    if (cell.island == sim::World::kSea) {
                        continue;
                    }
                    const bool own_city = std::any_of(w.cities().begin(), w.cities().end(), [&](const sim::City& ct) {
                        return ct.island == cell.island && ct.owner == ship.owner;
                    });
                    k.found = !own_city;
                }
            }
        }
        for (const sim::CostBuilding& b : w.cost_buildings()) {
            if (b.kind != sim::CostBuilding::Kind::Shipyard || b.island >= w.islands().size()) {
                continue;
            }
            const sim::MapCell& top = w.islands()[b.island].map.top(b.x, b.y);
            if (top.owner == ship.owner && sim::building_in_reach(w, c.buildings, b.island, b.x, b.y, ship.x, ship.y)) {
                k.yard = b;
                break;
            }
        }
        return k;
    }

    [[nodiscard]] static bool is_natives(const sim::City& city, const PanelContext& c) {
        const sim::Player* p = c.world().player(city.owner);
        return p != nullptr && p->role == sim::PlayerRole::Natives;
    }
    [[nodiscard]] static sim::ShipCommand ship_command(const sim::Ship& ship, const PanelContext& c,
                                                       sim::ShipCommand::Op op) {
        sim::ShipCommand o;
        o.player = c.local;
        o.ship = ship.id;
        o.op = op;
        return o;
    }
    [[nodiscard]] std::uint32_t step(const ui::Panel& p, std::int32_t first) const {
        for (std::size_t k = 0; k < kSteps.size(); ++k) {
            if (p.selected(first + static_cast<std::int32_t>(k))) {
                return kSteps[k];
            }
        }
        return kSteps[0];
    }

    void holds(ui::Panel& p, const sim::Ship& ship, const PanelContext& c, std::int32_t icon, std::int32_t bar,
               std::int32_t button, bool shown) {
        const assets::FigureType* f = c.figures.get(ship.figure);
        const std::size_t n = f != nullptr && f->ship.cargo_slots != 0 ? f->ship.cargo_slots : ship.cargo.size();
        for (std::size_t h = 0; h < 8; ++h) {
            const auto k = static_cast<std::int32_t>(h);
            const bool hold = shown && h < n && h < ship.cargo.size();
            p.set_visible(icon + k, hold);
            p.set_visible(button + k, hold);
            const bool filled = hold && ship.cargo[h].ware != 0 && ship.cargo[h].amount != 0;
            p.set_visible(bar + k, filled);
            if (!hold) {
                continue;
            }
            const sim::CargoSlot& slot = ship.cargo[h];
            p.set_graphic(icon + k, filled ? m_gfx_ware + slot.ware : m_gfx_empty, 0);
            if (filled) {
                p.set_slider(bar + k, 1, static_cast<std::int32_t>(std::min(slot.amount, sim::kHoldCapacity)),
                             static_cast<std::int32_t>(sim::kHoldCapacity));
            }
        }
    }

    void refresh_root(const sim::Ship& ship, const Context& k, const PanelContext& c) {
        m_ship.set_text(kName, ship.name);
        const bool own = !k.foreign;
        const bool idle = own && !k.moving;
        m_ship.set_visible(kTrade, idle && k.foreign_kontor);
        m_ship.set_visible(kTransfer, idle && k.own_kontor);
        const assets::FigureType* f = c.figures.get(ship.figure);
        const std::uint32_t max_health = f != nullptr ? std::max<std::uint32_t>(f->ship.max_health, 1) : 1;
        const std::uint32_t health = ship.health == 0 ? max_health : ship.health;
        m_ship.set_visible(kRepair, idle && k.yard.has_value() && health < max_health);
        m_ship.set_selected(kRepair, ship.repairing);
        m_ship.set_visible(kFound, idle && k.found);
        m_ship.set_visible(kStop, own && k.moving);
        const bool running = k.explore == sim::ExploreState::Running;
        m_ship.set_visible(kExplore, own && (k.explore == sim::ExploreState::Possible || running));
        m_ship.set_visible(kExploreRing, own && running);
        if (running) {
            m_ship.set_graphic(kExploreRing, m_gfx_ship + 28 + (11 - std::min<std::int32_t>(ship.explore_steps, 11)),
                               0);
        }
        // Sale: an own ship can be offered; another's only shows its price, and can be bought.
        m_ship.set_visible(kSale, own || ship.for_sale);
        m_ship.set_selected(kSale, ship.for_sale && own);
        for (const std::int32_t id : {kPriceTrack, kPriceSlider, kPriceDown, kPriceUp}) {
            m_ship.set_visible(id, ship.for_sale && own);
        }
        m_ship.set_visible(kPriceLabel, ship.for_sale);
        m_ship.set_visible(kPrice, ship.for_sale);
        if (ship.for_sale) {
            m_ship.set_text(kPriceLabel, c.text("GAME", 10));
            m_ship.set_text(kPrice, "$ " + std::to_string(c.simulation.fleet().sale_price(ship)));
            if (m_ship.held() != kPriceSlider) {
                m_ship.set_slider(kPriceSlider, 0, ship.sale_factor, sim::ShipCommand::kMaxSaleFactor);
            }
        }
        for (const std::int32_t id : {kHealthFrame, kHealth, kHealthHover, kCannonsDown, kCannons}) {
            m_ship.set_visible(id, own);
        }
        m_ship.set_slider(kHealth, 0, static_cast<std::int32_t>(std::uint64_t{health} * 1000 / max_health), 1000);
        m_ship.set_text(kCannons, std::to_string(ship.cannons));
        m_ship.set_disabled(kCannonsDown, ship.cannons == 0);
        m_ship.set_visible(kRoute, own && !ship.for_sale && ship.route.has_stops());
        m_ship.set_selected(kRoute, !ship.route.active);
        holds(m_ship, ship, c, kHoldIcon, kHoldBar, kHoldButton, own || ship.for_sale);
    }

    void clicked_root(const ui::PanelEvent& e, const sim::Ship& ship, const PanelContext& c,
                      std::vector<sim::Command>& out) {
        if (e.kind == ui::PanelEvent::Kind::Slid && e.id == kPriceSlider) {
            const auto factor =
                static_cast<std::uint16_t>(m_ship.slider(kPriceSlider, 0, sim::ShipCommand::kMaxSaleFactor));
            if (factor != ship.sale_factor) {
                sim::ShipCommand o = ship_command(ship, c, sim::ShipCommand::Op::OfferForSale);
                o.factor = factor;
                out.emplace_back(o);
            }
            return;
        }
        if (e.kind != ui::PanelEvent::Kind::Clicked) {
            return;
        }
        const Context k = context(ship, c);
        switch (e.id) {
            case kStop: {
                sim::ShipOrder o;
                o.player = c.local;
                o.ship = ship.id;
                out.emplace_back(o);
                break;
            }
            case kExplore: {
                sim::ExploreOrder o;
                o.player = c.local;
                o.ship = ship.id;
                out.emplace_back(o);
                break;
            }
            case kRoute: {
                sim::RouteEdit o;
                o.player = c.local;
                o.ship = ship.id;
                o.op = ship.route.active ? sim::RouteEdit::Op::Stop : sim::RouteEdit::Op::Start;
                out.emplace_back(o);
                break;
            }
            case kRepair:
                if (k.yard) {
                    sim::ShipyardOrder o;
                    o.player = c.local;
                    o.island = k.yard->island;
                    o.x = k.yard->x;
                    o.y = k.yard->y;
                    o.op = sim::ShipyardOrder::Op::Repair;
                    o.ship = ship.id;
                    o.on = !ship.repairing;
                    out.emplace_back(o);
                }
                break;
            case kSale:
                out.emplace_back(ship_command(
                    ship, c, ship.for_sale ? sim::ShipCommand::Op::WithdrawSale : sim::ShipCommand::Op::OfferForSale));
                break;
            case kPriceDown:
            case kPriceUp: {
                sim::ShipCommand o = ship_command(ship, c, sim::ShipCommand::Op::OfferForSale);
                o.factor = static_cast<std::uint16_t>(std::clamp<int>(ship.sale_factor + (e.id == kPriceUp ? 1 : -1), 0,
                                                                      sim::ShipCommand::kMaxSaleFactor));
                out.emplace_back(o);
                break;
            }
            case kCannonsDown:
                if (ship.cannons > 0) {
                    out.emplace_back(ship_command(ship, c, sim::ShipCommand::Op::UnmountCannon));
                }
                break;
            case kTrade:
            case kTransfer:
                if (k.city) {
                    m_city = k.city;
                    m_page = e.id == kTrade ? Page::Trade : Page::Transfer;
                    // The fullest hold is chosen first (0x0044E390).
                    m_hold = 0;
                    for (std::size_t h = 0; h < ship.cargo.size(); ++h) {
                        if (ship.cargo[h].amount >= ship.cargo[static_cast<std::size_t>(m_hold)].amount) {
                            m_hold = static_cast<std::int32_t>(h);
                        }
                    }
                    m_offer = 0;
                }
                break;
            case kFound:
                m_request = Request{Request::Kind::FoundKontor, ship.id};
                break;
            default:
                if (e.id >= kHoldButton && e.id < kHoldButton + 8) {
                    const auto h = static_cast<std::size_t>(e.id - kHoldButton);
                    if (h >= ship.cargo.size()) {
                        break;
                    }
                    const sim::CargoSlot& slot = ship.cargo[h];
                    if (slot.ware >= kFirstSoldierWare && slot.ware <= kLastSoldierWare && slot.amount != 0) {
                        // Soldiers in the hold go ashore (0x00448F90).
                        sim::UnloadOrder o;
                        o.player = c.local;
                        o.ship = ship.id;
                        o.hold = static_cast<std::uint8_t>(h);
                        out.emplace_back(o);
                    } else if (slot.ware == kCannonWare && slot.amount >= 32) {
                        sim::ShipCommand o = ship_command(ship, c, sim::ShipCommand::Op::MountCannon);
                        o.hold = static_cast<std::uint8_t>(h);
                        out.emplace_back(o);
                    }
                }
                break;
        }
    }

    // Transfer: the wares of the Kontor in four groups, four at a time.
    static std::pair<std::uint8_t, std::uint8_t> category_range(std::int32_t cat) {
        switch (cat) {
            case 0:
                return {22, 24};  // Baumaterial
            case 1:
                return {11, 13};  // Waffen
            case 2:
                return {14, 21};  // Bedarfsgüter
            default:
                return {2, 10};  // Rohstoffe
        }
    }
    static std::int32_t category_of(std::uint8_t w) {
        for (std::int32_t c = 0; c < 4; ++c) {
            const auto [first, last] = category_range(c);
            if (w >= first && w <= last) {
                return c;
            }
        }
        return 0;
    }
    [[nodiscard]] std::int32_t category() const {
        for (std::int32_t k = 0; k < 4; ++k) {
            if (m_transfer.selected(45141 + k)) {
                return k;
            }
        }
        return 0;
    }
    /// The ware on Kontor button k (0..3), 0 if none.
    [[nodiscard]] std::uint8_t kontor_ware(std::int32_t k) const {
        const std::int32_t cat = category();
        const auto [first, last] = category_range(cat);
        const std::int32_t w = first + m_scroll[static_cast<std::size_t>(cat)] + k;
        return w <= last ? static_cast<std::uint8_t>(w) : 0;
    }
    void show_ware(std::uint8_t w) {
        const std::int32_t cat = category_of(w);
        for (std::int32_t k = 0; k < 4; ++k) {
            m_transfer.set_selected(45141 + k, k == cat);
        }
        const auto [first, last] = category_range(cat);
        std::int32_t& scroll = m_scroll[static_cast<std::size_t>(cat)];
        if (last - first + 1 > 4) {
            scroll = std::clamp<std::int32_t>(w - first - 1, 0, last - first - 3);
        }
        for (std::int32_t k = 0; k < 4; ++k) {
            m_transfer.set_selected(45031 + k, kontor_ware(k) == w);
        }
    }

    void refresh_transfer(const sim::Ship& ship, const Context&, const PanelContext& c) {
        const sim::City* city = m_city ? c.world().city(m_city->first, m_city->second) : nullptr;
        if (city == nullptr) {
            m_page = Page::Root;
            return;
        }
        m_transfer.set_text(45122, ship.name);
        m_transfer.set_text(45124, city->name);
        const std::int32_t cat = category();
        const auto [first, last] = category_range(cat);
        const bool scrolls = last - first + 1 > 4;
        const std::int32_t scroll = m_scroll[static_cast<std::size_t>(cat)];
        m_transfer.set_visible(45007, scrolls && scroll > 0);
        m_transfer.set_visible(45008, scrolls && scroll < last - first - 3);
        const auto cap = static_cast<std::int32_t>(std::max<std::uint32_t>(city->capacity, 1));
        for (std::int32_t k = 0; k < 4; ++k) {
            const std::uint8_t w = kontor_ware(k);
            m_transfer.set_visible(45011 + k, w != 0);
            m_transfer.set_visible(45031 + k, w != 0);
            m_transfer.set_visible(45131 + k, w != 0);
            if (w == 0) {
                continue;
            }
            m_transfer.set_graphic(45011 + k, m_gfx_ware + w, 0);
            m_transfer.set_disabled(45011 + k, city->stock(w) == 0);
            m_transfer.set_slider(
                45131 + k, 1, static_cast<std::int32_t>(std::min<std::uint32_t>(city->stock(w), city->capacity)), cap);
        }
        holds(m_transfer, ship, c, 45051, 45091, 45071, true);
        for (std::int32_t h = 0; h < 8; ++h) {
            m_transfer.set_selected(45071 + h, h == m_hold);
        }
    }

    void clicked_transfer(const ui::PanelEvent& e, const sim::Ship& ship, const PanelContext& c,
                          std::vector<sim::Command>& out) {
        if (e.kind != ui::PanelEvent::Kind::Clicked) {
            return;
        }
        const std::uint32_t amount = step(m_transfer, 45003);
        const std::int32_t cat = category();
        const auto [first, last] = category_range(cat);
        std::int32_t& scroll = m_scroll[static_cast<std::size_t>(cat)];
        if (e.id == 45007) {
            scroll = std::max(0, scroll - 1);
        } else if (e.id == 45008) {
            scroll = std::min(scroll + 1, std::max(0, last - first - 3));
        } else if (e.id >= 45071 && e.id <= 45078) {
            m_hold = e.id - 45071;
            const auto h = static_cast<std::size_t>(m_hold);
            if (h < ship.cargo.size() && ship.cargo[h].ware >= 2 && ship.cargo[h].ware <= 24 &&
                ship.cargo[h].amount != 0) {
                show_ware(ship.cargo[h].ware);  // the Kontor shows the same ware
            }
        } else if (e.id == 45001) {
            std::uint8_t w = 0;
            for (std::int32_t k = 0; k < 4; ++k) {
                if (m_transfer.selected(45031 + k)) {
                    w = kontor_ware(k);
                }
            }
            if (w >= 2 && w <= 24) {
                sim::CargoTransfer o;
                o.player = c.local;
                o.ship = ship.id;
                o.ware = w;
                o.to_ship = true;
                o.amount = amount;
                out.emplace_back(o);
            }
        } else if (e.id == 45002) {
            const auto h = static_cast<std::size_t>(m_hold);
            if (h < ship.cargo.size() && ship.cargo[h].ware >= 2 && ship.cargo[h].ware <= 24 &&
                ship.cargo[h].amount != 0) {
                show_ware(ship.cargo[h].ware);
                sim::CargoTransfer o;
                o.player = c.local;
                o.ship = ship.id;
                o.ware = ship.cargo[h].ware;
                o.to_ship = false;
                o.amount = std::min(amount, ship.cargo[h].amount);
                o.hold = static_cast<std::uint8_t>(h);
                out.emplace_back(o);
            }
        }
    }

    // Trade.
    [[nodiscard]] static std::uint32_t sellable(const sim::City& city, std::uint8_t w) {
        const sim::City::Trade* t = city.trade_of(w);
        if (t == nullptr || !t->sell) {
            return 0;
        }
        const std::int64_t n = std::int64_t{city.stock(w)} - t->keep - city.reserved_of(w);
        return n < 32 ? 0 : static_cast<std::uint32_t>(n);
    }
    [[nodiscard]] static std::uint32_t buyable(const sim::City& city, std::uint8_t w) {
        const sim::City::Trade* t = city.trade_of(w);
        if (t == nullptr || !t->buy) {
            return 0;
        }
        const std::int64_t n = std::int64_t{t->limit} - city.stock(w) + city.reserved_of(w);
        return n < 32 ? 0 : static_cast<std::uint32_t>(n);
    }

    void refresh_trade(const sim::Ship& ship, const Context&, const PanelContext& c) {
        const sim::City* city = m_city ? c.world().city(m_city->first, m_city->second) : nullptr;
        if (city == nullptr) {
            m_page = Page::Root;
            return;
        }
        const bool natives = is_natives(*city, c);
        m_trade.set_text(33122, ship.name);
        m_trade.set_text(33124, city->name);
        const assets::BuildingType* kontor = sim::kontor_of(c.world(), c.buildings, *city);
        const std::int32_t places = std::min<std::int32_t>(kontor != nullptr ? kontor->max_wares : 8, 8);
        m_offers.clear();
        for (std::uint8_t w = 2; w <= 24; ++w) {
            if (sellable(*city, w) >= 32) {
                m_offers.push_back(w);
            }
        }
        const auto cap = static_cast<std::int32_t>(std::max<std::uint32_t>(city->capacity, 1));
        for (std::int32_t k = 0; k < 8; ++k) {
            const bool place = k < places;
            const bool ware = place && k < static_cast<std::int32_t>(m_offers.size());
            m_trade.set_visible(33011 + k, place);
            m_trade.set_visible(33031 + k, place);
            m_trade.set_visible(33131 + k, ware);
            m_trade.set_selected(33031 + k, k == m_offer);
            if (ware) {
                const std::uint8_t w = m_offers[static_cast<std::size_t>(k)];
                m_trade.set_graphic(33011 + k, m_gfx_ware + w, 0);
                m_trade.set_slider(
                    33131 + k, 1,
                    static_cast<std::int32_t>(std::min<std::uint32_t>(sellable(*city, w), city->capacity)), cap);
            } else if (place) {
                m_trade.set_graphic(33011 + k, m_gfx_empty, 0);
            }
        }
        const bool offer = m_offer < static_cast<std::int32_t>(m_offers.size());
        m_trade.set_visible(33111, offer);
        m_trade.set_visible(33112, offer && !natives);
        m_trade.set_visible(33113, offer && !natives);
        m_trade.set_disabled(33001, !offer);
        if (offer) {
            const std::uint8_t w = m_offers[static_cast<std::size_t>(m_offer)];
            m_trade.set_text(33111, c.text("WARE", w));
            const sim::City::Trade* t = city->trade_of(w);
            m_trade.set_text(33112, std::to_string(t != nullptr ? t->sell_price : 0));
        }
        holds(m_trade, ship, c, 33051, 33091, 33071, true);
        if (m_hold >= static_cast<std::int32_t>(ship.cargo.size())) {
            m_hold = 0;
        }
        for (std::int32_t h = 0; h < 8; ++h) {
            m_trade.set_selected(33071 + h, h == m_hold);
        }
        const auto h = static_cast<std::size_t>(m_hold);
        const std::uint8_t hw = h < ship.cargo.size() && ship.cargo[h].amount != 0 ? ship.cargo[h].ware : 0;
        const bool wanted = hw >= 2 && hw <= 24 && buyable(*city, hw) >= 32;
        m_trade.set_visible(33114, wanted);
        m_trade.set_visible(33115, wanted && !natives);
        m_trade.set_visible(33116, wanted && !natives);
        m_trade.set_disabled(33002, !wanted);
        if (wanted) {
            const sim::City::Trade* t = city->trade_of(hw);
            m_trade.set_text(33114, c.text("WARE", hw));
            m_trade.set_text(33115, std::to_string(t != nullptr ? t->buy_price : 0));
        }
    }

    void clicked_trade(const ui::PanelEvent& e, const sim::Ship& ship, const PanelContext& c,
                       std::vector<sim::Command>& out) {
        if (e.kind != ui::PanelEvent::Kind::Clicked) {
            return;
        }
        const std::uint32_t amount = step(m_trade, 33003);
        if (e.id >= 33031 && e.id <= 33038) {
            m_offer = e.id - 33031;
        } else if (e.id >= 33071 && e.id <= 33078) {
            m_hold = e.id - 33071;
        } else if (e.id == 33001 && m_offer < static_cast<std::int32_t>(m_offers.size())) {
            sim::CargoTransfer o;
            o.player = c.local;
            o.ship = ship.id;
            o.ware = m_offers[static_cast<std::size_t>(m_offer)];
            o.to_ship = true;
            o.amount = amount;  // the simulation takes what fits, what is offered and what is paid for
            out.emplace_back(o);
        } else if (e.id == 33002) {
            const auto h = static_cast<std::size_t>(m_hold);
            if (h < ship.cargo.size() && ship.cargo[h].ware >= 2 && ship.cargo[h].ware <= 24 &&
                ship.cargo[h].amount != 0) {
                sim::CargoTransfer o;
                o.player = c.local;
                o.ship = ship.id;
                o.ware = ship.cargo[h].ware;
                o.to_ship = false;
                o.amount = std::min(amount, ship.cargo[h].amount);
                o.hold = static_cast<std::uint8_t>(h);
                out.emplace_back(o);
            }
        }
    }

    ui::Panel m_ship;
    ui::Panel m_transfer;
    ui::Panel m_trade;
    std::int32_t m_gfx_ware = 192;
    std::int32_t m_gfx_empty = 189;
    std::int32_t m_gfx_ship = 363;
    Page m_page = Page::Root;
    std::uint32_t m_last_ship = 0;
    std::int32_t m_hold = 0;
    std::int32_t m_offer = 0;
    std::array<std::int32_t, 4> m_scroll{};
    std::vector<std::uint8_t> m_offers;
    std::optional<std::pair<std::uint16_t, std::uint8_t>> m_city;
    std::optional<Request> m_request;
};

}  // namespace

// ---- The set -------------------------------------------------------------------------------------

SelectionPanels::SelectionPanels(const assets::GameDirectory& game) {
    const auto load = [&](const char* file) -> std::optional<assets::GadgetLayout> {
        auto l = assets::load_gad(game, file);
        return l ? std::optional<assets::GadgetLayout>(std::move(l).value()) : std::nullopt;
    };
    if (auto l = load("PROD.GAD")) {
        const std::int32_t ware = l->constant("GFXWARE").value_or(192);
        m_panels["PROD"] = std::make_unique<ProductionPanel>(std::move(*l), kProd, ware);
    }
    if (auto l = load("PLANTAGE.GAD")) {
        const std::int32_t ware = l->constant("GFXWARE").value_or(192);
        m_panels["PLANTAGE"] = std::make_unique<ProductionPanel>(std::move(*l), kPlantage, ware);
    }
    if (auto l = load("BERGWERK.GAD")) {
        const std::int32_t ware = l->constant("GFXWARE").value_or(192);
        m_panels["BERGWERK"] = std::make_unique<ProductionPanel>(std::move(*l), kMine, ware);
    }
    if (auto stadt = load("STADT.GAD")) {
        auto kontor = load("KONTOR.GAD");
        auto lager = load("LAGER.GAD");
        if (kontor && lager) {
            m_panels["STADT"] = std::make_unique<CityPanel>(std::move(*stadt), std::move(*kontor), std::move(*lager));
        }
    }
    if (auto ship = load("SHIP.GAD")) {
        auto transfer = load("TRANSFER.GAD");
        auto trade = load("TRADE.GAD");
        if (transfer && trade) {
            m_panels["SHIP"] = std::make_unique<ShipPanel>(std::move(*ship), std::move(*transfer), std::move(*trade));
        }
    }
    if (auto werft = load("WERFT.GAD")) {
        if (auto bauship = load("BAUSHIP.GAD")) {
            m_panels["WERFT"] = std::make_unique<ShipyardPanel>(std::move(*werft), std::move(*bauship));
        }
    }
    if (auto l = load("MILITAR.GAD")) {
        m_panels["MILITAR"] = std::make_unique<CastlePanel>(std::move(*l));
    }
    if (auto l = load("HANDEL.GAD")) {
        m_panels["HANDEL"] = std::make_unique<ForeignKontorPanel>(std::move(*l));
    }
    if (auto l = load("SIEDLER.GAD")) {
        m_panels["SIEDLER"] = std::make_unique<SettlerPanel>(std::move(*l));
    }
    if (auto l = load("INFRA.GAD")) {
        const std::int32_t bauhaus = l->constant("GFXBAUHAUS").value_or(81);
        m_panels["INFRA"] = std::make_unique<InfraPanel>(std::move(*l), bauhaus);
    }
}

void SelectionPanels::set_scale(double scale) {
    for (auto& [name, panel] : m_panels) {
        panel->set_scale(scale);
    }
}

SelectionPanel* SelectionPanels::panel_for(const Selection& s, const PanelContext& c) {
    const auto get = [&](const char* name) -> SelectionPanel* {
        const auto it = m_panels.find(name);
        return it != m_panels.end() ? it->second.get() : nullptr;
    };
    if (s.kind == Selection::Kind::Ship) {
        return c.world().ship(s.ship) != nullptr ? get("SHIP") : nullptr;
    }
    if (s.kind != Selection::Kind::Building) {
        return nullptr;
    }
    const assets::BuildingType* b = building_at(s, c);
    if (b == nullptr) {
        return nullptr;
    }
    const sim::MapCell& top = c.world().islands()[s.island].map.top(s.x, s.y);
    if (top.owner != c.local) {
        // Foreign buildings: only Kontor and market, and only with a trade treaty.
        const sim::City* city = city_at(s, c);
        if ((b->prod_kind == 7 || b->prod_kind == 8) && city != nullptr && c.local != sim::kNoPlayer &&
            c.world().diplomacy().may_trade(c.local, city->owner)) {
            return get("HANDEL");
        }
        return nullptr;
    }
    // Panel_UpdateSelection 0x0043AA00: the building's HAUS_PRODTYP Kind decides.
    switch (b->prod_kind) {
        case 1:
            return c.world().production(s.island, s.x, s.y) != nullptr ? get("PROD") : nullptr;
        case 2:
        case 4:
        case 5:
        case 6:
            return c.world().production(s.island, s.x, s.y) != nullptr ? get("PLANTAGE") : nullptr;
        case 3:
            return c.world().production(s.island, s.x, s.y) != nullptr ? get("BERGWERK") : nullptr;
        default:
            break;
    }
    if (b->prod_kind >= 0x11 && b->prod_kind <= 0x1B) {
        return get("INFRA");
    }
    if (b->prod_kind == 14 && cost_building_at(s, c, sim::CostBuilding::Kind::Shipyard) != nullptr) {
        return get("WERFT");
    }
    if (b->prod_kind == 15 && cost_building_at(s, c, sim::CostBuilding::Kind::Castle) != nullptr &&
        city_at(s, c) != nullptr) {
        return get("MILITAR");
    }
    if (b->prod_kind == 13 && c.world().house(s.island, s.x, s.y) != nullptr && city_at(s, c) != nullptr) {
        return get("SIEDLER");
    }
    if ((b->prod_kind == 7 || b->prod_kind == 8) && city_at(s, c) != nullptr) {
        return get("STADT");  // market or Kontor
    }
    return nullptr;
}

}  // namespace o1602::app
