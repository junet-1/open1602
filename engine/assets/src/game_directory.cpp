// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/game_directory.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <system_error>

#include "o1602/core/file.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace o1602::assets {

namespace fs = std::filesystem;

using core::ErrorCode;
using core::make_error;

namespace {

void add_if_folder(std::vector<fs::path>& out, const fs::path& folder) {
    std::error_code ec;
    if (!folder.empty() && fs::is_directory(folder, ec) && std::find(out.begin(), out.end(), folder) == out.end()) {
        out.push_back(folder);
    }
}

/// The folders in `parent` named after the game.
void add_named_below(std::vector<fs::path>& out, const fs::path& parent) {
    for (const fs::path& folder : GameDirectory::folders_named_1602(parent)) {
        add_if_folder(out, folder);
    }
}

std::optional<fs::path> environment_path(const char* name) {
    const auto value = core::environment_variable(name);
    if (!value) {
        return std::nullopt;
    }
    return core::path_from_utf8(GameDirectory::clean_path_text(*value));
}

#if defined(_WIN32)
/// The install folders GOG's installer records, one subkey per game with a "path" value.
void add_gog_registry(std::vector<fs::path>& out) {
    for (const wchar_t* games : {L"SOFTWARE\\WOW6432Node\\GOG.com\\Games", L"SOFTWARE\\GOG.com\\Games"}) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, games, 0, KEY_READ, &key) != ERROR_SUCCESS) {
            continue;
        }
        std::array<wchar_t, 256> subkey{};
        for (DWORD index = 0;; ++index) {
            auto subkey_size = static_cast<DWORD>(subkey.size());
            if (RegEnumKeyExW(key, index, subkey.data(), &subkey_size, nullptr, nullptr, nullptr, nullptr) !=
                ERROR_SUCCESS) {
                break;
            }
            std::array<wchar_t, MAX_PATH * 2> value{};
            auto value_size = static_cast<DWORD>(value.size() * sizeof(wchar_t));
            if (RegGetValueW(key, subkey.data(), L"path", RRF_RT_REG_SZ, nullptr, value.data(), &value_size) ==
                ERROR_SUCCESS) {
                add_if_folder(out, fs::path(value.data()));
            }
        }
        RegCloseKey(key);
    }
}
#endif

}  // namespace

std::optional<std::string_view> GameDirectory::missing_file(const fs::path& root) {
    // Files every supported edition contains. 1602.exe itself is not required.
    constexpr std::array<std::string_view, 4> kRequired = {"haeuser.cod", "figuren.cod", "toolgfx/stadtfld.col",
                                                           "gfx/stadtfld.bsh"};
    for (const std::string_view file : kRequired) {
        if (!core::find_case_insensitive(root, file)) {
            return file;
        }
    }
    return std::nullopt;
}

core::Result<GameDirectory> GameDirectory::open(const fs::path& root) {
    if (const auto file = missing_file(root)) {
        return make_error(ErrorCode::NotFound, core::path_to_utf8(root) +
                                                   " does not look like an Anno 1602 installation (missing " +
                                                   std::string(*file) + ")");
    }
    GameDirectory dir;
    dir.m_root = root;
    return dir;
}

core::Result<GameDirectory> GameDirectory::from_environment() {
    const auto root = environment_path(kEnvVar);
    if (!root || root->empty()) {
        return make_error(ErrorCode::NotFound, std::string("environment variable ") + kEnvVar + " is not set");
    }
    return open(*root);
}

std::vector<fs::path> GameDirectory::installation_candidates() {
    std::vector<fs::path> out;
#if defined(_WIN32)
    add_gog_registry(out);
    const fs::path drive = environment_path("SystemDrive").value_or(fs::path("C:"));
    add_named_below(out, drive / "/GOG Games");  // "C:" / "/GOG Games" is C:\GOG Games
    for (const char* program_files : {"ProgramFiles(x86)", "ProgramFiles"}) {
        if (const auto folder = environment_path(program_files)) {
            add_named_below(out, *folder / "GOG Galaxy" / "Games");
            add_named_below(out, *folder / "GOG Games");
        }
    }
#else
    if (const auto home = environment_path("HOME"); home && !home->empty()) {
        add_named_below(out, *home / "GOG Games");
        add_named_below(out, *home / "Games" / "Heroic");  // Heroic Games Launcher
        add_named_below(out, *home / "Games");
        for (const char* windows_folder : {"GOG Games", "Program Files (x86)/GOG Galaxy/Games"}) {
            add_named_below(out, *home / ".wine" / "drive_c" / windows_folder);
        }
    }
#endif
    return out;
}

std::vector<fs::path> GameDirectory::folders_named_1602(const fs::path& parent) {
    std::vector<fs::path> out;
    std::error_code ec;
    for (fs::directory_iterator it(parent, ec), last; !ec && it != last; it.increment(ec)) {
        if (it->is_directory(ec) && core::path_to_utf8(it->path().filename()).find("1602") != std::string::npos) {
            out.push_back(it->path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string GameDirectory::clean_path_text(std::string_view text) {
    const auto trim = [](std::string_view& s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
            s.remove_prefix(1);
        }
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
            s.remove_suffix(1);
        }
    };
    trim(text);
    // Quotes never belong to a Windows path; each end is stripped on its own (see the header).
    if (!text.empty() && text.front() == '"') {
        text.remove_prefix(1);
    }
    if (!text.empty() && text.back() == '"') {
        text.remove_suffix(1);
    }
    trim(text);
    return std::string(text);
}

std::optional<fs::path> GameDirectory::resolve(std::string_view relative) const {
    return core::find_case_insensitive(m_root, relative);
}

}  // namespace o1602::assets
