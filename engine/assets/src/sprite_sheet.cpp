// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/sprite_sheet.hpp"

#include <algorithm>

#include "o1602/core/byte_reader.hpp"

namespace o1602::assets {

using core::ErrorCode;
using core::make_error;

namespace {
constexpr std::uint32_t kRecordHeaderSize = 16;
constexpr std::uint8_t kEndOfImage = 0xFF;
constexpr std::uint8_t kEndOfRow = 0xFE;
// Sanity limit: the largest original sprites are full-screen images (1024x768).
constexpr std::uint32_t kMaxDimension = 4096;
}  // namespace

std::vector<std::uint8_t> IndexedImage::to_rgba(const Palette& palette) const {
    std::vector<std::uint8_t> rgba(indices.size() * 4, 0);
    for (std::size_t i = 0; i < indices.size(); ++i) {
        if (opaque[i] != 0) {
            const Rgba& c = palette[indices[i]];
            rgba[i * 4 + 0] = c.r;
            rgba[i * 4 + 1] = c.g;
            rgba[i * 4 + 2] = c.b;
            rgba[i * 4 + 3] = 255;
        }
    }
    return rgba;
}

core::Result<SpriteSheet> SpriteSheet::from_chunk_file(ChunkFile file) {
    SpriteSheet sheet;
    sheet.m_file = std::move(file);
    const Chunk* chunk = sheet.m_file.find("BSH");
    if (chunk == nullptr) {
        chunk = sheet.m_file.find("ZEI");
    }
    if (chunk == nullptr) {
        return make_error(ErrorCode::InvalidFormat, "no BSH or ZEI chunk");
    }
    sheet.m_chunk = *chunk;

    const auto data = sheet.m_file.payload(sheet.m_chunk);
    if (data.size() < 4) {
        return make_error(ErrorCode::InvalidFormat, "sprite chunk too small");
    }

    // The payload starts with a table of uint32 offsets. The table has no explicit count;
    // it ends where the first (lowest) sprite record begins.
    std::size_t count = core::ByteReader::load_u32(data, 0) / 4;
    for (std::size_t i = 0; i < count; ++i) {
        if ((i + 1) * 4 > data.size()) {
            return make_error(ErrorCode::InvalidFormat, "offset table exceeds chunk");
        }
        const std::uint32_t offset = core::ByteReader::load_u32(data, i * 4);
        count = std::min<std::size_t>(count, offset / 4);
    }

    sheet.m_infos.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        SpriteInfo info;
        info.offset = core::ByteReader::load_u32(data, i * 4);
        if (std::size_t{info.offset} + kRecordHeaderSize > data.size()) {
            return make_error(ErrorCode::InvalidFormat, "sprite " + std::to_string(i) + " outside of chunk");
        }
        info.width = core::ByteReader::load_u32(data, info.offset);
        info.height = core::ByteReader::load_u32(data, info.offset + 4);
        info.type = core::ByteReader::load_u32(data, info.offset + 8);
        info.length = core::ByteReader::load_u32(data, info.offset + 12);
        if (info.width > kMaxDimension || info.height > kMaxDimension) {
            return make_error(ErrorCode::InvalidFormat, "sprite " + std::to_string(i) + " has invalid size");
        }
        sheet.m_infos.push_back(info);
    }
    return sheet;
}

core::Result<SpriteSheet> SpriteSheet::load(const std::filesystem::path& path) {
    auto file = ChunkFile::load(path);
    if (!file) {
        return file.error();
    }
    auto sheet = from_chunk_file(std::move(file).value());
    if (!sheet) {
        return make_error(sheet.error().code, path.string() + ": " + sheet.error().message);
    }
    return sheet;
}

core::Result<IndexedImage> SpriteSheet::decode(std::size_t index) const {
    if (index >= m_infos.size()) {
        return make_error(ErrorCode::OutOfRange, "sprite index out of range");
    }
    const SpriteInfo& info = m_infos[index];
    const auto data = m_file.payload(m_chunk);

    IndexedImage image;
    image.width = info.width;
    image.height = info.height;
    const std::size_t pixel_count = std::size_t{info.width} * info.height;
    image.indices.assign(pixel_count, 0);
    image.opaque.assign(pixel_count, 0);

    // RLE stream: repeated [skip][count][count palette indices], 0xFE = next row, 0xFF = end.
    core::ByteReader reader(data);
    (void)reader.seek(info.offset + kRecordHeaderSize);
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    for (;;) {
        const auto code = reader.u8();
        if (!code) {
            return make_error(ErrorCode::InvalidFormat, "sprite " + std::to_string(index) + ": unterminated RLE data");
        }
        if (*code == kEndOfImage) {
            break;
        }
        if (*code == kEndOfRow) {
            x = 0;
            ++y;
            continue;
        }
        x += *code;
        const auto run = reader.u8();
        if (!run) {
            return make_error(ErrorCode::InvalidFormat, "sprite " + std::to_string(index) + ": truncated run");
        }
        const auto pixels = reader.bytes(*run);
        if (!pixels) {
            return make_error(ErrorCode::InvalidFormat, "sprite " + std::to_string(index) + ": truncated pixels");
        }
        for (const std::uint8_t color : *pixels) {
            if (x < info.width && y < info.height) {
                const std::size_t i = std::size_t{y} * info.width + x;
                image.indices[i] = color;
                image.opaque[i] = 1;
            } else {
                ++image.clipped_pixels;
            }
            ++x;
        }
    }
    return image;
}

}  // namespace o1602::assets
