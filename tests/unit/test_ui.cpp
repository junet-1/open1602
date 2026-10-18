// SPDX-License-Identifier: GPL-3.0-or-later
// Interface layouts (GAD), fonts and text, panels at work (docs/formats/gad.md, docs/re/ui.md).

#include <string>
#include <vector>

#include "../test_framework.hpp"
#include "test_support.hpp"

#include "o1602/assets/gad.hpp"
#include "o1602/ui/font.hpp"
#include "o1602/ui/panel.hpp"
#include "o1602/ui/resources.hpp"

using namespace o1602;

namespace {

/// A font of 96 glyphs (codes 32..127), each 4 pixels wide and 6 high; 'W' (code 87) is 8 wide.
ui::Font test_font() {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> sizes(96, {4, 6});
    sizes['W' - 32] = {8, 6};
    const auto sheet = test::make_sheet(sizes, 1);
    auto font = ui::Font::from_sheet(sheet);
    REQUIRE(font.ok());
    return std::move(font).value();
}

constexpr const char* kInclude =
    "  BLK_TOOL = 0\n"
    "  GFXPFEIL = 1\n";

constexpr const char* kLayout =
    "  Include: \"TOOLS.INC\"\n"
    "  Objekt: GADGET\n"
    "    @Nummer:    0\n"
    "    Id:         0\n"
    "    Blocknr:    BLK_TOOL\n"
    "    Kind:       GAD_UNUSED\n"
    "    ObjFill:    0,MAXGADGET\n"
    "    IDHAUPT = 500\n"
    "    @Nummer:    0\n"
    "    BASE = Nummer\n"
    "    Id:         IDHAUPT+0\n"
    "    Kind:       GAD_GFX\n"
    "    Gfxnr:      GFXPFEIL\n"
    "    Pressoff:   2\n"
    "    Flipflg:    1\n"
    "    Color:      1, 3\n"
    "    Pos:        10, 20\n"
    "    Size:       40, 30\n"
    "    Posoffs:    12-Pos[0], 25-Pos[1]\n"
    "    @Nummer:    +1\n"
    "    Id:         IDHAUPT+1\n"
    "    Kind:       GAD_TEXTZ\n"
    "    Color:      5\n"
    "    Reiheflg:   1\n"
    "    Basenr:     BASE+1\n"
    "    Pos:        0, 100\n"
    "    Size:       50, 20\n"
    "    @Nummer:    +1\n"
    "    Id:         IDHAUPT+2\n"
    "    Kind:       GAD_TEXTZ\n"
    "    Reiheflg:   1\n"
    "    Basenr:     BASE+1\n"
    "    @Pos:       +50\n"
    "    Size:       50, 20\n"
    "    @Nummer:    +1\n"
    "    Id:         IDHAUPT+3\n"
    "    Kind:       GAD_BALKEN\n"
    "    Slidhorflg: 1\n"
    "    Pos:        0, 200\n"
    "    Size:       100, 10\n"
    "  EndObj\n";

assets::GadgetLayout layout() {
    return assets::parse_gad(kLayout, [](std::string_view name) -> std::optional<std::string> {
        return name == "TOOLS.INC" ? std::optional<std::string>(kInclude) : std::nullopt;
    });
}

}  // namespace

TEST_CASE("ui: a GAD layout with include, relative numbers and pairs that keep their second value") {
    const auto l = layout();
    REQUIRE(l.gadgets.size() == 4);  // the highest Nummer, not the 300 defaults of ObjFill
    const assets::Gadget& arrow = l.gadgets[0];
    CHECK(arrow.id == 500);
    CHECK(arrow.kind == assets::GadgetKind::Graphic);
    CHECK(arrow.gfx == 1);
    CHECK(arrow.toggle);
    CHECK(arrow.offset[0] == 2);
    CHECK(arrow.offset[1] == 5);
    const assets::Gadget& text = l.gadgets[1];
    CHECK(text.color[0] == 5);
    CHECK(text.color[1] == 3);  // kept from the line before
    CHECK(text.base == 1);
    // "@Pos: +50" moves only x.
    CHECK(l.gadgets[2].pos[0] == 50);
    CHECK(l.gadgets[2].pos[1] == 100);
    CHECK(l.gadgets[3].slider);
    CHECK(l.gadgets[3].slider_size[0] == 100);  // a range of 0 is the size
    CHECK(l.find(502) == &l.gadgets[2]);
    CHECK(l.warnings.empty());
    const auto missing = assets::parse_gad(kLayout);
    CHECK(!missing.warnings.empty());
}

