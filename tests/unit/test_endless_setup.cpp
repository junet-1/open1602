// SPDX-License-Identifier: GPL-3.0-or-later
// The start dialog of the endless game (ADR 0008): its items and values, the presets, the lines
// in settings.cfg and the screen at work.

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../test_framework.hpp"
#include "app_settings.hpp"
#include "endless_setup.hpp"
#include "game_settings_menu.hpp"

#include "o1602/ui/localization.hpp"

using namespace o1602;
using app::EndlessSetup;
using app::SetupItem;

namespace {

constexpr std::array<const ui::Font*, 3> kNoFonts = {nullptr, nullptr, nullptr};

std::optional<std::pair<std::int32_t, std::int32_t>> find_text(app::GameSettingsMenu& menu, const std::string& text) {
    for (const ui::DrawItem& d : menu.draw(kNoFonts)) {
        if (d.kind == ui::DrawItem::Kind::Text && d.text == text) {
            return std::pair{d.x, d.y};
        }
    }
    return std::nullopt;
}

app::GameSettingsMenu::Result click(app::GameSettingsMenu& menu, const std::string& text) {
    const auto at = find_text(menu, text);
    REQUIRE(at.has_value());
    (void)menu.mouse(at->first, at->second - 4, true);
    return menu.mouse(at->first, at->second - 4, false);
}

std::string value_of(const EndlessSetup& s, SetupItem item) {
    return app::setup_value(item, app::setup_choice(s, item));
}

}  // namespace

TEST_CASE("endless setup: every item goes round its values and back") {
    for (std::size_t i = 0; i < app::kSetupItems; ++i) {
        const auto item = static_cast<SetupItem>(i);
        EndlessSetup s;
        const std::size_t n = app::setup_choices(item);
        REQUIRE(n >= 2);
        const std::size_t start = app::setup_choice(s, item);
        std::vector<std::string> seen;
        for (std::size_t k = 0; k < n; ++k) {
            seen.push_back(value_of(s, item));
            app::choose_next(s, item);
        }
        CHECK(app::setup_choice(s, item) == start);
        for (std::size_t a = 0; a < seen.size(); ++a) {
            CHECK(!seen[a].empty());
            for (std::size_t b = a + 1; b < seen.size(); ++b) {
                CHECK(seen[a] != seen[b]);  // each value has a text of its own
            }
        }
        CHECK(!app::setup_label(item).empty());
    }
}

TEST_CASE("endless setup: the values reach the rules and the world") {
    EndlessSetup s;
    app::choose(s, SetupItem::Refund, 2);
    CHECK(s.rules.demolish_refund == sim::Refund::Partial);
    CHECK(s.rules.refund_share() == 50);
    app::choose(s, SetupItem::Refund, 4);
    CHECK(s.rules.refund_share() == 100);
    app::choose(s, SetupItem::Ships, 3);
    CHECK(s.rules.max_ships_per_player == 0);  // unlimited
    app::choose(s, SetupItem::Droughts, 4);
    CHECK(s.rules.droughts == sim::Droughts::Frequent);
    app::choose(s, SetupItem::Volcanoes, 1);
    CHECK(s.rules.volcanoes == sim::Volcanoes::Off);
    app::choose(s, SetupItem::TraderPrices, 1);
    CHECK(s.rules.trader_prices == sim::TraderPrices::Market);
    app::choose(s, SetupItem::Resources, 1);
    CHECK(!s.rules.guarantee_resources);
    CHECK(!s.rules.classic_ai);  // Open1602's computer players by default
    app::choose(s, SetupItem::ClassicAi, 1);
    CHECK(s.rules.classic_ai);

    // The world's items count only for a world of its own.
    CHECK(!app::setup_item_used(s, SetupItem::Players));
    CHECK(app::setup_item_used(s, SetupItem::Resources));
    app::choose(s, SetupItem::Map, 1);
    CHECK(s.own_world);
    CHECK(app::setup_item_used(s, SetupItem::Players));
    app::choose(s, SetupItem::Players, 4);
    CHECK(s.world.players == 6);
    app::choose(s, SetupItem::WorldSize, 3);
    CHECK(s.world.width == 1000);
    CHECK(s.world.height == 700);
    app::choose(s, SetupItem::Villages, 0);
    CHECK(s.world.natives == sim::WorldSettings::Amount::None);
    app::choose(s, SetupItem::Starts, 1);
    CHECK(s.world.starts == sim::WorldSettings::Starts::Together);
    app::choose(s, SetupItem::Islands, 99);  // no such value: unchanged
    CHECK(s.world.islands == sim::WorldSettings::Amount::Normal);

    // A setting between the offered values shows the nearest.
    EndlessSetup odd;
    odd.rules.max_ships_per_player = 150;
    odd.rules.demolish_refund = sim::Refund::Partial;
    odd.rules.refund_percent = 60;
    CHECK(app::setup_choice(odd, SetupItem::Ships) == 1);   // 100
    CHECK(app::setup_choice(odd, SetupItem::Refund) == 2);  // 50 %
}

