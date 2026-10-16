// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>

namespace o1602::core {

/// Converts Windows-1252 (the encoding of all original text files) to UTF-8.
std::string cp1252_to_utf8(std::string_view input);

}  // namespace o1602::core