TEST_CASE("ui: regions of the screen and the original's rounding") {
    const auto r = ui::screen_regions(1024, 768);
    CHECK((r.side_panel == ui::Rect{768, 0, 256, 768}));
    CHECK((r.status_bar == ui::Rect{0, 734, 768, 34}));
    CHECK((r.tutor == ui::Rect{0, 594, 768, 140}));
    CHECK((r.game_view == ui::Rect{0, 0, 768, 734}));
    const auto s = ui::screen_regions(800, 600);
    CHECK(s.side_panel.w == 200);
    CHECK(ui::to_pixels(10, 0.78125) == 8);
    CHECK(ui::original_art_size(1280) == ui::ArtSize::Medium);
    CHECK(ui::art_size(1280) == ui::ArtSize::Large);
    CHECK(ui::sheet_file(0, ui::ArtSize::Medium) == "TOOLGFX/TOOLS8.BSH");
    CHECK(ui::font_file(2, ui::ArtSize::Small) == "TOOLGFX/ZEI20V.ZEI");
}

TEST_CASE("ui: a panel draws by state and answers the mouse") {
    ui::Panel panel(layout(), 1.0);
    panel.set_text(501, "Ja");
    const std::array<const ui::Font*, 3> fonts{};
    auto items = panel.draw(fonts);
    REQUIRE(items.size() == 3);  // the arrow, one text (the other is empty), the bar
    CHECK(items[0].kind == ui::DrawItem::Kind::Sprite);
    CHECK(items[0].gfx == 1);
    CHECK(items[0].x == 12);
    CHECK(items[0].y == 25);
    CHECK(items[1].kind == ui::DrawItem::Kind::Text);
    CHECK(items[1].align == ui::Align::Centre);
    CHECK(items[1].colour == 5);
    CHECK(items[2].w == 0);  // the bar follows its slider
    // Pressing the arrow shows the pressed picture; releasing on it clicks and toggles it.
    CHECK(panel.mouse(20, 30, true).empty());
    CHECK(panel.draw(fonts)[0].gfx == 3);
    auto ev = panel.mouse(20, 30, false);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].kind == ui::PanelEvent::Kind::Clicked);
    CHECK(ev[0].id == 500);
    CHECK(panel.selected(500));
    // Released elsewhere: no click.
    panel.mouse(20, 30, true);
    CHECK(panel.mouse(300, 300, false).empty());
    CHECK(panel.selected(500));
    // Radio group: one of the two texts.
    panel.mouse(10, 110, true);
    panel.mouse(10, 110, false);
    CHECK(panel.selected(501));
    panel.mouse(60, 110, true);
    panel.mouse(60, 110, false);
    CHECK(panel.selected(502));
    CHECK(!panel.selected(501));
    CHECK(panel.hovered() == std::optional<std::int32_t>(502));
    // The slider (bar) follows the mouse while held and scales to a range.
    ev = panel.mouse(40, 205, true);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].kind == ui::PanelEvent::Kind::Slid);
    CHECK(panel.slider(503, 0, 1000) == 400);
    panel.mouse(250, 205, true);
    CHECK(panel.slider(503, 0, 1000) == 1000);
    panel.mouse(250, 205, false);
    panel.set_slider(503, 0, 250, 1000);
    CHECK(panel.draw(fonts)[2].w == 25);
    // Disabled: a checkerboard over the gadget.
    panel.set_disabled(500, true);
    items = panel.draw(fonts);
    CHECK(items[1].kind == ui::DrawItem::Kind::SpriteRaster);
}

TEST_CASE("ui: text width, colours, wrapping with hyphens and hard breaks") {
    const ui::Font font = test_font();
    CHECK(font.height() == 6);
    CHECK(font.width("abc") == 12);
    CHECK(font.width("aW") == 12);
    CHECK(font.width("ab\ncd") == 8);  // up to the control character
    // Lines of at most 20 pixels (5 characters).
    CHECK((ui::wrap_text(font, "aa bb cc", 20) == std::vector<std::string>{"aa bb", "cc"}));
    CHECK((ui::wrap_text(font, "aa\n\nbb", 100) == std::vector<std::string>{"aa", "bb"}));  // several breaks: one
    CHECK((ui::wrap_text(font, "abc--def", 100) == std::vector<std::string>{"abcdef"}));
    CHECK((ui::wrap_text(font, "x abc--def", 24) == std::vector<std::string>{"x abc-", "def"}));
    CHECK((ui::wrap_text(font, "abcdefgh", 12) == std::vector<std::string>{"abc", "def", "gh"}));
    // A line starting with a byte from 0x80 keeps it after a hard break (U-017).
    CHECK((ui::wrap_text(font, "a\n\xC4z", 100).back() == std::string("\xC4z")));
    CHECK((ui::wrap_text(font, "aa bb cc", 20000) == std::vector<std::string>{"aa bb cc"}));
    // Drawing: glyph index 1 becomes the colour's body.
    assets::IndexedImage canvas;
    canvas.width = 20;
    canvas.height = 10;
    canvas.indices.assign(200, 0);
    canvas.opaque.assign(200, 0);
    ui::Recolour colour{};
    colour[1] = 42;
    CHECK(ui::draw_text(canvas, font, "ab", 10, 8, colour, ui::Align::Centre) == 8);
    CHECK(canvas.indices[2 * 20 + 6] == 42);  // top row 8 - 6 = 2, left 10 - 4
    CHECK(canvas.opaque[2 * 20 + 5] == 0);
}
