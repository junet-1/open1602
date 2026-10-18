// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/core/compress.hpp"

#include <array>
#include <limits>

#if defined(O1602_HAVE_ZLIB)
#include <zlib.h>
#endif

namespace o1602::core {

bool has_zlib() noexcept {
#if defined(O1602_HAVE_ZLIB)
    return true;
#else
    return false;
#endif
}

Result<std::vector<std::uint8_t>> zlib_deflate(std::span<const std::uint8_t> data) {
#if defined(O1602_HAVE_ZLIB)
    if (data.size() > std::numeric_limits<uLong>::max()) {
        return make_error(ErrorCode::OutOfRange, "zlib: data too large");
    }
    uLongf size = compressBound(static_cast<uLong>(data.size()));
    std::vector<std::uint8_t> out(size);
    if (compress2(out.data(), &size, data.data(), static_cast<uLong>(data.size()), 6) != Z_OK) {
        return make_error(ErrorCode::InvalidFormat, "zlib: compression failed");
    }
    out.resize(size);
    return out;
#else
    (void)data;
    return make_error(ErrorCode::Unsupported, "zlib: this build of Open1602 has no zlib");
#endif
}

Result<std::vector<std::uint8_t>> zlib_inflate(std::span<const std::uint8_t> stream, std::size_t size) {
#if defined(O1602_HAVE_ZLIB)
    if (stream.size() > std::numeric_limits<uLong>::max() || size > std::numeric_limits<uLong>::max()) {
        return make_error(ErrorCode::OutOfRange, "zlib: data too large");
    }
    std::vector<std::uint8_t> out(size);
    uLongf got = static_cast<uLongf>(size);
    const int status = uncompress(out.data(), &got, stream.data(), static_cast<uLong>(stream.size()));
    if (status != Z_OK || got != size) {
        return make_error(ErrorCode::InvalidFormat, "zlib: damaged stream");
    }
    return out;
#else
    (void)stream;
    (void)size;
    return make_error(ErrorCode::Unsupported, "zlib: this build of Open1602 has no zlib");
#endif
}

namespace {

constexpr std::array<std::uint32_t, 256> make_crc_table() noexcept {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1U) != 0 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
        }
        table[n] = c;
    }
    return table;
}

constexpr std::array<std::uint32_t, 256> kCrcTable = make_crc_table();

}  // namespace

std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept {
    std::uint32_t c = 0xFFFFFFFFU;
    for (const std::uint8_t b : data) {
        c = kCrcTable[(c ^ b) & 0xFFU] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFU;
}

}  // namespace o1602::core
