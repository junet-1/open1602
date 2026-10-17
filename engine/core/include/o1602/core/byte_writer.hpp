// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace o1602::core {

/// Little-endian writer into a growing byte buffer; the counterpart of ByteReader.
class ByteWriter {
public:
    void u8(std::uint8_t v) { m_data.push_back(v); }
    void u16(std::uint16_t v) { put(v, 2); }
    void u32(std::uint32_t v) { put(v, 4); }
    void u64(std::uint64_t v) { put(v, 8); }
    void i32(std::int32_t v) { put(static_cast<std::uint32_t>(v), 4); }
    void i64(std::int64_t v) { put(static_cast<std::uint64_t>(v), 8); }
    void boolean(bool v) { u8(v ? 1 : 0); }
    void bytes(std::span<const std::uint8_t> data) { m_data.insert(m_data.end(), data.begin(), data.end()); }
    /// A string with its length (u32) in front.
    void string(std::string_view s) {
        u32(static_cast<std::uint32_t>(s.size()));
        m_data.insert(m_data.end(), s.begin(), s.end());
    }
    /// Overwrites a u32 written earlier (sizes known only afterwards).
    void patch_u32(std::size_t offset, std::uint32_t v) {
        for (std::size_t k = 0; k < 4; ++k) {
            m_data[offset + k] = static_cast<std::uint8_t>(v >> (8 * k));
        }
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_data.size(); }
    [[nodiscard]] const std::vector<std::uint8_t>& data() const noexcept { return m_data; }
    [[nodiscard]] std::vector<std::uint8_t> take() noexcept { return std::move(m_data); }

private:
    void put(std::uint64_t v, std::size_t n) {
        for (std::size_t k = 0; k < n; ++k) {
            m_data.push_back(static_cast<std::uint8_t>(v >> (8 * k)));
        }
    }
    std::vector<std::uint8_t> m_data;
};

}  // namespace o1602::core
