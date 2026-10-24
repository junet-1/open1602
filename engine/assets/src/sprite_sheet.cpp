// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/assets/sprite_sheet.hpp"

#include <algorithm>

#include "o1602/core/byte_reader.hpp"
#include "o1602/core/file.hpp"

namespace o1602::assets {

using core::ErrorCode;
using core::make_error;

namespace {
constexpr std::uint32_t kRecordHeaderSize = 16;
constexpr std::uint8_t kEndOfImage = 0xFF;
constexpr std::uint8_t kEndOfRow = 0xFE;
// Sanity limit: the largest original sprites are full-screen images (1024x768).
constexpr std::uint32_t kMaxDimension = 4096;
/// Pixels all sprites of one sheet may claim together (HI-18): 256 M, far above the original's
/// largest sheets. Decoding takes two bytes per pixel, and a font keeps all its glyphs.
constexpr std::uint64_t kMaxSheetPixels = std::uint64_t{256} << 20;
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
    std::uint64_t pixels = 0;
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
        // Every row but the last ends with a byte of its own (0xFE): a sprite cannot have more rows
        // than bytes follow it (HI-7, HI-18). Several offsets may point at one record, so the sheet
        // as a whole has a budget too.
        const std::size_t rle_bytes = data.size() - info.offset - kRecordHeaderSize;
        if (info.height > rle_bytes + 1) {
            return make_error(ErrorCode::InvalidFormat,
                              "sprite " + std::to_string(i) + " claims more rows than its data holds");
        }
        pixels += std::uint64_t{info.width} * info.height;
        if (pixels > kMaxSheetPixels) {
            return make_error(ErrorCode::InvalidFormat, "the sprites claim more pixels than a sheet may have");
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
        return make_error(sheet.error().code, core::path_to_utf8(path) + ": " + sheet.error().message);
    }
    return sheet;
}

core::Result<IndexedImage> SpriteSheet::decode(std::size_t index, std::uint32_t extra_width) const {
    if (index >= m_infos.size()) {
        return make_error(ErrorCode::OutOfRange, "sprite index out of range");
    }
    const SpriteInfo& info = m_infos[index];
    const auto data = m_file.payload(m_chunk);

    IndexedImage image;
    image.width = info.width + extra_width;
    image.height = info.height;
    const std::size_t pixel_count = std::size_t{image.width} * info.height;
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
            if (x < image.width && y < info.height) {
                const std::size_t i = std::size_t{y} * image.width + x;
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
