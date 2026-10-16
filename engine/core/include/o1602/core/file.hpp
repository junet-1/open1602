// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "o1602/core/result.hpp"

namespace o1602::core {

using Bytes = std::vector<std::uint8_t>;

/// Reads a whole file into memory.
Result<Bytes> read_file(const std::filesystem::path& path);

/// Writes a whole file (creates or truncates).
Status write_file(const std::filesystem::path& path, std::span<const std::uint8_t> data);

/// Resolves a relative path below `root` ignoring the case of every path component.
///
/// The original game was written for Windows (case-insensitive file system) and references
/// files with inconsistent casing, e.g. "mgfx\stadtfld.bsh" while the file is "MGFX/Stadtfld.bsh".
/// Both '/' and '\\' are accepted as separators in `relative`.
/// An exact match is tried first, so the common case does not touch the directory listing.
std::optional<std::filesystem::path> find_case_insensitive(const std::filesystem::path& root,
                                                           std::string_view relative);

}  // namespace o1602::core
