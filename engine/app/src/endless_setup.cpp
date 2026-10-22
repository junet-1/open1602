// SPDX-License-Identifier: GPL-3.0-or-later
#include "endless_setup.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <utility>

#include "o1602/sim/player.hpp"
#include "o1602/ui/localization.hpp"

namespace o1602::app {

namespace {

using Amount = sim::WorldSettings::Amount;
using Sizes = sim::WorldSettings::Sizes;
using Starts = sim::WorldSettings::Starts;

// The values the dialog offers.
constexpr std::array<std::uint8_t, 3> kRefundPercents = {25, 50, 75};  // between none and full
constexpr std::array<std::uint32_t, 4> kShips = {33, 100, 200, 0};     // 0: unlimited
constexpr std::array<std::uint32_t, 4> kUnits = {100, 250, 500, 0};
constexpr std::array<std::uint32_t, 2> kSpeeds = {4, 8};
/// Map sizes: smaller than the original's, its 500 x 350, then larger ones of the same shape.
constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 5> kSizes = {
    {{350, 250}, {500, 350}, {700, 490}, {1000, 700}, {1400, 980}}};
constexpr std::uint8_t kFewestPlayers = 2;
constexpr std::uint8_t kMostPlayers = 8;
constexpr std::array<Amount, 3> kIslandAmounts = {Amount::Few, Amount::Normal, Amount::Many};
constexpr std::array<Amount, 4> kAmounts = {Amount::None, Amount::Few, Amount::Normal, Amount::Many};

/// The index of the value nearest to `v` (0 counts as the largest: unlimited).
template <std::size_t N>
std::size_t nearest(const std::array<std::uint32_t, N>& values, std::uint32_t v) {
    if (v == 0) {
        for (std::size_t i = 0; i < N; ++i) {
            if (values[i] == 0) {
                return i;
            }
        }
    }
    std::size_t best = 0;
    std::uint64_t best_distance = UINT64_MAX;
    for (std::size_t i = 0; i < N; ++i) {
        if (values[i] == 0) {
            continue;
        }
        const std::uint64_t d = values[i] > v ? values[i] - v : v - values[i];
        if (d < best_distance) {
            best = i;
            best_distance = d;
        }
    }
    return best;
}

template <class T, std::size_t N>
std::size_t index_of(const std::array<T, N>& values, T v) {
    for (std::size_t i = 0; i < N; ++i) {
        if (values[i] == v) {
            return i;
        }
    }
    return 0;
}

std::string number_text(std::uint32_t v) {
    return v == 0 ? ui::tr("setup.unlimited") : std::to_string(v);
}

std::string amount_text(Amount a) {
    switch (a) {
        case Amount::None:
            return ui::tr("setup.amount.none");
        case Amount::Few:
            return ui::tr("setup.amount.few");
        case Amount::Normal:
            break;
        case Amount::Many:
            return ui::tr("setup.amount.many");
    }
    return ui::tr("setup.amount.normal");
}

std::optional<std::uint32_t> parse_number(std::string_view v) {
    std::uint32_t n = 0;
    const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
    return ec == std::errc{} && end == v.data() + v.size() && !v.empty() ? std::optional(n) : std::nullopt;
}

// Names of the values in settings.cfg.
constexpr std::array<std::string_view, 5> kDroughtNames = {"game", "off", "rare", "normal", "frequent"};
constexpr std::array<std::string_view, 3> kVolcanoNames = {"game", "off", "on"};
constexpr std::array<std::string_view, 4> kAmountNames = {"none", "few", "normal", "many"};
constexpr std::array<std::string_view, 3> kSizeNames = {"small", "mixed", "large"};
constexpr std::array<std::string_view, 2> kStartNames = {"spread", "together"};

template <std::size_t N>
std::optional<std::size_t> name_index(const std::array<std::string_view, N>& names, std::string_view v) {
    for (std::size_t i = 0; i < N; ++i) {
        if (names[i] == v) {
            return i;
        }
    }
    return std::nullopt;
}

std::string_view on_off(bool b) {
    return b ? "on" : "off";
}

}  // namespace

EndlessSetup EndlessSetup::original() {
    EndlessSetup s;
    s.rules = sim::Rules::original();
    return s;
}

bool operator==(const EndlessSetup& a, const EndlessSetup& b) noexcept {
    const sim::WorldSettings& x = a.world;
    const sim::WorldSettings& y = b.world;
    return a.rules == b.rules && a.own_world == b.own_world && x.width == y.width && x.height == y.height &&
           x.players == y.players && x.islands == y.islands && x.sizes == y.sizes && x.natives == y.natives &&
           x.volcanoes == y.volcanoes && x.starts == y.starts;
}

std::size_t setup_choices(SetupItem item) noexcept {
    switch (item) {
        case SetupItem::Refund:
            return kRefundPercents.size() + 2;
        case SetupItem::Ships:
            return kShips.size();
        case SetupItem::Units:
            return kUnits.size();
        case SetupItem::GameSpeed:
            return kSpeeds.size();
        case SetupItem::Droughts:
            return kDroughtNames.size();
        case SetupItem::Volcanoes:
            return kVolcanoNames.size();
        case SetupItem::WorldSize:
            return kSizes.size();
        case SetupItem::Players:
            return kMostPlayers - kFewestPlayers + 1;
        case SetupItem::Islands:
            return kIslandAmounts.size();
        case SetupItem::IslandSizes:
            return kSizeNames.size();
        case SetupItem::Villages:
        case SetupItem::VolcanoIslands:
            return kAmounts.size();
        case SetupItem::TraderPrices:
        case SetupItem::Map:
        case SetupItem::Starts:
        case SetupItem::Resources:
        case SetupItem::ClassicAi:
            return 2;
        case SetupItem::Count:
            break;
    }
    return 0;
}

std::size_t setup_choice(const EndlessSetup& setup, SetupItem item) noexcept {
    const sim::Rules& r = setup.rules;
    const sim::WorldSettings& w = setup.world;
    switch (item) {
        case SetupItem::Refund: {
            switch (r.demolish_refund) {
                case sim::Refund::None:
                    return 0;
                case sim::Refund::Full:
                    return kRefundPercents.size() + 1;
                case sim::Refund::Partial:
                    break;
            }
            std::array<std::uint32_t, kRefundPercents.size()> percents{};
            std::copy(kRefundPercents.begin(), kRefundPercents.end(), percents.begin());
            return 1 + nearest(percents, std::max<std::uint32_t>(r.refund_percent, 1));
        }
        case SetupItem::TraderPrices:
            return r.trader_prices == sim::TraderPrices::Market ? 1 : 0;
        case SetupItem::Ships:
            return nearest(kShips, r.max_ships_per_player);
        case SetupItem::Units:
            return nearest(kUnits, r.max_army);
        case SetupItem::GameSpeed:
            return nearest(kSpeeds, r.max_game_speed);
        case SetupItem::Droughts:
            return std::min<std::size_t>(static_cast<std::size_t>(r.droughts), kDroughtNames.size() - 1);
        case SetupItem::Volcanoes:
            return std::min<std::size_t>(static_cast<std::size_t>(r.volcanoes), kVolcanoNames.size() - 1);
        case SetupItem::Map:
            return setup.own_world ? 1 : 0;
        case SetupItem::WorldSize: {
            std::array<std::uint32_t, kSizes.size()> widths{};
            for (std::size_t i = 0; i < kSizes.size(); ++i) {
                widths[i] = kSizes[i].first;
            }
            return nearest(widths, std::max<std::uint32_t>(w.width, 1));
        }
        case SetupItem::Players:
            return static_cast<std::size_t>(std::clamp(w.players, kFewestPlayers, kMostPlayers) - kFewestPlayers);
        case SetupItem::Islands:
            return w.islands == Amount::None ? 0 : index_of(kIslandAmounts, w.islands);
        case SetupItem::IslandSizes:
            return std::min<std::size_t>(static_cast<std::size_t>(w.sizes), kSizeNames.size() - 1);
        case SetupItem::Villages:
            return index_of(kAmounts, w.natives);
        case SetupItem::VolcanoIslands:
            return index_of(kAmounts, w.volcanoes);
        case SetupItem::Starts:
            return w.starts == Starts::Together ? 1 : 0;
        case SetupItem::Resources:
            return r.guarantee_resources ? 0 : 1;
        case SetupItem::ClassicAi:
            return r.classic_ai ? 1 : 0;
        case SetupItem::Count:
            break;
    }
    return 0;
}

void choose(EndlessSetup& setup, SetupItem item, std::size_t choice) {
    if (choice >= setup_choices(item)) {
        return;
    }
    sim::Rules& r = setup.rules;
    sim::WorldSettings& w = setup.world;
    switch (item) {
        case SetupItem::Refund:
            if (choice == 0) {
                r.demolish_refund = sim::Refund::None;
            } else if (choice > kRefundPercents.size()) {
                r.demolish_refund = sim::Refund::Full;
            } else {
                r.demolish_refund = sim::Refund::Partial;
                r.refund_percent = kRefundPercents[choice - 1];
            }
            break;
        case SetupItem::TraderPrices:
            r.trader_prices = choice == 1 ? sim::TraderPrices::Market : sim::TraderPrices::Original;
            break;
        case SetupItem::Ships:
            r.max_ships_per_player = kShips[choice];
            break;
        case SetupItem::Units:
            r.max_army = kUnits[choice];
            break;
        case SetupItem::GameSpeed:
            r.max_game_speed = kSpeeds[choice];
            break;
        case SetupItem::Droughts:
            r.droughts = static_cast<sim::Droughts>(choice);
            break;
        case SetupItem::Volcanoes:
            r.volcanoes = static_cast<sim::Volcanoes>(choice);
            break;
        case SetupItem::Map:
            setup.own_world = choice == 1;
            break;
        case SetupItem::WorldSize:
            w.width = kSizes[choice].first;
            w.height = kSizes[choice].second;
            break;
        case SetupItem::Players:
            w.players = static_cast<std::uint8_t>(kFewestPlayers + choice);
            break;
        case SetupItem::Islands:
            w.islands = kIslandAmounts[choice];
            break;
        case SetupItem::IslandSizes:
            w.sizes = static_cast<Sizes>(choice);
            break;
        case SetupItem::Villages:
            w.natives = kAmounts[choice];
            break;
        case SetupItem::VolcanoIslands:
            w.volcanoes = kAmounts[choice];
            break;
        case SetupItem::Starts:
            w.starts = choice == 1 ? Starts::Together : Starts::Spread;
            break;
        case SetupItem::Resources:
            r.guarantee_resources = choice == 0;
            break;
        case SetupItem::ClassicAi:
            r.classic_ai = choice == 1;
            break;
        case SetupItem::Count:
            break;
    }
}

void choose_next(EndlessSetup& setup, SetupItem item) {
    const std::size_t n = setup_choices(item);
    if (n != 0) {
        choose(setup, item, (setup_choice(setup, item) + 1) % n);
    }
}

bool setup_item_used(const EndlessSetup& setup, SetupItem item) noexcept {
    switch (item) {
        case SetupItem::WorldSize:
        case SetupItem::Players:
        case SetupItem::Islands:
        case SetupItem::IslandSizes:
        case SetupItem::Villages:
        case SetupItem::VolcanoIslands:
        case SetupItem::Starts:
            return setup.own_world;
        default:
            return item != SetupItem::Count;
    }
}

std::string setup_label(SetupItem item) {
    static constexpr std::array<std::string_view, kSetupItems> kKeys = {
        "setup.refund",   "setup.trader",          "setup.ships",     "setup.units",
        "setup.speed",    "setup.droughts",        "setup.volcanoes", "setup.map",
        "setup.size",     "setup.players",         "setup.islands",   "setup.island_sizes",
        "setup.villages", "setup.volcano_islands", "setup.starts",    "setup.resources",
        "setup.ai"};
    const auto i = static_cast<std::size_t>(item);
    return i < kKeys.size() ? ui::tr(kKeys[i]) : std::string{};
}

std::string setup_value(SetupItem item, std::size_t choice) {
    if (choice >= setup_choices(item)) {
        return {};
    }
    switch (item) {
        case SetupItem::Refund:
            if (choice == 0) {
                return ui::tr("setup.refund.none");
            }
            if (choice > kRefundPercents.size()) {
                return ui::tr("setup.refund.full");
            }
            return ui::tr("setup.refund.partial", {std::to_string(kRefundPercents[choice - 1])});
        case SetupItem::TraderPrices:
            return ui::tr(choice == 1 ? "setup.trader.market" : "setup.trader.original");
        case SetupItem::Ships:
            return number_text(kShips[choice]);
        case SetupItem::Units:
            return number_text(kUnits[choice]);
        case SetupItem::GameSpeed:
            return ui::tr("setup.speed.value", {std::to_string(kSpeeds[choice])});
        case SetupItem::Droughts: {
            static constexpr std::array<std::string_view, 5> kKeys = {"setup.droughts.game", "setup.off",
                                                                      "setup.droughts.rare", "setup.droughts.normal",
                                                                      "setup.droughts.frequent"};
            return ui::tr(kKeys[choice]);
        }
        case SetupItem::Volcanoes: {
            static constexpr std::array<std::string_view, 3> kKeys = {"setup.volcanoes.game", "setup.off", "setup.on"};
            return ui::tr(kKeys[choice]);
        }
        case SetupItem::Map:
            return ui::tr(choice == 1 ? "setup.map.own" : "setup.map.original");
        case SetupItem::WorldSize:
            return ui::tr("setup.size.value",
                          {std::to_string(kSizes[choice].first), std::to_string(kSizes[choice].second)});
        case SetupItem::Players:
            return std::to_string(kFewestPlayers + choice);
        case SetupItem::Islands:
            return amount_text(kIslandAmounts[choice]);
        case SetupItem::IslandSizes: {
            static constexpr std::array<std::string_view, 3> kKeys = {
                "setup.island_sizes.small", "setup.island_sizes.mixed", "setup.island_sizes.large"};
            return ui::tr(kKeys[choice]);
        }
        case SetupItem::Villages:
        case SetupItem::VolcanoIslands:
            return amount_text(kAmounts[choice]);
        case SetupItem::Starts:
            return ui::tr(choice == 1 ? "setup.starts.together" : "setup.starts.spread");
        case SetupItem::Resources:
            return ui::tr(choice == 0 ? "setup.on" : "setup.off");
        case SetupItem::ClassicAi:
            return ui::tr(choice == 1 ? "setup.ai.classic" : "setup.ai.open1602");
        case SetupItem::Count:
            break;
    }
    return {};
}

bool read_setup_line(EndlessSetup& setup, std::string_view key, std::string_view value) {
    sim::Rules& r = setup.rules;
    sim::WorldSettings& w = setup.world;
    const auto n = parse_number(value);
    if (key == "refund") {
        // 0: none, 100: full, else a share in percent.
        if (n && *n == 0) {
            r.demolish_refund = sim::Refund::None;
        } else if (n && *n >= 100) {
            r.demolish_refund = sim::Refund::Full;
        } else if (n) {
            r.demolish_refund = sim::Refund::Partial;
            r.refund_percent = static_cast<std::uint8_t>(*n);
        }
    } else if (key == "trader_prices") {
        if (value == "market" || value == "original") {
            r.trader_prices = value == "market" ? sim::TraderPrices::Market : sim::TraderPrices::Original;
        }
    } else if (key == "ships") {
        r.max_ships_per_player = n.value_or(r.max_ships_per_player);
    } else if (key == "units") {
        r.max_army = n.value_or(r.max_army);
    } else if (key == "game_speed") {
        if (n && *n >= 1 && *n <= 64) {
            r.max_game_speed = *n;
        }
    } else if (key == "droughts") {
        if (const auto i = name_index(kDroughtNames, value)) {
            r.droughts = static_cast<sim::Droughts>(*i);
        }
    } else if (key == "volcanoes") {
        if (const auto i = name_index(kVolcanoNames, value)) {
            r.volcanoes = static_cast<sim::Volcanoes>(*i);
        }
    } else if (key == "resources") {
        if (value == "on" || value == "off") {
            r.guarantee_resources = value == "on";
        }
    } else if (key == "ai") {
        if (value == "classic" || value == "open1602") {
            r.classic_ai = value == "classic";
        }
    } else if (key == "own_world") {
        if (value == "on" || value == "off") {
            setup.own_world = value == "on";
        }
    } else if (key == "world") {
        const std::size_t x = value.find('x');
        const auto width = x == std::string_view::npos ? std::nullopt : parse_number(value.substr(0, x));
        const auto height = x == std::string_view::npos ? std::nullopt : parse_number(value.substr(x + 1));
        constexpr auto lo = sim::WorldSettings::kMinSide;
        constexpr auto hi = sim::WorldSettings::kMaxSide;
        if (width && height && *width >= lo && *width <= hi && *height >= lo && *height <= hi) {
            w.width = *width;
            w.height = *height;
        }
    } else if (key == "players") {
        if (n && *n >= 1 && *n <= sim::kMaxPlayers) {
            w.players = static_cast<std::uint8_t>(*n);
        }
    } else if (key == "islands" || key == "villages" || key == "volcano_islands") {
        if (const auto i = name_index(kAmountNames, value); i && !(key == "islands" && *i == 0)) {
            (key == "islands" ? w.islands : key == "villages" ? w.natives : w.volcanoes) = kAmounts[*i];
        }
    } else if (key == "island_sizes") {
        if (const auto i = name_index(kSizeNames, value)) {
            w.sizes = static_cast<Sizes>(*i);
        }
    } else if (key == "starts") {
        if (const auto i = name_index(kStartNames, value)) {
            w.starts = *i == 1 ? Starts::Together : Starts::Spread;
        }
    } else {
        return false;
    }
    return true;
}

std::string write_setup(const EndlessSetup& setup) {
    const sim::Rules& r = setup.rules;
    const sim::WorldSettings& w = setup.world;
    std::string out;
    const auto line = [&out](std::string_view key, std::string_view value) {
        out += "endless.";
        out += key;
        out += " = ";
        out += value;
        out += '\n';
    };
    line("refund", std::to_string(r.refund_share()));
    line("trader_prices", r.trader_prices == sim::TraderPrices::Market ? "market" : "original");
    line("ships", std::to_string(r.max_ships_per_player));
    line("units", std::to_string(r.max_army));
    line("game_speed", std::to_string(r.max_game_speed));
    line("droughts", kDroughtNames[std::min<std::size_t>(static_cast<std::size_t>(r.droughts), 4)]);
    line("volcanoes", kVolcanoNames[std::min<std::size_t>(static_cast<std::size_t>(r.volcanoes), 2)]);
    line("resources", on_off(r.guarantee_resources));
    line("ai", r.classic_ai ? "classic" : "open1602");
    line("own_world", on_off(setup.own_world));
    line("world", std::to_string(w.width) + "x" + std::to_string(w.height));
    line("players", std::to_string(w.players));
    line("islands", kAmountNames[static_cast<std::size_t>(w.islands)]);
    line("island_sizes", kSizeNames[std::min<std::size_t>(static_cast<std::size_t>(w.sizes), 2)]);
    line("villages", kAmountNames[static_cast<std::size_t>(w.natives)]);
    line("volcano_islands", kAmountNames[static_cast<std::size_t>(w.volcanoes)]);
    line("starts", kStartNames[w.starts == Starts::Together ? 1 : 0]);
    return out;
}

}  // namespace o1602::app
