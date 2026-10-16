// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "o1602/assets/chunk_file.hpp"
#include "o1602/core/result.hpp"

namespace o1602::assets {

/// Fields of the INSEL3 / INSEL5 chunk that are verified so far. See docs/formats/scp.md.
struct IslandHeader {
    std::string chunk_name;  ///< "INSEL3" (40 bytes) or "INSEL5" (116 bytes)
    std::uint8_t number = 0;
    std::uint8_t width = 0;   ///< in tiles
    std::uint8_t height = 0;  ///< in tiles
    std::uint8_t flags = 0;   ///< unknown, 0 in all samples so far
    std::uint16_t pos_x = 0;  ///< position on the 500x350 world map
    std::uint16_t pos_y = 0;
    std::vector<std::uint8_t> raw;  ///< complete chunk payload for not yet decoded fields
};

/// One entry of an INSELHAUS chunk (8 bytes).
struct IslandTile {
    /// Building / terrain id. Matches the "Id" values in haeuser.cod minus 20000
    /// (e.g. 1201 -> IDMEER+0 = 21201). Verified for all ids of NORD/big.scp, see docs/formats/scp.md.
    std::uint16_t id = 0;
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint32_t bits = 0;  ///< Packed attributes (rotation, animation, owner, ...), not decoded yet.

    static constexpr std::uint32_t kCodIdOffset = 20000;
    [[nodiscard]] std::uint32_t cod_id() const noexcept { return std::uint32_t{id} + kCodIdOffset; }
};

struct Island {
    IslandHeader header;
    /// Each INSELHAUS chunk following the header is one layer.
    std::vector<std::vector<IslandTile>> layers;
};

/// Extracts all islands from a chunk file (.SCP island template, .SZS/.SZM scenario, .GAM savegame).
core::Result<std::vector<Island>> load_islands(const ChunkFile& file);

}  // namespace o1602::assets
