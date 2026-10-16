// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "o1602/core/result.hpp"

namespace o1602::assets {

/// Access to an original Anno 1602 installation (e.g. GOG "Koenigsedition").
/// The engine never ships original data; the user points it to their own installation.
class GameDirectory {
public:
    /// Environment variable that may contain the installation path.
    static constexpr const char* kEnvVar = "ANNO1602_DIR";

    /// Validates that `root` looks like an Anno 1602 installation.
    static core::Result<GameDirectory> open(const std::filesystem::path& root);

    /// Opens the directory named by ANNO1602_DIR.
    static core::Result<GameDirectory> from_environment();

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return m_root; }

    /// Resolves a path as used by the original game (case-insensitive, '\\' or '/').
    [[nodiscard]] std::optional<std::filesystem::path> resolve(std::string_view relative) const;

private:
    std::filesystem::path m_root;
};

}  // namespace o1602::assets
