// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/core/text.hpp"

#include <array>
#include <cstdint>

namespace o1602::core {

namespace {

// Unicode code points for bytes 0x80..0x9F in Windows-1252 (0 = undefined).
constexpr std::array<std::uint16_t, 32> kCp1252High = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0,      0x017D, 0,      0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178,
};

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

}  // namespace

std::string cp1252_to_utf8(std::string_view input) {
    std::string out;
    out.reserve(input.size() + input.size() / 8);
    for (const char ch : input) {
        const auto b = static_cast<std::uint8_t>(ch);
        std::uint32_t cp = b;
        if (b >= 0x80 && b <= 0x9F) {
            cp = kCp1252High[b - 0x80];
            if (cp == 0) {
                cp = 0xFFFD;  // replacement character for undefined bytes
            }
        }
        append_utf8(out, cp);
    }
    return out;
}

}  // namespace o1602::core
