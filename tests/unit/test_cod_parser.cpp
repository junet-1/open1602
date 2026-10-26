// SPDX-License-Identifier: GPL-3.0-or-later
// Tests for the .cod interpreter. Each case pins down one behaviour of the original
// interpreter documented in docs/formats/cod.md.

#include <string>

#include "../test_framework.hpp"

#include "o1602/assets/cod_parser.hpp"

using namespace o1602::assets;

namespace {

const CodPredefinedSymbol kObjectTypes[] = {{"HAUS", 1}, {"HAUS_PRODTYP", 2}};

std::int32_t sym_int(const cod::Document& doc, const char* name, std::size_t slot = 0) {
    const cod::Symbol* s = doc.symbols.find(name);
    return s == nullptr ? -999999 : s->values[slot].integer;
}

}  // namespace

TEST_CASE("cod: constants and left-to-right evaluation without precedence") {
    const auto doc = cod::parse("A = 2+3*4\nB = A-1\nC = 10/3\nD = 6|1&3\n");
    CHECK(sym_int(doc, "A") == 20);  // (2+3)*4
    CHECK(sym_int(doc, "B") == 19);
    CHECK(sym_int(doc, "C") == 3);
    CHECK(sym_int(doc, "D") == 3);  // (6|1)&3
    CHECK(doc.diagnostics.empty());
}

TEST_CASE("cod: numbers in decimal, real, hex and binary notation") {
    const auto doc = cod::parse("R: 1.3\nH: $1F\nB: %101\nN: -5\n");
    const cod::Symbol* r = doc.symbols.find("R");
    REQUIRE(r != nullptr);
    CHECK(r->values[0].integer == 1);  // ftol truncation
    CHECK(r->values[0].real > 1.29 && r->values[0].real < 1.31);
    CHECK(sym_int(doc, "H") == 31);
    CHECK(sym_int(doc, "B") == 5);
    CHECK(sym_int(doc, "N") == -5);  // 0 - 5
}

TEST_CASE("cod: the History Edition's colour mark after a number is dropped") {
    // TOOLS.INC of the History Edition: "COL_RED = 249c"; its game reads 249.
    const auto doc = cod::parse("COL_RED = 249c\nCOL_BLK = 0C ; black\nA: 7c, 8c\nB: 2cm\n");
    CHECK(sym_int(doc, "COL_RED") == 249);
    CHECK(sym_int(doc, "COL_BLK") == 0);
    const cod::Symbol* a = doc.symbols.find("A");
    REQUIRE(a != nullptr);
    CHECK(a->values[0].integer == 7);
    CHECK(a->values[1].integer == 8);
    CHECK(sym_int(doc, "B") == 0);  // a name "cm" as before, not a mark
    CHECK(doc.diagnostics.size() == 1);
}

TEST_CASE("cod: numbers beyond 32 bits stay at the largest, on every platform") {
    // The original's strtol() has a 32-bit long; Linux and macOS have 64 bits.
    const auto doc = cod::parse(
        "D: 4294967297\nH: $100000001\nB: %111111111111111111111111111111111\n"
        "E: 2147483647\nF: $7FFFFFFF\nG: 99999999999999999999999999\n");
    for (const char* name : {"D", "H", "B", "E", "F", "G"}) {
        CHECK(sym_int(doc, name) == 2147483647);
    }
    CHECK(sym_int(cod::parse("A: $ffff\n"), "A") == 65535);
}

TEST_CASE("cod: real numbers round like strtod(), whatever the locale") {
    const auto doc = cod::parse("A: 0.1\nB: 1.3\nC: 2.\nD: 0.0125\nE: 123456.789\n");
    const auto real = [&](const char* name) { return doc.symbols.find(name)->values[0].real; };
    CHECK(real("A") == 0.1);
    CHECK(real("B") == 1.3);
    CHECK(real("C") == 2.0);
    CHECK(real("D") == 0.0125);
    CHECK(real("E") == 123456.789);
    CHECK(doc.symbols.find("E")->values[0].integer == 123456);
}

