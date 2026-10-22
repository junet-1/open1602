// SPDX-License-Identifier: GPL-3.0-or-later
//
// XXH64 as specified in xxHash's doc/xxhash_spec.md (Yann Collet, BSD-2-Clause).
#include "o1602/core/hash.hpp"

#include <bit>
#include <cstddef>
#include <cstring>

namespace o1602::core {

namespace {

constexpr std::uint64_t kPrime1 = 0x9E3779B185EBCA87ULL;
constexpr std::uint64_t kPrime2 = 0xC2B2AE3D27D4EB4FULL;
constexpr std::uint64_t kPrime3 = 0x165667B19E3779F9ULL;
constexpr std::uint64_t kPrime4 = 0x85EBCA77C2B2AE63ULL;
constexpr std::uint64_t kPrime5 = 0x27D4EB2F165667C5ULL;

/// The little-endian number of `sizeof(T)` bytes at `p`: one load on a little-endian processor.
template <class T>
T read_le(const std::uint8_t* p) noexcept {
    T v = 0;
    if constexpr (std::endian::native == std::endian::little) {
        std::memcpy(&v, p, sizeof v);
    } else {
        for (std::size_t k = 0; k < sizeof v; ++k) {
            v |= static_cast<T>(T{p[k]} << (8 * k));
        }
    }
    return v;
}

std::uint64_t read64(const std::uint8_t* p) noexcept {
    return read_le<std::uint64_t>(p);
}
std::uint32_t read32(const std::uint8_t* p) noexcept {
    return read_le<std::uint32_t>(p);
}

std::uint64_t mix_lane(std::uint64_t acc, std::uint64_t input) noexcept {
    acc += input * kPrime2;
    acc = std::rotl(acc, 31);
    return acc * kPrime1;
}

std::uint64_t merge(std::uint64_t acc, std::uint64_t lane) noexcept {
    acc ^= mix_lane(0, lane);
    return acc * kPrime1 + kPrime4;
}

}  // namespace

std::uint64_t xxh64(std::span<const std::uint8_t> data, std::uint64_t seed) noexcept {
    const std::uint8_t* p = data.data();
    std::size_t n = data.size();
    std::uint64_t h = 0;
    if (n >= 32) {
        // Four lanes side by side, 32 bytes a step.
        std::uint64_t v1 = seed + kPrime1 + kPrime2;
        std::uint64_t v2 = seed + kPrime2;
        std::uint64_t v3 = seed;
        std::uint64_t v4 = seed - kPrime1;
        do {
            v1 = mix_lane(v1, read64(p));
            v2 = mix_lane(v2, read64(p + 8));
            v3 = mix_lane(v3, read64(p + 16));
            v4 = mix_lane(v4, read64(p + 24));
            p += 32;
            n -= 32;
        } while (n >= 32);
        h = std::rotl(v1, 1) + std::rotl(v2, 7) + std::rotl(v3, 12) + std::rotl(v4, 18);
        h = merge(h, v1);
        h = merge(h, v2);
        h = merge(h, v3);
        h = merge(h, v4);
    } else {
        h = seed + kPrime5;
    }
    h += data.size();
    for (; n >= 8; p += 8, n -= 8) {
        h ^= mix_lane(0, read64(p));
        h = std::rotl(h, 27) * kPrime1 + kPrime4;
    }
    if (n >= 4) {
        h ^= std::uint64_t{read32(p)} * kPrime1;
        h = std::rotl(h, 23) * kPrime2 + kPrime3;
        p += 4;
        n -= 4;
    }
    for (; n > 0; ++p, --n) {
        h ^= std::uint64_t{*p} * kPrime5;
        h = std::rotl(h, 11) * kPrime1;
    }
    h ^= h >> 33;
    h *= kPrime2;
    h ^= h >> 29;
    h *= kPrime3;
    h ^= h >> 32;
    return h;
}

}  // namespace o1602::core
