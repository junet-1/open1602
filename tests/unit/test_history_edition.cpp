// SPDX-License-Identifier: GPL-3.0-or-later
// The History Edition's own data: its RDA archive, its texts and the scenarios named by them.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/game_directory.hpp"
#include "o1602/assets/history_texts.hpp"
#include "o1602/assets/rda_archive.hpp"
#include "o1602/assets/text_table.hpp"

using namespace o1602;

namespace {

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

void put_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    put_u32(out, static_cast<std::uint32_t>(v));
    put_u32(out, static_cast<std::uint32_t>(v >> 32U));
}

/// An RDA 2.2 archive with `files` in one stored block, and a second, compressed block whose file
/// is left out: header, data, directory, block header; then the second block.
std::vector<std::uint8_t> make_rda(const std::vector<std::pair<std::string, std::string>>& files) {
    std::vector<std::uint8_t> out;
    const std::string_view magic = "Resource File V2.2";
    out.insert(out.end(), magic.begin(), magic.end());
    out.resize(0x310, 0);
    put_u64(out, 0);  // the first block's header, set below
    std::vector<std::uint64_t> offsets;
    for (const auto& [name, data] : files) {
        offsets.push_back(out.size());
        out.insert(out.end(), data.begin(), data.end());
    }
    const auto directory = [&](const std::vector<std::pair<std::string, std::string>>& list,
                               const std::vector<std::uint64_t>& at, std::uint64_t stored_extra) {
        for (std::size_t i = 0; i < list.size(); ++i) {
            std::vector<std::uint8_t> name(520, 0);
            for (std::size_t c = 0; c < list[i].first.size(); ++c) {
                name[c * 2] = static_cast<std::uint8_t>(list[i].first[c]);
            }
            out.insert(out.end(), name.begin(), name.end());
            put_u64(out, at[i]);
            put_u64(out, list[i].second.size() + stored_extra);
            put_u64(out, list[i].second.size());
            put_u64(out, 0);
            put_u64(out, 0);
        }
    };
    directory(files, offsets, 0);
    const std::uint64_t first = out.size();
    for (int i = 0; i < 8; ++i) {
        out[0x310 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(first >> (8 * i));
    }
    put_u32(out, 0);
    put_u32(out, static_cast<std::uint32_t>(files.size()));
    put_u64(out, files.size() * 560);
    put_u64(out, files.size() * 560);
    const std::size_t next_at = out.size();
    put_u64(out, 0);
    // The second block: flag 1 (compressed); its directory is not read.
    const std::vector<std::pair<std::string, std::string>> packed{{"data/packed.bin", "xyz"}};
    directory(packed, {0x318}, 0);
    const std::uint64_t second = out.size();
    for (int i = 0; i < 8; ++i) {
        out[next_at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(second >> (8 * i));
    }
    put_u32(out, 1);
    put_u32(out, 1);
    put_u64(out, 560);
    put_u64(out, 560);
    put_u64(out, second + 32);  // the end of the file: no further block
    return out;
}

constexpr std::string_view kTexts =
    "<root>\r\n<languages><language languageID=\"1\">en_us</language>"
    "<language languageID=\"16\">de_de</language></languages>\r\n<texts>\r\n"
    "<line lineID=\"7\"><translation languageID=\"1\">Atoll</translation>"
    "<translation languageID=\"16\">Das Atoll</translation></line>\r\n"
    "<line lineID=\"8\"><translation languageID=\"16\">Erste Zeile\r\nZweite &amp; letzte</translation></line>\r\n"
    "<line lineID=\"48\"><translation languageID=\"1\">Continous Play</translation>"
    "<translation languageID=\"16\">Endlosspiel</translation></line>\r\n"
    "<line lineID=\"363\"><translation languageID=\"1\">Continous Play</translation>"
    "<translation languageID=\"16\">Endlosspiel</translation></line>\r\n"
    "<line lineID=\"676\"><translation languageID=\"1\">Show game options screen</translation>"
    "<translation languageID=\"16\">Spieloptionen anzeigen</translation></line>\r\n"
    "<line lineID=\"700\"><translation languageID=\"1\">Caf&#233;</translation></line>\r\n"
    "</texts></root>";

void write(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(text.data(), static_cast<std::streamsize>(text.size()));
}

void write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    write(path, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

}  // namespace

TEST_CASE("history edition: the stored files of an RDA 2.2 archive are read, packed ones are not") {
    auto archive = assets::RdaArchive::from_bytes(make_rda({{"data/texts.xml", "<root/>"}, {"data\\a.txt", "abc"}}));
    REQUIRE(archive.ok());
    REQUIRE(archive->entries().size() == 2);            // the compressed block's directory is not read
    CHECK(archive->entries()[1].name == "data/a.txt");  // '\' as '/'
    REQUIRE(archive->read("DATA/Texts.xml").ok());
    const auto texts = archive->read("data/texts.xml");
    CHECK(std::string(texts->begin(), texts->end()) == "<root/>");
    CHECK(archive->read("data/a.txt").ok());
    CHECK(archive->find("data/packed.bin") == nullptr);  // its block is compressed
    CHECK(!archive->read("data/missing.xml").ok());

    CHECK(!assets::RdaArchive::from_bytes({'R', 'e', 's'}).ok());
    auto broken = make_rda({{"data/a.txt", "abc"}});
    broken[0x310] = 0xF0;  // the first block's header beyond the file
    broken[0x311] = 0xFF;
    broken[0x312] = 0xFF;
    CHECK(!assets::RdaArchive::from_bytes(broken).ok());
}

TEST_CASE("history edition: texts.xml gives the lines in a language, English when it lacks one") {
    const auto texts = assets::HistoryTexts::parse(kTexts);
    REQUIRE(texts.ok());
    CHECK(texts->size() == 6);
    CHECK(texts->text(7, "de") == "Das Atoll");
    CHECK(texts->text(7, "de-DE") == "Das Atoll");
    CHECK(texts->text(7, "en") == "Atoll");
    CHECK(texts->text(7, "fr") == "Atoll");  // no French: English
    CHECK(texts->text(8, "de") == "Erste Zeile\nZweite & letzte");
    CHECK(!texts->text(8, "en").has_value());        // German only
    CHECK(texts->text(700, "en") == "Caf\xC3\xA9");  // UTF-8
    CHECK(!texts->text(9, "de").has_value());

    CHECK(texts->expand("[[676]]", "de") == "Spieloptionen anzeigen");
    CHECK(texts->expand("a [[7]] b [[9]] c [[x]] [[48]]", "en") == "a Atoll b [[9]] c [[x]] Continous Play");
    CHECK(texts->expand("[[7", "de") == "[[7");

    const std::vector<std::int32_t> endless{48, 363};
    CHECK(texts->find_all("endlosspiel", "de") == endless);
    CHECK(texts->find_all("Das Atoll", "en") == std::vector<std::int32_t>{7});  // German in English
    CHECK(texts->find_all("Kalt erwischt", "de").empty());

    CHECK(!assets::HistoryTexts::parse("<root><texts></texts></root>").ok());
    CHECK(!assets::HistoryTexts::parse("<line lineID=\"1\"><translation languageID=\"1\">a").ok());
}

TEST_CASE("history edition: scenarios are found by name and Text.cod's placeholders are filled") {
    const auto root = test::temp_path("o1602-test-history-texts");
    std::filesystem::remove_all(root);
    for (const char* file : {"haeuser.cod", "figuren.cod", "ToolGfx/STADTFLD.COL", "GFX/STADTFLD.BSH",
                             "Szenes/[[7]].szs", "Szenes/[[48]]12.szs", "Szenes/[[363]]00.szm"}) {
        write(root / file, std::string_view{});
    }
    write(root / "ahe" / "a1he0.rda", make_rda({{"data/texts.xml", std::string(kTexts)}}));
    // Text.cod as the original's: every byte negated.
    std::string cod = "[LEISTE]\r\nMusik\r\n[[676]]\r\nEnde\r\n[END]\r\n";
    for (char& c : cod) {
        c = static_cast<char>(-static_cast<signed char>(c));
    }
    write(root / "Loca" / "de-DE" / "Text.cod", cod);

    auto game = assets::GameDirectory::open(root);
    REQUIRE(game);
    REQUIRE(game->history_texts() != nullptr);
    game->choose_language("de");
    CHECK(game->text_language() == "de");
    CHECK(game->resolve("Szenes/Das Atoll.szs") == root / "Szenes" / "[[7]].szs");
    CHECK(game->resolve("szenes/das atoll.szs") == root / "Szenes" / "[[7]].szs");
    CHECK(game->resolve("Szenes/Endlosspiel12.szs") == root / "Szenes" / "[[48]]12.szs");
    CHECK(game->resolve("Szenes/Endlosspiel00.szm") == root / "Szenes" / "[[363]]00.szm");  // the other line
    CHECK(game->resolve("Szenes/Atoll.szs") == root / "Szenes" / "[[7]].szs");              // English
    CHECK(!game->resolve("Szenes/Kalt erwischt.szs"));
    CHECK(!game->resolve("Das Atoll.szs"));  // only scenarios
    CHECK(game->expand("[[7]]1") == "Das Atoll1");

    const auto table = assets::TextTable::load(*game);
    REQUIRE(table.ok());
    const std::vector<std::string> leiste{"Musik", "Spieloptionen anzeigen", "Ende"};
    CHECK(table->section("LEISTE") == leiste);

    game->choose_language("en");
    CHECK(game->text_language() == "en");
    CHECK(game->expand("[[7]]") == "Atoll");

    // Other editions: nothing changes.
    std::filesystem::remove_all(root / "ahe");
    std::filesystem::remove_all(root / "Loca");
    auto plain = assets::GameDirectory::open(root);
    REQUIRE(plain);
    CHECK(plain->history_texts() == nullptr);
    CHECK(plain->text_language().empty());
    CHECK(plain->expand("[[7]]") == "[[7]]");
    CHECK(!plain->resolve("Szenes/Das Atoll.szs"));
    std::filesystem::remove_all(root);
}
