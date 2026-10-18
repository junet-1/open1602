// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sounds of the original: RIFF WAVE files (docs/formats/wav.md). SAMPLES and Speech8 hold
// 8-bit mono PCM, MUSIC8 4-bit stereo music.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "o1602/core/result.hpp"

namespace o1602::audio {

/// Decoded sound: 16-bit samples, interleaved when stereo.
struct Pcm {
    std::uint32_t rate = 22050;
    std::uint8_t channels = 1;
    std::vector<std::int16_t> samples;

    [[nodiscard]] std::size_t frames() const noexcept { return channels == 0 ? 0 : samples.size() / channels; }
    /// Length in milliseconds.
    [[nodiscard]] std::uint64_t duration_ms() const noexcept {
        return rate == 0 ? 0 : std::uint64_t{frames()} * 1000 / rate;
    }
};

/// Decodes a WAVE file. Fails on damaged files and encodings it does not know.
[[nodiscard]] core::Result<Pcm> decode_wav(std::span<const std::uint8_t> bytes);

}  // namespace o1602::audio
