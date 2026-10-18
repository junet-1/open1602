// SPDX-License-Identifier: GPL-3.0-or-later
// The original's layouts and fonts (docs/formats/gad.md, docs/re/ui.md).

#include <cstdio>
#include <string>

#include "../test_framework.hpp"

#include "o1602/assets/gad.hpp"
#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/palette.hpp"
#include "o1602/ui/font.hpp"
#include "o1602/ui/panel.hpp"
#include "o1602/ui/resources.hpp"

using namespace o1602;

namespace {

assets::GameDirectory game_or_skip(const char* needs) {
    auto game = assets::GameDirectory::from_environment();
    if (!game) {
        SKIP("ANNO1602_DIR not set");
    }
    if (!game->resolve(needs)) {
        SKIP("interface files not installed");
    }
    return std::move(game).value();
}

}  // namespace

TEST_CASE("data: the layouts of GADDATA load; the panel's minimap area and buttons") {
    const auto game = game_or_skip("GADDATA/CTRL.GAD");
    auto ctrl = assets::load_gad(game, "CTRL.GAD");
    REQUIRE(ctrl.ok());
    CHECK(ctrl->warnings.empty());
    const assets::Gadget* map = ctrl->find(30005);
    REQUIRE(map != nullptr);
    CHECK(map->kind == assets::GadgetKind::Area);
    CHECK(map->pos == (std::array<std::int32_t, 2>{34, 14}));
    CHECK(map->size == (std::array<std::int32_t, 2>{186, 160}));
    const assets::Gadget* left = ctrl->find(30001);
    REQUIRE(left != nullptr);
    CHECK(left->kind == assets::GadgetKind::Graphic);
    CHECK(left->gfx == 1);  // GFXPFEIL
    CHECK(left->press_offset == 2);
    CHECK(left->offset == (std::array<std::int16_t, 2>{9, 14}));  // 32 - 23, 213 - 199
    CHECK(ctrl->gadgets[0].gfx == 0);                             // GFXTAFEL, the panel's background
    // Every file the original loads parses without trouble, none beyond the original's 300.
    std::size_t files = 0;
    for (const char* name :
         {"ANNO.GAD",    "BASE.GAD",     "BAU.GAD",    "BAUKONT.GAD",  "BAUSHIP.GAD",  "BERGWERK.GAD", "BUBBLE.GAD",
          "CLIENT.GAD",  "CTRL.GAD",     "DISK.GAD",   "ENDE.GAD",     "FARBWAHL.GAD", "HANDEL.GAD",   "HOST.GAD",
          "INFO.GAD",    "INFRA.GAD",    "KONTOR.GAD", "LAGER.GAD",    "LEIST.GAD",    "MILITAR.GAD",  "MISSION.GAD",
          "MKSHIP.GAD",  "MKSOLDAT.GAD", "MUSIK.GAD",  "OPTION.GAD",   "PIRAT.GAD",    "PIRATORD.GAD", "PLANTAGE.GAD",
          "PROD.GAD",    "ROUTE.GAD",    "SHIP.GAD",   "SHIPLIST.GAD", "SIEDLER.GAD",  "SKSHIP.GAD",   "SKSOLDAT.GAD",
          "STADT.GAD",   "STADTLST.GAD", "TRADE.GAD",  "TRADER.GAD",   "TRANSFER.GAD", "TRIBUT.GAD",   "TUTOR.GAD",
          "VERTRAG.GAD", "WAITLOAD.GAD", "WERFT.GAD"}) {
        auto l = assets::load_gad(game, name);
        if (!l) {
            continue;
        }
        ++files;
        CHECK(!l->gadgets.empty());
        CHECK(l->gadgets.size() <= assets::kOriginalMaxGadgets);
        for (const auto& w : l->warnings) {
            std::printf("  %s: %s\n", name, w.c_str());
        }
    }
    CHECK(files == 45);
    // The status line: right aligned text in the brown colour (LEIST.GAD 100001).
    auto leist = assets::load_gad(game, "LEIST.GAD");
    REQUIRE(leist.ok());
    const assets::Gadget* status = leist->find(100001);
    REQUIRE(status != nullptr);
    CHECK(status->kind == assets::GadgetKind::TextRight);
    CHECK(status->color[0] == 5);
}

TEST_CASE("data: the fonts of the three screen sizes, umlauts and the text colours") {
    const auto game = game_or_skip("TOOLGFX/ZEI14A.ZEI");
    auto font = ui::Font::load(game, ui::font_file(0, ui::ArtSize::Large));
    REQUIRE(font.ok());
    CHECK(font->height() == 24);
    CHECK(font->width("0") == 11);
    CHECK(font->width(" ") == 8);
    CHECK(font->has(static_cast<unsigned char>('\xDC')));  // Ü
    auto large = ui::Font::load(game, ui::font_file(2, ui::ArtSize::Large));
    REQUIRE(large.ok());
    CHECK(large->has('9'));
    CHECK(!large->has('A'));  // digits and punctuation only
    for (auto size : {ui::ArtSize::Small, ui::ArtSize::Medium, ui::ArtSize::Large}) {
        for (std::int32_t slot = 0; slot < 3; ++slot) {
            CHECK(ui::Font::load(game, ui::font_file(slot, size)).ok());
        }
    }
    const auto palette = assets::Palette::load(*game.resolve("TOOLGFX/STADTFLD.COL"));
    REQUIRE(palette.ok());
    const ui::Recolour white = ui::text_recolour(*palette, 0);
    const assets::Rgba& body = (*palette)[white[1]];
    CHECK(body.r > 240);
    CHECK(body.g > 240);
    const ui::Recolour red = ui::text_recolour(*palette, 9);
    CHECK((*palette)[red[1]].r > 180);
    CHECK((*palette)[red[1]].g < 60);
    // A mission text wraps into the briefing's lines without losing words.
    const std::string text =
        "Nach einer langen, kr\xE4"
        "ftezehrenden Reise entdeckt Ihr ein Atoll.";
    const auto lines = ui::wrap_text(*font, text, 200);
    CHECK(lines.size() >= 2);
    std::string joined;
    for (const auto& l : lines) {
        joined += (joined.empty() ? "" : " ") + l;
    }
    CHECK(joined == text);
}
