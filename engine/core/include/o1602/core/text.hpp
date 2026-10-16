// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>

namespace o1602::core {

/// Converts Windows-1252 (the encoding of all original text files) to UTF-8.
std::string cp1252_to_utf8(std::string_view input);

/// Compares two strings, ignoring the case of ASCII letters (file and savegame names).
[[nodiscard]] constexpr bool equals_ignore_case(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i];
        char y = b[i];
        if (x >= 'A' && x <= 'Z') {
            x = static_cast<char>(x - 'A' + 'a');
        }
        if (y >= 'A' && y <= 'Z') {
            y = static_cast<char>(y - 'A' + 'a');
        }
        if (x != y) {
            return false;
        }
    }
    return true;
}

}  // namespace o1602::core
