// SPDX-License-Identifier: GPL-3.0-or-later
//
// Commands as bytes (ADR 0004): what travels between the computers of a multiplayer game and what
// a replay keeps. Little-endian and of fixed widths like the savegame; every field is written, so
// the same command gives the same bytes on every platform.
//
// A command starts with its kind, the index in the Command variant. New kinds are appended at
// the end of the variant and never reordered, so older bytes keep their meaning.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "o1602/core/byte_reader.hpp"
#include "o1602/core/byte_writer.hpp"
#include "o1602/sim/simulation.hpp"

namespace o1602::sim {

/// Upper limits a decoded command must keep; longer lists come from damaged or hostile data.
inline constexpr std::uint32_t kMaxCommandItems = 4096;
inline constexpr std::uint32_t kMaxCommandText = 255;

/// A command for a tick, as the network or a replay carries it.
struct ScheduledCommand {
    std::uint64_t tick = 0;
    Command command;
};

void write_command(core::ByteWriter& out, const Command& command);
/// Reads one command; nothing if the bytes are cut off, of an unknown kind or out of range.
[[nodiscard]] std::optional<Command> read_command(core::ByteReader& in);

void write_scheduled(core::ByteWriter& out, const ScheduledCommand& command);
[[nodiscard]] std::optional<ScheduledCommand> read_scheduled(core::ByteReader& in);

/// Convenience: one command as a block of bytes and back (the whole block must be used).
[[nodiscard]] std::vector<std::uint8_t> encode_command(const Command& command);
[[nodiscard]] std::optional<Command> decode_command(std::span<const std::uint8_t> bytes);

}  // namespace o1602::sim
