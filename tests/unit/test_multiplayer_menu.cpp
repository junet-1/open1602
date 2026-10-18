// SPDX-License-Identifier: GPL-3.0-or-later
// The multiplayer menu (ADR 0011): addresses, its screens at work, its texts in both languages.

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../test_framework.hpp"
#include "multiplayer_menu.hpp"
#include "multiplayer_texts.hpp"

#include "o1602/net/loopback.hpp"

using namespace o1602;
using app::MpText;
using app::MultiplayerEvent;
using app::MultiplayerMenu;

namespace {

constexpr std::array<const ui::Font*, 3> kNoFonts = {nullptr, nullptr, nullptr};

/// Where the text is drawn (its anchor in design pixels), if the screen shows it.
std::optional<std::pair<std::int32_t, std::int32_t>> find_text(MultiplayerMenu& menu,
                                                               const app::MultiplayerModel& model,
                                                               const std::string& text) {
    for (const ui::DrawItem& d : menu.draw(model, kNoFonts)) {
        if (d.kind == ui::DrawItem::Kind::Text && d.text == text) {
            return std::pair{d.x, d.y};
        }
    }
    return std::nullopt;
}

/// A click on the widget that shows `text` (just above its text's base line).
std::vector<MultiplayerEvent> click(MultiplayerMenu& menu, const app::MultiplayerModel& model,
                                    const std::string& text) {
    const auto at = find_text(menu, model, text);
    REQUIRE(at.has_value());
    (void)menu.mouse(model, at->first, at->second - 4, true);
    return menu.mouse(model, at->first, at->second - 4, false);
}

/// A click at a point of the design.
std::vector<MultiplayerEvent> click_at(MultiplayerMenu& menu, const app::MultiplayerModel& model, std::int32_t x,
                                       std::int32_t y) {
    (void)menu.mouse(model, x, y, true);
    return menu.mouse(model, x, y, false);
}

std::string de(MpText id) {
    return app::mp_text(id, app::Language::German);
}

}  // namespace

TEST_CASE("multiplayer menu: addresses with and without a port") {
    const auto plain = app::parse_address(" 192.168.1.20 ");
    REQUIRE(plain.has_value());
    CHECK(plain->first == "192.168.1.20");
    CHECK(plain->second == net::kDefaultPort);
    const auto named = app::parse_address("anno.example.org:17000");
    REQUIRE(named.has_value());
    CHECK(named->first == "anno.example.org");
    CHECK(named->second == 17000);
    CHECK(!app::parse_address("").has_value());
    CHECK(!app::parse_address("host:").has_value());
    CHECK(!app::parse_address("host:0").has_value());
    CHECK(!app::parse_address("host:70000").has_value());
    CHECK(!app::parse_address("two words").has_value());
    CHECK(!app::parse_address(":16020").has_value());
}

TEST_CASE("multiplayer menu: every text in both languages, with its value") {
    for (std::size_t i = 0; i <= static_cast<std::size_t>(MpText::StartFailed); ++i) {
        const auto id = static_cast<MpText>(i);
        CHECK(!app::mp_text(id, app::Language::English).empty());
        CHECK(!app::mp_text(id, app::Language::German).empty());
    }
    CHECK(app::mp_text(MpText::PlayerJoined, app::Language::English, "Anna") == "Anna joined.");
    CHECK(app::mp_text(MpText::PlayerJoined, app::Language::German, "Anna") == "Anna ist beigetreten.");
    // The fonts' Windows-1252: "Zurück" with 0xFC.
    CHECK(de(MpText::Back) ==
          "Zur\xFC"
          "ck");
    CHECK(!app::mp_text(net::RefuseReason::Password, app::Language::German).empty());
    CHECK(!app::mp_text(net::EndReason::HostLeft, app::Language::English).empty());
    CHECK(app::language_of(nullptr) == app::Language::English);
}

