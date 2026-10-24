// SPDX-License-Identifier: GPL-3.0-or-later
// What the game, its tools and a server share to start the same game
// (engine/assets/include/o1602/assets/game_content.hpp, LC-17, DS-5).

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "../test_framework.hpp"
#include "test_support.hpp"

#include "o1602/assets/game_content.hpp"

using namespace o1602;

namespace {

struct Folder {
    std::filesystem::path root;
    explicit Folder(const char* name) : root(test::temp_path(name)) {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
    }
    ~Folder() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    void write(const char* file, const std::string& text) const { std::ofstream(root / file) << text; }
};

}  // namespace

TEST_CASE("game content: the fingerprint changes with the tables the simulation reads, not with the rest") {
    Folder folder("o1602_content_test");
    for (const char* d : {"toolgfx", "gfx"}) {
        std::filesystem::create_directories(folder.root / d);
    }
    folder.write("toolgfx/stadtfld.col", "x");
    folder.write("gfx/stadtfld.bsh", "x");
    folder.write("haeuser.cod", "a");
    folder.write("figuren.cod", "b");
    folder.write("text.cod", "one");
    auto game = assets::GameDirectory::open(folder.root);
    REQUIRE(game.ok());
    const std::uint64_t first = assets::content_fingerprint(*game);
    CHECK(assets::content_fingerprint(*game) == first);
    folder.write("text.cod", "another language");
    CHECK(assets::content_fingerprint(*game) == first);
    folder.write("figuren.cod", "c");
    CHECK(assets::content_fingerprint(*game) != first);
    // The files' boundaries count: "ab" + "" is not "a" + "b".
    folder.write("haeuser.cod", "ab");
    folder.write("figuren.cod", "");
    const std::uint64_t joined = assets::content_fingerprint(*game);
    folder.write("haeuser.cod", "a");
    folder.write("figuren.cod", "b");
    CHECK(assets::content_fingerprint(*game) == first);
    CHECK(joined != first);
}

TEST_CASE("game content: the mods' packages in the order of their UTF-8 names on every system") {
    Folder mods("o1602_mod_packages_test");
    for (const char* name : {"zeta", "Alpha", "beta", "\xc3\xa4rger"}) {  // "ärger": UTF-8 after ASCII
        std::filesystem::create_directories(
            mods.root / std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(name))));
    }
    mods.write("readme.txt", "not a package");
    const std::vector<assets::ModPackage> packages = assets::mod_packages(mods.root);
    REQUIRE(packages.size() == 4);
    CHECK(packages[0].id == "Alpha");
    CHECK(packages[1].id == "beta");
    CHECK(packages[2].id == "zeta");
    CHECK(packages[3].id == "\xc3\xa4rger");
    CHECK(packages[3].folder.filename() == std::filesystem::path(u8"ärger"));
    CHECK(assets::mod_packages(mods.root / "missing").empty());
}
