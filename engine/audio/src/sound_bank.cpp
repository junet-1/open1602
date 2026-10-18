// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/audio/sound_bank.hpp"

#include <algorithm>
#include <cctype>

#include "o1602/core/file.hpp"

namespace o1602::audio {

std::shared_ptr<const Pcm> SoundBank::get(std::string_view relative) {
    std::string key(relative);
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return c == '\\' ? '/' : static_cast<char>(std::tolower(c)); });
    if (const auto it = m_cache.find(key); it != m_cache.end()) {
        return it->second;
    }
    auto sound = load(relative);
    m_cache.emplace(std::move(key), sound);  // a missing file is remembered too
    return sound;
}

std::shared_ptr<const Pcm> SoundBank::load(std::string_view relative) const {
    const auto path = m_game->resolve(relative);
    if (!path) {
        m_error = std::string(relative) + ": not found";
        return nullptr;
    }
    auto bytes = core::read_file(*path);
    if (!bytes) {
        m_error = bytes.error().message;
        return nullptr;
    }
    auto pcm = decode_wav(*bytes);
    if (!pcm) {
        m_error = std::string(relative) + ": " + pcm.error().message;
        return nullptr;
    }
    return std::make_shared<const Pcm>(std::move(pcm).value());
}

}  // namespace o1602::audio
