// SPDX-License-Identifier: GPL-3.0-or-later
//
// The messages of a multiplayer game (ADR 0011). Every computer talks only to the host; the host
// decides which commands belong to which frame and sends every frame, complete, to everyone. A
// frame is one call of Simulation::tick(), counted from the start of the session; the game time
// stands still in the frames of a pause, the frames go on.
//
// Little-endian with fixed widths, like the commands (sim/command_io.hpp). A message starts with
// its type. New types and new fields at the end of a message come with a new kProtocolVersion;
// the host refuses a computer of another version.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "o1602/sim/simulation.hpp"

namespace o1602::net {

/// Raised whenever the messages or the simulation change in a way older builds would not follow.
inline constexpr std::uint16_t kProtocolVersion = 1;

/// Upper limits a decoded message must keep; more comes from damaged or hostile data.
inline constexpr std::size_t kMaxNameLength = sim::PlayerRename::kMaxNameLength;
inline constexpr std::uint32_t kMaxCommandsPerMessage = 1024;
inline constexpr std::uint32_t kMaxFramesPerTurn = 1U << 16;
inline constexpr std::uint32_t kMaxRosterMembers = 256;
inline constexpr std::uint32_t kMaxSnapshotBytes = 32U << 20;

/// Which player of the world a computer plays: kNoPlayer watches only.
using Seat = sim::PlayerId;

// --- From a computer to the host ---

/// The first message after connecting.
struct Hello {
    std::uint16_t protocol = kProtocolVersion;
    /// Fingerprint of the game data the simulation depends on (content_fingerprint()): computers
    /// with other data would compute another game.
    std::uint64_t content = 0;
    std::string name;            ///< the player's name, 1 to kMaxNameLength bytes (Windows-1252)
    Seat seat = sim::kNoPlayer;  ///< the player slot wanted, kNoPlayer for any
    std::uint64_t token = 0;     ///< from an earlier Welcome: takes the same slot again
};

/// Commands of the computer's player, in the order given; the host puts them into the next frame
/// it closes.
struct CommandBatch {
    std::vector<sim::Command> commands;
};

/// The fingerprint of the game after a frame (Simulation::state_hash()).
struct FrameHash {
    std::uint64_t frame = 0;  ///< frames run when it was taken
    std::uint64_t hash = 0;
    friend bool operator==(const FrameHash&, const FrameHash&) = default;
};

/// How far a computer is: the host waits for the slowest, compares fingerprints and knows which
/// frames it may forget.
struct Progress {
    std::uint32_t epoch = 0;   ///< of the game it runs (Snapshot::epoch): older reports are stale
    std::uint64_t frames = 0;  ///< frames run so far
    std::optional<FrameHash> hash;
};

// --- From the host to a computer ---

/// The computer is in: the slot it plays and the token to come back to it.
struct Welcome {
    std::uint16_t protocol = kProtocolVersion;
    Seat seat = sim::kNoPlayer;
    std::uint64_t token = 0;
};

/// Why the host does not take a computer (it disconnects right after).
enum class RefuseReason : std::uint8_t {
    Version,  ///< another kProtocolVersion
    Content,  ///< other game data
    Full,     ///< no free slot and no spectators
    Closed,   ///< the host takes nobody in a running game
    Desync,   ///< the computer kept diverging from the host's game
    Invalid,  ///< the computer sent what makes no sense
};

struct Refused {
    RefuseReason reason = RefuseReason::Version;
};

/// The whole game as the host has it before frame `frame`: at the start, for a computer that
/// comes in later and after a desync. The frames from `frame` on follow in Turns.
struct Snapshot {
    std::uint32_t epoch = 0;  ///< counts the games sent to this computer
    std::uint64_t frame = 0;
    std::uint32_t speed = 1;  ///< game speed and pause: not part of the savegame
    bool paused = false;
    std::vector<std::uint8_t> save;  ///< Open1602 savegame (sim/save_game.hpp)
};

/// The commands of one frame, in the order they are carried out.
struct FrameCommands {
    std::uint64_t frame = 0;
    std::vector<sim::Command> commands;
};

/// Frames `first` to `closed` - 1 are complete: those not listed have no commands. A computer may
/// run every frame below `closed`.
struct Turn {
    std::uint64_t first = 0;
    std::uint64_t closed = 0;
    /// How many frames the host closes ahead of its own game: a computer with more frames in hand
    /// than this runs faster to catch up.
    std::uint16_t lead = 0;
    std::vector<FrameCommands> frames;  ///< ascending, within [first, closed)
};

/// A computer of the session as everyone sees it.
struct Member {
    enum class State : std::uint8_t {
        Playing,
        Loading,  ///< receiving or loading the game
        Lagging,  ///< so far behind that the others no longer wait for it
    };
    Seat seat = sim::kNoPlayer;
    std::string name;
    State state = State::Playing;
    bool host = false;
    friend bool operator==(const Member&, const Member&) = default;
};

struct Roster {
    std::vector<Member> members;
};

/// The fingerprints of these players differed from the host's after frame `frame`; they get the
/// game anew (Snapshot).
struct Desync {
    std::uint64_t frame = 0;
    std::vector<Seat> seats;
};

using Message = std::variant<Hello, CommandBatch, Progress, Welcome, Refused, Snapshot, Turn, Roster, Desync>;

[[nodiscard]] std::vector<std::uint8_t> encode(const Message& message);
/// Nothing if the bytes are cut off, too long, of an unknown type or out of range.
[[nodiscard]] std::optional<Message> decode(std::span<const std::uint8_t> bytes);

/// The player a command acts for (every command names one).
[[nodiscard]] sim::PlayerId command_player(const sim::Command& command) noexcept;

/// FNV-1a 64 over data, continuing from `seed`: for content fingerprints over several files.
[[nodiscard]] std::uint64_t fingerprint(std::span<const std::uint8_t> data,
                                        std::uint64_t seed = 0xcbf29ce484222325ULL) noexcept;

}  // namespace o1602::net
