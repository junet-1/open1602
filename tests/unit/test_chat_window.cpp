// SPDX-License-Identifier: GPL-3.0-or-later
// The chat of a running multiplayer game (ADR 0011 point 17): to whom a line goes, the keys, and
// the lines that fade over the map while the window is closed.

#include <array>
#include <deque>
#include <string>

#include "../test_framework.hpp"
#include "chat_window.hpp"

using namespace o1602;
using app::ChatLine;
using app::ChatModel;
using app::ChatWindow;

namespace {

constexpr std::array<const ui::Font*, ui::kFontSlots> kNoFonts = {nullptr, nullptr, nullptr};

ChatModel two_partners() {
    ChatModel model;
    model.available = true;
    model.partners = {{1, "Anna", 1}, {2, "Bernd", 2}};
    return model;
}

std::size_t texts(const std::vector<ui::DrawItem>& items) {
    std::size_t n = 0;
    for (const ui::DrawItem& d : items) {
        n += d.kind == ui::DrawItem::Kind::Text ? 1U : 0U;
    }
    return n;
}

}  // namespace

TEST_CASE("chat window: Enter sends to everyone, Tab picks one player after another") {
    const ChatModel model = two_partners();
    ChatWindow chat;
    chat.open();
    CHECK(chat.is_open());
    chat.type("Hallo");
    const auto all = chat.key(model, 13);
    REQUIRE(all.has_value());
    CHECK(all->text == "Hallo");
    CHECK(all->to.empty());
    CHECK(chat.text().empty());
    CHECK(chat.is_open());  // the window stays for the next line

    (void)chat.key(model, 9);
    CHECK(chat.chosen() == std::vector<sim::PlayerId>{1});
    (void)chat.key(model, 9);
    CHECK(chat.chosen() == std::vector<sim::PlayerId>{2});
    chat.type("Nur fuer Bernd");
    const auto whisper = chat.key(model, 13);
    REQUIRE(whisper.has_value());
    CHECK(whisper->to == std::vector<sim::PlayerId>{2});
    (void)chat.key(model, 9);
    CHECK(chat.chosen().empty());  // after the last player: everyone again

    // Enter on an empty line closes, as does Escape.
    CHECK(!chat.key(model, 13).has_value());
    CHECK(!chat.is_open());
    chat.open();
    (void)chat.key(model, 27);
    CHECK(!chat.is_open());
}

TEST_CASE("chat window: a player who left is no longer chosen; a spectator who may not write sends nothing") {
    ChatModel model = two_partners();
    ChatWindow chat;
    chat.open();
    (void)chat.key(model, 9);
    CHECK(chat.chosen() == std::vector<sim::PlayerId>{1});
    model.partners.erase(model.partners.begin());  // Anna left
    chat.type("x");
    const auto say = chat.key(model, 13);
    REQUIRE(say.has_value());
    CHECK(say->to.empty());

    ChatModel watching;
    watching.available = true;
    watching.may_write = false;
    chat.type("y");
    CHECK(!chat.key(watching, 13).has_value());
    CHECK(!chat.is_open());
}

TEST_CASE("chat window: closed, the newest lines show over the map for a while") {
    const ChatWindow chat;
    std::deque<ChatLine> lines;
    for (int i = 0; i < 7; ++i) {
        lines.push_back(ChatLine{"Anna", "Zeile " + std::to_string(i), 1, 1000, false, {}});
    }
    lines.push_back(ChatLine{"Bernd", "privat", 2, 2000, true, "an Euch"});
    CHECK(texts(chat.draw_recent(lines, 2000, kNoFonts)) == ChatWindow::kRecentLines);
    // Only Bernd's line is still new enough.
    CHECK(texts(chat.draw_recent(lines, 1000 + ChatWindow::kRecentMs, kNoFonts)) == 1);
    CHECK(chat.draw_recent(lines, 2000 + ChatWindow::kRecentMs, kNoFonts).empty());
    CHECK(app::chat_prefix(lines.back()) == "Bernd (an Euch): ");
    CHECK(app::chat_prefix(lines.front()) == "Anna: ");
}
