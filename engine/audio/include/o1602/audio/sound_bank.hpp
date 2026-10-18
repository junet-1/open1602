// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sound files of the game directory, decoded once and kept (SAMPLES are small; music is decoded
// when its track starts and not kept).
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "o1602/assets/game_directory.hpp"
#include "o1602/audio/wav.hpp"

namespace o1602::audio {

class SoundBank {
public:
    explicit SoundBank(const assets::GameDirectory& game) : m_game(&game) {}

    /// The sound at a path relative to the game directory (case does not matter), decoded and kept;
    /// nullptr if it is missing or cannot be decoded (once reported in `last_error`).
    std::shared_ptr<const Pcm> get(std::string_view relative);
    /// Decodes a file without keeping it (music).
    [[nodiscard]] std::shared_ptr<const Pcm> load(std::string_view relative) const;
    [[nodiscard]] const std::string& last_error() const noexcept { return m_error; }

private:
    const assets::GameDirectory* m_game;
    std::unordered_map<std::string, std::shared_ptr<const Pcm>> m_cache;
    mutable std::string m_error;
};

}  // namespace o1602::audio
