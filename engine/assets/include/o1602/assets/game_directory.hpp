// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "o1602/core/result.hpp"

namespace o1602::assets {

/// Access to an original Anno 1602 installation (e.g. GOG "Koenigsedition" or Ubisoft's "History
/// Edition"). The engine never ships original data; the user points it to their own installation.
///
/// The History Edition keeps the files of a language in Loca/<language>/ (de-DE: Text.cod, the
/// speech, the menu art, the videos), with the English ones in the installation's own folders.
/// resolve() looks into that language folder first, so the game speaks the installed language.
class GameDirectory {
public:
    /// Environment variable that may contain the installation path.
    static constexpr const char* kEnvVar = "ANNO1602_DIR";

    /// Validates that `root` looks like an Anno 1602 installation. Of the History Edition's language
    /// folders it takes that of the system's language (see language_folder()).
    static core::Result<GameDirectory> open(const std::filesystem::path& root);

    /// The first file of an installation that `root` lacks ("haeuser.cod", ...), or nothing if it
    /// has them all. open() fails exactly when there is one.
    [[nodiscard]] static std::optional<std::string_view> missing_file(const std::filesystem::path& root);

    /// Opens the directory named by ANNO1602_DIR.
    static core::Result<GameDirectory> from_environment();

    /// Folders where installers usually put the game, most likely first: GOG's registry entries on
    /// Windows, then the default folders of GOG, GOG Galaxy and the CD's installer; elsewhere those of
    /// home_candidates(). Only folders that exist; none of them is checked to be an installation.
    [[nodiscard]] static std::vector<std::filesystem::path> installation_candidates();

    /// The candidates below a home folder (macOS, Linux): GOG, Heroic and Lutris (~/Games), the
    /// Applications folders, and the Windows folders (GOG's, Ubisoft Connect's, the CD's) in the
    /// prefixes of Wine, CrossOver, Bottles and Heroic.
    [[nodiscard]] static std::vector<std::filesystem::path> home_candidates(const std::filesystem::path& home);

    /// The folders directly in `parent` whose name contains "1602", sorted by name.
    [[nodiscard]] static std::vector<std::filesystem::path> folders_named_1602(const std::filesystem::path& parent);

    /// The History Edition's language folder below `root` (Loca/de-DE, ...) that fits `language`
    /// best: the one of that language ("de" fits "de-DE"), else the one of the system's language,
    /// else English, else the first by name. Nothing when `root` has no Loca folder.
    [[nodiscard]] static std::optional<std::filesystem::path> language_folder(const std::filesystem::path& root,
                                                                               std::string_view language = {});

    /// A path as typed by the user, without surrounding blanks and quotes: `set VAR="C:\a b"` in cmd
    /// keeps the quotes in the value, and `"C:\a b\"` reaches a program as `C:\a b"`.
    [[nodiscard]] static std::string clean_path_text(std::string_view text);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return m_root; }

    /// Takes the History Edition's language folder of `language` ("de", "en-US", ...) instead, if
    /// the installation has one; other editions stay as they are.
    void choose_language(std::string_view language);

    /// The History Edition's language folder in use (Loca/de-DE, ...); empty for other editions.
    [[nodiscard]] const std::filesystem::path& localized_root() const noexcept { return m_localized; }

    /// Resolves a path as used by the original game (case-insensitive, '\\' or '/'): in the language
    /// folder if it has it, else in the installation.
    [[nodiscard]] std::optional<std::filesystem::path> resolve(std::string_view relative) const;

private:
    std::filesystem::path m_root;
    std::filesystem::path m_localized;
};

}  // namespace o1602::assets