TEST_CASE("multiplayer menu: searching, joining by address and back") {
    MultiplayerMenu menu(app::Language::German, ui::FormStyle{});
    app::MultiplayerModel model;
    model.name = "Anna";
    menu.open();
    CHECK(menu.screen() == MultiplayerMenu::Screen::Browse);
    CHECK(find_text(menu, model, de(MpText::NoGamesFound)).has_value());
    CHECK(menu.name() == "Anna");

    // A game that answered the search: one click chooses it, the button joins it.
    net::FoundGame found;
    found.address = "192.168.1.20";
    found.info.title = "Annas Spiel";
    found.info.host = "Anna";
    found.info.players = 1;
    found.info.slots = 4;
    found.ping_ms = 12;
    model.found.push_back(found);
    CHECK(click(menu, model, "Annas Spiel").empty());
    auto events = click(menu, model, de(MpText::Join));
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == MultiplayerEvent::Kind::Join);
    CHECK(events[0].address == "192.168.1.20");
    CHECK(events[0].port == net::kDefaultPort);

    // An address typed in, with a password; Enter connects.
    CHECK(click_at(menu, model, 800, 166).empty());  // the address field
    CHECK(menu.typing());
    menu.type("10.0.0.7:17000");
    (void)menu.key(model, 9);  // Tab: the password
    menu.type("geheim");
    events = menu.key(model, 13);
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == MultiplayerEvent::Kind::Join);
    CHECK(events[0].address == "10.0.0.7");
    CHECK(events[0].port == 17000);
    CHECK(events[0].password == "geheim");
    CHECK(!menu.typing());

    // Escape: on the search it closes the menu.
    const auto back = menu.back();
    REQUIRE(back.has_value());
    CHECK(back->kind == MultiplayerEvent::Kind::Close);
}

TEST_CASE("multiplayer menu: creating a game: endless, scenario, settings") {
    MultiplayerMenu menu(app::Language::English, ui::FormStyle{});
    app::MultiplayerModel model;
    model.name = "Bernd";
    model.level_names = {"Easy", "Medium", "Hard", "Very hard"};
    app::MenuScenario scenario;
    scenario.name = "Die Insel";
    scenario.files = {{0xFF, "Szenes/Insel.szs"}};
    model.scenarios.push_back(scenario);
    menu.open();
    CHECK(click(menu, model, app::mp_text(MpText::CreateGame, app::Language::English)).empty());
    CHECK(menu.screen() == MultiplayerMenu::Screen::Create);
    // Escape goes back to the search by itself: nothing for the viewer.
    CHECK(!menu.back().has_value());
    CHECK(menu.screen() == MultiplayerMenu::Screen::Browse);
    CHECK(click(menu, model, "Create game").empty());

    CHECK(click(menu, model, "Hard").empty());
    CHECK(click(menu, model, "Host without a player").empty());
    auto events = click(menu, model, "Open lobby");
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == MultiplayerEvent::Kind::Host);
    const app::HostSetup& endless = events[0].setup;
    CHECK(endless.kind == app::HostSetup::Kind::Endless);
    CHECK(endless.level == 2);
    CHECK(endless.dedicated);
    CHECK(endless.title == "Bernd's game");
    CHECK(endless.port == net::kDefaultPort);
    CHECK(endless.file_name.find("Hard") != std::string::npos);

    // A scenario must be chosen before the lobby opens.
    CHECK(click(menu, model, "Scenario").empty());
    CHECK(click(menu, model, "Open lobby").empty());  // not yet: nothing chosen
    CHECK(click(menu, model, "Die Insel").empty());
    events = click(menu, model, "Open lobby");
    REQUIRE(events.size() == 1);
    CHECK(events[0].setup.kind == app::HostSetup::Kind::Scenario);
    CHECK(events[0].setup.file == std::filesystem::path("Szenes/Insel.szs"));
    CHECK(events[0].setup.file_name == "Die Insel");
}

TEST_CASE("multiplayer menu: the host's lobby: slots, chat and the start") {
    net::LoopbackNetwork network;
    net::Lobby lobby;
    lobby.title = "Abend";
    lobby.game = "Endless game";
    lobby.seats.resize(4);
    net::HostSession host(network.listen(), lobby, 0, "Anna", 1);
    host.update(0);

    MultiplayerMenu menu(app::Language::English, ui::FormStyle{});
    app::MultiplayerModel model;
    model.name = "Anna";
    model.session = &host;
    model.hosting = true;
    model.everyone_ready = host.everyone_ready();
    menu.show_lobby();
    CHECK(find_text(menu, model, "Abend").has_value());
    CHECK(find_text(menu, model, "Anna (you)").has_value());
    CHECK(menu.typing());  // the chat line takes the keys

    // The host clicks an open slot through the kinds.
    auto events = click(menu, model, "Open");
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == MultiplayerEvent::Kind::SetSeat);
    CHECK(events[0].seat == 1);
    CHECK(events[0].seat_kind == net::LobbySeat::Kind::Computer);

    menu.type("Hallo");
    events = menu.key(model, 13);
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == MultiplayerEvent::Kind::Say);
    CHECK(events[0].text == "Hallo");
    CHECK(menu.typing());  // the chat line stays open

    events = click(menu, model, "Start game");
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == MultiplayerEvent::Kind::Start);

    // Escape twice: out of the chat line, then out of the session.
    (void)menu.key(model, 27);
    const auto back = menu.back();
    REQUIRE(back.has_value());
    CHECK(back->kind == MultiplayerEvent::Kind::Leave);
}
