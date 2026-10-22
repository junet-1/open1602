// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

    /// ANNO1602_DIR when it is set, otherwise the first installation in installation_candidates().
    static core::Result<GameDirectory> locate();

    /// Folders where installers usually put the game, most likely first: GOG's registry entries on
    /// Windows, then the default folders of GOG, GOG Galaxy, Heroic and Wine. Only folders that exist;
    /// none of them is checked to be an installation.
    [[nodiscard]] static std::vector<std::filesystem::path> installation_candidates();

    /// The folders directly in `parent` whose name contains "1602", sorted by name.
    [[nodiscard]] static std::vector<std::filesystem::path> folders_named_1602(const std::filesystem::path& parent);

    /// A path as typed by the user, without surrounding blanks and quotes: `set VAR="C:\a b"` in cmd
    /// keeps the quotes in the value, and `"C:\a b\"` reaches a program as `C:\a b"`.
    [[nodiscard]] static std::string clean_path_text(std::string_view text);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return m_root; }

    /// Resolves a path as used by the original game (case-insensitive, '\\' or '/').
    [[nodiscard]] std::optional<std::filesystem::path> resolve(std::string_view relative) const;

private:
    std::filesystem::path m_root;
};

}  // namespace o1602::assets
