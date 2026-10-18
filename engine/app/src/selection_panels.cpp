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
                m_panel.set_hidden(id, !on);
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
            m_panel.set_hidden(62021, false);
        } else {
            m_panel.set_hidden(62021, true);
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
        m_stadt.set_hidden(city_ids::kTrader, true);
        m_lager.set_hidden(city_ids::kLDeadSlider, true);
        m_lager.set_hidden(44134, true);  // marker and corner of the 24th place: never shown
        m_lager.set_hidden(44164, true);
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
            m_kontor.set_hidden(city_ids::kKIcon + k, !slot);
            m_kontor.set_hidden(city_ids::kKFrame + k, !slot);
            m_kontor.set_hidden(city_ids::kKBar + k, !ware);
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
            m_kontor.set_hidden(id, !chosen);
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
            m_lager.set_hidden(city_ids::kLMarker + w, !buy && !sell);
            m_lager.set_hidden(city_ids::kLCorner + w, !buy && !sell);
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
        m_lager.set_hidden(city_ids::kLIcon + last, !picker);
        m_lager.set_hidden(city_ids::kLFrame + last, !picker);
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
            m_panel.set_hidden(kDemandIcon + id, !on);
            m_panel.set_hidden(kDemandBar + id, !on);
            m_panel.set_hidden(kDemandFrame + id, !on);
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
            m_panel.set_hidden(id, !on);
            m_panel.set_hidden(kServiceHover + static_cast<std::int32_t>(k), !on);
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
    if (s.kind != Selection::Kind::Building) {
        return nullptr;
    }
    const assets::BuildingType* b = building_at(s, c);
    if (b == nullptr) {
        return nullptr;
    }
    const sim::MapCell& top = c.world().islands()[s.island].map.top(s.x, s.y);
    if (top.owner != c.local) {
        return nullptr;  // foreign buildings: only Kontor and market with a trade treaty (HANDEL, later)
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
    if (b->prod_kind == 13 && c.world().house(s.island, s.x, s.y) != nullptr && city_at(s, c) != nullptr) {
        return get("SIEDLER");
    }
    if ((b->prod_kind == 7 || b->prod_kind == 8) && city_at(s, c) != nullptr) {
        return get("STADT");  // market or Kontor
    }
    return nullptr;
}

}  // namespace o1602::app
