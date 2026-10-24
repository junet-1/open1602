// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/game_content.hpp"

#include <algorithm>
#include <system_error>

#include "o1602/core/file.hpp"
#include "o1602/core/hash.hpp"

namespace o1602::assets {

std::uint64_t content_fingerprint(const GameDirectory& game) {
    std::uint64_t h = 0;
    for (const char* file : {"haeuser.cod", "figuren.cod"}) {
        if (const auto path = game.resolve(file)) {
            if (const auto bytes = core::read_file(*path)) {
                h = core::xxh64(*bytes, h);
                continue;
            }
        }
        h = core::xxh64({}, h ^ 0x9E3779B97F4A7C15ULL);  // a missing file is not an empty one
    }
    return h;
}

std::vector<ModPackage> mod_packages(const std::filesystem::path& mods) {
    std::vector<ModPackage> out;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(mods, ec)) {
        if (entry.is_directory(ec)) {
            out.push_back(ModPackage{core::path_to_utf8(entry.path().filename()), entry.path()});
        }
    }
    std::sort(out.begin(), out.end(), [](const ModPackage& a, const ModPackage& b) { return a.id < b.id; });
    return out;
}

}  // namespace o1602::assets
