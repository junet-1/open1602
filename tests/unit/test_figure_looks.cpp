// SPDX-License-Identifier: GPL-3.0-or-later
// Figures and ships drawn with a package's images: figure_images.toml
// (engine/assets/include/o1602/assets/figure_looks.hpp, docs/modding/graphics.md).

#include <string>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"
#include "o1602/assets/figure_looks.hpp"

using namespace o1602;

namespace {

/// Swordsmen in the four colours (1 to 4, one picture each) and a ship (21, eight directions).
assets::FigureTable make_figures() {
    const assets::CodPredefinedSymbol types[] = {{"FIGUR", 1}};
    const auto doc = assets::cod::parse(
        "Objekt: FIGUR\n"
        "  Nummer: 1\n  Kind: 1\n  Gfx: 10\n  Blocknr: 1\n  Schatten: 30\n"
        "  Nummer: 2\n  Kind: 1\n  Gfx: 11\n  Blocknr: 1\n  Schatten: 30\n"
        "  Nummer: 3\n  Kind: 1\n  Gfx: 12\n  Blocknr: 1\n  Schatten: 30\n"
        "  Nummer: 4\n  Kind: 1\n  Gfx: 13\n  Blocknr: 1\n  Schatten: 30\n"
        "  Nummer: 21\n  Gfx: 40\n  Blocknr: 2\n  Rotate: 2\n"
        "EndObj\n",
        types);
    auto table = assets::FigureTable::from_cod(doc);
    REQUIRE(table.ok());
    return std::move(table).value();
}

assets::PackageImage image(std::string id, std::uint32_t first, std::uint32_t count) {
    assets::PackageImage i;
    i.id = std::move(id);
    i.first = first;
    i.count = count;
    i.width = 64;
    i.height = 64;
    return i;
}

assets::FigureLook look(std::uint32_t figure, std::uint32_t gfx, bool shadow) {
    return assets::FigureLook{figure, gfx, shadow};
}

}  // namespace

TEST_CASE("figure images: how many sprites a figure draws") {
    assets::FigureType type;
    type.rotate = 3;
    CHECK(assets::figure_sprite_count(type) == 22);  // 8 directions, 3 apart
    assets::FigureAnim walk;
    walk.defined = true;
    walk.offset = 24;
    walk.add = 1;
    walk.count = 8;
    walk.rotate = 8;
    type.anims = {walk};
    CHECK(assets::figure_sprite_count(type) == 24 + 7 + 7 * 8 + 1);
    type.anims[0].defined = false;  // an ANIM number the file skips
    CHECK(assets::figure_sprite_count(type) == 22);
}

TEST_CASE("figure images: a package draws figures and ships with its images") {
    const assets::FigureTable original = make_figures();
    const std::vector<assets::PackageImage> images = {image("knights:knight", 40000, 1),
                                                      image("knights:cog", 40001, 16)};
    std::vector<assets::FigureLook> looks;
    std::vector<std::string> warnings;
    assets::apply_figure_images_file(looks, "knights",
                                     "[\"anno1602:swordsman\"]\n"  // lines 1-3
                                     "image = \"knights:knight\"\n"
                                     "shadow = false\n"
                                     "[\"anno1602:21\"]\n"  // line 4
                                     "image = \"knights:cog\"\n"
                                     "[\"anno1602:pikeman_x\"]\n"  // line 6
                                     "image = \"knights:cog\"\n"
                                     "[\"anno1602:0x15\"]\n"  // line 8: the ship again, an image too small
                                     "image = \"knights:knight\"\n"
                                     "[\"anno1602:2\"]\n"  // line 10
                                     "image = \"knights:none\"\n"
                                     "size = 2\n"
                                     "[\"anno1602:3\"]\n"    // line 13
                                     "[\"anno1602:999\"]\n"  // line 14
                                     "image = \"knights:cog\"\n",
                                     images, original, warnings);
    REQUIRE(looks.size() == 5);
    CHECK(looks[0] == look(1, 40000, false));
    CHECK(looks[3] == look(4, 40000, false));
    CHECK(looks[4] == look(21, 40001, true));
    REQUIRE(warnings.size() == 6);
    CHECK(warnings[0] ==
          "knights/figure_images.toml line 6: [anno1602:pikeman_x]: names no figure of the original "
          "(anno1602:<name or number>)");
    CHECK(warnings[1] ==
          "knights/figure_images.toml line 8: [anno1602:0x15]: its image knights:knight holds 1 sprites, the figure "
          "draws 15");
    CHECK(warnings[2] ==
          "knights/figure_images.toml line 11: 'image' names no image of a package (graphics.toml, <package>:<name>)");
    CHECK(warnings[3] ==
          "knights/figure_images.toml line 12: 'size' is no field of a figure's image and is passed over");
    CHECK(warnings[4] == "knights/figure_images.toml line 13: [anno1602:3] needs an 'image'");
    CHECK(warnings[5] == "knights/figure_images.toml line 14: [anno1602:999]: figuren.cod has no figure 999");

    // The table draws them from the terrain sheet; the original's pictures come back without them.
    assets::FigureTable table = original;
    assets::apply_figure_looks(table, original, looks);
    CHECK(table.get(2)->sheet == assets::FigureSheet::Stadtfld);
    CHECK(table.get(2)->gfx == 40000);
    CHECK(table.get(2)->shadow == 0);
    CHECK(table.get(21)->gfx == 40001);
    CHECK(table.get(21)->rotate == 2);
    assets::apply_figure_looks(table, original, {});
    CHECK(table.get(2)->sheet == assets::FigureSheet::Soldat);
    CHECK(table.get(2)->gfx == 11);
    CHECK(table.get(2)->shadow == 30);
    CHECK(table.get(21)->sheet == assets::FigureSheet::Ship);
}