TEST_CASE("cod: comma separated values and value count") {
    const auto doc = cod::parse("Wegspeed: 145, 120, 170, 100 ; comment\n");
    const cod::Symbol* s = doc.symbols.find("Wegspeed");
    REQUIRE(s != nullptr);
    CHECK(s->count == 4);
    CHECK(s->values[0].integer == 145);
    CHECK(s->values[3].integer == 100);
    REQUIRE(doc.root.field("Wegspeed") != nullptr);
    CHECK(doc.root.field("Wegspeed")->values.size() == 4);
}

TEST_CASE("cod: '@' evaluates relative to the previous symbol value") {
    const auto doc = cod::parse("Gfx: 100\n@Gfx: +40\n@Gfx: -36\nX: 7\nX: +1\n");
    CHECK(sym_int(doc, "Gfx") == 104);
    CHECK(sym_int(doc, "X") == 1);  // without '@' the target is reset before evaluation
}

TEST_CASE("cod: identifiers on the right side use symbol values, undefined ones are 0") {
    const auto doc = cod::parse("BASE = 20101\nId: BASE+2\nKind: UNKNOWN_THING\n");
    CHECK(sym_int(doc, "Id") == 20103);
    CHECK(sym_int(doc, "Kind") == 0);
    REQUIRE(doc.diagnostics.size() == 1);
    CHECK(doc.diagnostics[0].line == 3);
}

TEST_CASE("cod: a thousand problems are listed, the rest only counted once (HI-3)") {
    std::string text;
    for (int i = 0; i < 5000; ++i) {
        text += "Kind: UNDEFINED_" + std::to_string(i) + "\n";
    }
    const auto doc = cod::parse(text);
    REQUIRE(doc.diagnostics.size() == 1001);
    CHECK(doc.diagnostics[999].line == 1000);
    CHECK(doc.diagnostics.back().message.find("not listed") != std::string::npos);
}

TEST_CASE("cod: predefined symbols") {
    const CodPredefinedSymbol predefined[] = {{"BODEN", 11}, {"MAXHAUS", 500}};
    const auto doc = cod::parse("Kind: BODEN\nM: MAXHAUS-1\n", predefined);
    CHECK(sym_int(doc, "Kind") == 11);
    CHECK(sym_int(doc, "M") == 499);
    CHECK(doc.diagnostics.empty());
}

TEST_CASE("cod: strings and concatenation") {
    const auto doc = cod::parse("S: \"abc\"\nT: S + \"def\"\n");
    const cod::Symbol* t = doc.symbols.find("T");
    REQUIRE(t != nullptr);
    CHECK(t->values[0].type == cod::ValueType::String);
    CHECK(t->values[0].text == "abcdef");
}

TEST_CASE("cod: an operator at the end of a line continues on the next line") {
    const auto doc = cod::parse("A: 1 +\n   2\nB: 5\n");
    CHECK(sym_int(doc, "A") == 3);
    CHECK(sym_int(doc, "B") == 5);
}

TEST_CASE("cod: subscript selects the value slot") {
    const auto doc = cod::parse("P: 1, 2, 3\nQ: P[2]\n");
    CHECK(sym_int(doc, "Q") == 3);
}

TEST_CASE("cod: Objekt / Nummer / EndObj build numbered records") {
    const auto doc = cod::parse(
        "Nahrung: 1.3\n"
        "Objekt: HAUS\n"
        "  @Nummer: 0\n"
        "  Id: 7\n"
        "  @Nummer: +1\n"
        "  Id: 8\n"
        "  Objekt: HAUS_PRODTYP\n"
        "    Ware: 3\n"
        "  EndObj;\n"
        "  @Nummer: +1\n"
        "  Id: 9\n"
        "EndObj;\n",
        kObjectTypes);
    CHECK(doc.diagnostics.empty());
    CHECK(doc.root.field("Nahrung") != nullptr);
    const auto* haus = doc.root.records("HAUS");
    REQUIRE(haus != nullptr);
    REQUIRE(haus->size() == 3);
    CHECK(haus->at(0).integer("Id") == 7);
    CHECK(haus->at(1).integer("Id") == 8);
    CHECK(haus->at(2).integer("Id") == 9);  // Nummer restored to 1 by EndObj, then +1
    const auto* prod = haus->at(1).records("HAUS_PRODTYP");
    REQUIRE(prod != nullptr);
    CHECK(prod->at(0).integer("Ware") == 3);
}

