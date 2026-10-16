// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/game_directory.hpp"

#include <array>
#include <cstdlib>
#include <string>

#include "o1602/core/file.hpp"

namespace o1602::assets {

using core::ErrorCode;
using core::make_error;

core::Result<GameDirectory> GameDirectory::open(const std::filesystem::path& root) {
    // Files every supported edition contains. 1602.exe itself is not required.
    constexpr std::array kRequired = {"haeuser.cod", "figuren.cod", "toolgfx/stadtfld.col", "gfx/stadtfld.bsh"};
    for (const char* file : kRequired) {
        if (!core::find_case_insensitive(root, file)) {
            return make_error(ErrorCode::NotFound,
                              root.string() + " does not look like an Anno 1602 installation (missing " + file + ")");
        }
    }
    GameDirectory dir;
    dir.m_root = root;
    return dir;
}

core::Result<GameDirectory> GameDirectory::from_environment() {
    const char* value = std::getenv(kEnvVar);  // NOLINT(concurrency-mt-unsafe)
    if (value == nullptr || *value == '\0') {
        return make_error(ErrorCode::NotFound, std::string("environment variable ") + kEnvVar + " is not set");
    }
    return open(value);
}

std::optional<std::filesystem::path> GameDirectory::resolve(std::string_view relative) const {
    return core::find_case_insensitive(m_root, relative);
}

}  // namespace o1602::assets