TEST_CASE("endless setup: the original's rules and Open1602's defaults") {
    const EndlessSetup original = EndlessSetup::original();
    CHECK(original.rules == sim::Rules::original());
    CHECK(!original.own_world);
    CHECK(EndlessSetup{}.rules == sim::Rules::open1602());
    CHECK(!(original == EndlessSetup{}));
}

TEST_CASE("endless setup: settings.cfg keeps the last choice") {
    app::AppSettings a;
    app::choose(a.endless, SetupItem::Refund, 1);
    app::choose(a.endless, SetupItem::Units, 3);
    app::choose(a.endless, SetupItem::GameSpeed, 0);
    app::choose(a.endless, SetupItem::Droughts, 2);
    app::choose(a.endless, SetupItem::Map, 1);
    app::choose(a.endless, SetupItem::WorldSize, 4);
    app::choose(a.endless, SetupItem::Players, 6);
    app::choose(a.endless, SetupItem::IslandSizes, 2);
    app::choose(a.endless, SetupItem::VolcanoIslands, 3);
    const std::string text = app::write_settings(a);
    CHECK(text.find("endless.world = 1400x980\n") != std::string::npos);
    const app::AppSettings back = app::parse_settings(text, app::AppSettings{});
    CHECK(back.endless == a.endless);
    CHECK(back.endless.rules.refund_percent == 25);

    // Wrong values keep the setting; other keys are no business of the setup.
    EndlessSetup s;
    CHECK(app::read_setup_line(s, "world", "100x100"));
    CHECK(s.world.width == 500);
    CHECK(app::read_setup_line(s, "islands", "none"));  // an endless game needs islands
    CHECK(s.world.islands == sim::WorldSettings::Amount::Normal);
    CHECK(app::read_setup_line(s, "droughts", "always"));
    CHECK(s.rules.droughts == sim::Droughts::Game);
    CHECK(app::read_setup_line(s, "players", "40"));
    CHECK(s.world.players == 4);
    CHECK(!app::read_setup_line(s, "colour", "red"));
    CHECK(s == EndlessSetup{});
}

TEST_CASE("endless setup: the dialog chooses, resets and starts") {
    app::GameSettingsMenu menu;
    menu.open(EndlessSetup{});
    // A click on a value takes the next one.
    CHECK(click(menu, value_of(menu.setup(), SetupItem::Refund)) == app::GameSettingsMenu::Result::None);
    CHECK(menu.setup().rules.demolish_refund == sim::Refund::Partial);
    // The world's buttons do nothing on the original's maps.
    const std::string players = value_of(menu.setup(), SetupItem::Players);
    (void)click(menu, players);
    CHECK(menu.setup().world.players == 4);
    (void)click(menu, ui::tr("setup.map.original"));
    CHECK(menu.setup().own_world);
    (void)click(menu, players);
    CHECK(menu.setup().world.players == 5);

    CHECK(click(menu, ui::tr("setup.original")) == app::GameSettingsMenu::Result::None);
    CHECK(menu.setup() == EndlessSetup::original());
    (void)click(menu, ui::tr("setup.defaults"));
    CHECK(menu.setup() == EndlessSetup{});
    CHECK(click(menu, ui::tr("setup.start")) == app::GameSettingsMenu::Result::Start);
    CHECK(click(menu, ui::tr("setup.back")) == app::GameSettingsMenu::Result::Back);
}

TEST_CASE("endless setup: the dialog speaks English and German") {
    for (const std::string_view code : {"de", "en"}) {
        ui::set_language(code);
        CHECK(ui::tr("setup.title") != "setup.title");
        for (std::size_t i = 0; i < app::kSetupItems; ++i) {
            const auto item = static_cast<SetupItem>(i);
            CHECK(app::setup_label(item).find("setup.") == std::string::npos);
            for (std::size_t c = 0; c < app::setup_choices(item); ++c) {
                CHECK(app::setup_value(item, c).find("setup.") == std::string::npos);
            }
        }
    }
    CHECK(ui::tr("setup.start") == "Start game");
    ui::set_language(ui::kFallbackLanguage);
}