TEST_CASE("cod: ObjFill copies whole records, relative values follow the symbol") {
    const auto doc = cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n"
        "  Size: 1, 1\n"
        "  Gfx: 0\n"
        "  Objekt: HAUS_PRODTYP\n"
        "    Ware: 0\n"
        "  EndObj;\n"
        "  ObjFill: 0, 4\n"  // defaults for records 0..3
        "  @Nummer: +1\n"
        "  Gfx: 100\n"
        "  BASE = Nummer\n"
        "  @Nummer: +1\n"
        "  ObjFill: BASE\n"  // copy record 1 (Gfx 100)
        "  @Gfx: +40\n"      // relative to the *symbol* Gfx (100), not the record
        "EndObj;\n",
        kObjectTypes);
    const auto* haus = doc.root.records("HAUS");
    REQUIRE(haus != nullptr);
    CHECK(haus->size() == 4);
    CHECK(haus->at(3).integer("Size", 1) == 1);  // filled with defaults
    CHECK(haus->at(3).records("HAUS_PRODTYP") != nullptr);
    CHECK(haus->at(1).integer("Gfx") == 100);
    CHECK(haus->at(2).integer("Gfx") == 140);
    CHECK(haus->at(2).field("BASE") != nullptr);  // '=' assignments are stored on the record and copied too
    CHECK(haus->at(2).field("BASE")->assigned_with_equals);
}

TEST_CASE("cod: nested ObjFill stays within a record budget and is refused as a whole") {
    // 1 000 children per house, then 1 000 houses: a million records from a few lines (HI-2).
    const auto doc = cod::parse(
        "Objekt: HAUS\n"
        "  @Nummer: 0\n"
        "  Objekt: HAUS_PRODTYP\n"
        "    @Nummer: 0\n"
        "    Ware: 1\n"
        "    ObjFill: 0, 1000\n"
        "  EndObj;\n"
        "  @Nummer: 1\n"
        "  ObjFill: 0, 1000\n"
        "  @Nummer: 2000\n"
        "  ObjFill: 0, 5\n"  // later fills are refused too
        "EndObj;\n",
        kObjectTypes);
    const auto* haus = doc.root.records("HAUS");
    REQUIRE(haus != nullptr);
    CHECK(haus->size() == 1);  // the big fill made nothing, rather than part of its range
    REQUIRE(haus->at(0).records("HAUS_PRODTYP") != nullptr);
    CHECK(haus->at(0).records("HAUS_PRODTYP")->size() == 1000);
    CHECK(doc.diagnostics.size() == 1);
}

TEST_CASE("cod: structural errors are reported, not fatal") {
    const auto doc = cod::parse("EndObj;\nObjFill: 0\nObjekt: HAUS\nX: 1 / 0\n", kObjectTypes);
    CHECK(doc.diagnostics.size() == 4);  // EndObj, ObjFill at root, division by zero, unclosed block
    CHECK(sym_int(doc, "X") == 1);
}

TEST_CASE("cod: CRLF line endings and stray characters are ignored") {
    const auto doc = cod::parse("A: 1\r\n\tB:\t2 ]\r\n");
    CHECK(sym_int(doc, "A") == 1);
    CHECK(sym_int(doc, "B") == 2);
}

TEST_CASE("cod: repeated statements are kept in the history") {
    const auto doc = cod::parse("Soldat: 1, 5, 80\nSoldat: 2, 8, 130\n");
    CHECK(doc.root.integer("Soldat") == 2);  // field = last assignment
    REQUIRE(doc.root.history.size() == 2);
    CHECK(doc.root.history[0].first == "Soldat");
    CHECK(doc.root.history[0].second[2].integer == 80);
    CHECK(doc.root.history[1].second[1].integer == 8);
}
