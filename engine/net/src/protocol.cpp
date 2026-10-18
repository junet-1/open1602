// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/net/protocol.hpp"

#include <utility>

#include "o1602/core/byte_reader.hpp"
#include "o1602/core/byte_writer.hpp"
#include "o1602/sim/command_io.hpp"

namespace o1602::net {

namespace {

// A message starts with its type, the index in the Message variant (appended, never reordered).

/// Reader that remembers the first error, so that a decoder can read on and check once.
class In {
public:
    explicit In(std::span<const std::uint8_t> bytes) noexcept : m_reader(bytes) {}

    [[nodiscard]] bool ok() const noexcept { return m_ok; }
    [[nodiscard]] bool at_end() const noexcept { return m_reader.at_end(); }
    void fail() noexcept { m_ok = false; }
    core::ByteReader& reader() noexcept { return m_reader; }

    std::uint8_t u8() { return take(m_reader.u8()); }
    std::uint16_t u16() { return take(m_reader.u16()); }
    std::uint32_t u32() { return take(m_reader.u32()); }
    std::uint64_t u64() { return take(m_reader.u64()); }
    bool boolean() {
        const std::uint8_t b = u8();
        if (b > 1) {
            fail();
        }
        return b != 0;
    }
    /// A count of at most `limit` things.
    std::uint32_t count(std::uint32_t limit) {
        const std::uint32_t n = u32();
        if (n > limit) {
            fail();
            return 0;
        }
        return n;
    }
    std::string text(std::size_t limit) {
        const std::uint32_t n = count(static_cast<std::uint32_t>(limit));
        const auto bytes = m_ok ? m_reader.bytes(n) : std::nullopt;
        if (!bytes) {
            fail();
            return {};
        }
        return {bytes->begin(), bytes->end()};
    }
    std::vector<sim::Command> commands() {
        const std::uint32_t n = count(kMaxCommandsPerMessage);
        std::vector<sim::Command> out;
        out.reserve(n);
        for (std::uint32_t i = 0; i < n && m_ok; ++i) {
            auto command = sim::read_command(m_reader);
            if (!command) {
                fail();
                break;
            }
            out.push_back(std::move(*command));
        }
        return out;
    }

private:
    template <class T>
    T take(std::optional<T> v) {
        if (!v) {
            m_ok = false;
            return T{};
        }
        return *v;
    }

    core::ByteReader m_reader;
    bool m_ok = true;
};

void put_commands(core::ByteWriter& out, const std::vector<sim::Command>& commands) {
    out.u32(static_cast<std::uint32_t>(commands.size()));
    for (const sim::Command& c : commands) {
        sim::write_command(out, c);
    }
}

void put(core::ByteWriter& out, const Hello& m) {
    out.u16(m.protocol);
    out.u64(m.content);
    out.string(m.name);
    out.u8(m.seat);
    out.u64(m.token);
}

void put(core::ByteWriter& out, const CommandBatch& m) {
    put_commands(out, m.commands);
}

void put(core::ByteWriter& out, const Progress& m) {
    out.u32(m.epoch);
    out.u64(m.frames);
    out.boolean(m.hash.has_value());
    if (m.hash) {
        out.u64(m.hash->frame);
        out.u64(m.hash->hash);
    }
}

void put(core::ByteWriter& out, const Welcome& m) {
    out.u16(m.protocol);
    out.u8(m.seat);
    out.u64(m.token);
}

void put(core::ByteWriter& out, const Refused& m) {
    out.u8(static_cast<std::uint8_t>(m.reason));
}

void put(core::ByteWriter& out, const Snapshot& m) {
    out.u32(m.epoch);
    out.u64(m.frame);
    out.u32(m.speed);
    out.boolean(m.paused);
    out.u32(static_cast<std::uint32_t>(m.save.size()));
    out.bytes(m.save);
}

void put(core::ByteWriter& out, const Turn& m) {
    out.u64(m.first);
    out.u64(m.closed);
    out.u16(m.lead);
    out.u32(static_cast<std::uint32_t>(m.frames.size()));
    for (const FrameCommands& f : m.frames) {
        out.u64(f.frame);
        put_commands(out, f.commands);
    }
}

void put(core::ByteWriter& out, const Roster& m) {
    out.u32(static_cast<std::uint32_t>(m.members.size()));
    for (const Member& member : m.members) {
        out.u8(member.seat);
        out.string(member.name);
        out.u8(static_cast<std::uint8_t>(member.state));
        out.boolean(member.host);
    }
}

void put(core::ByteWriter& out, const Desync& m) {
    out.u64(m.frame);
    out.u32(static_cast<std::uint32_t>(m.seats.size()));
    for (const Seat s : m.seats) {
        out.u8(s);
    }
}

Hello get_hello(In& in) {
    Hello m;
    m.protocol = in.u16();
    m.content = in.u64();
    m.name = in.text(kMaxNameLength);
    m.seat = in.u8();
    m.token = in.u64();
    return m;
}

Progress get_progress(In& in) {
    Progress m;
    m.epoch = in.u32();
    m.frames = in.u64();
    if (in.boolean()) {
        FrameHash h;
        h.frame = in.u64();
        h.hash = in.u64();
        m.hash = h;
    }
    return m;
}

Welcome get_welcome(In& in) {
    Welcome m;
    m.protocol = in.u16();
    m.seat = in.u8();
    m.token = in.u64();
    return m;
}

Refused get_refused(In& in) {
    Refused m;
    const std::uint8_t reason = in.u8();
    if (reason > static_cast<std::uint8_t>(RefuseReason::Invalid)) {
        in.fail();
    }
    m.reason = static_cast<RefuseReason>(reason);
    return m;
}

Snapshot get_snapshot(In& in) {
    Snapshot m;
    m.epoch = in.u32();
    m.frame = in.u64();
    m.speed = in.u32();
    m.paused = in.boolean();
    const std::uint32_t n = in.count(kMaxSnapshotBytes);
    if (const auto bytes = in.ok() ? in.reader().bytes(n) : std::nullopt) {
        m.save.assign(bytes->begin(), bytes->end());
    } else {
        in.fail();
    }
    return m;
}

Turn get_turn(In& in) {
    Turn m;
    m.first = in.u64();
    m.closed = in.u64();
    m.lead = in.u16();
    if (m.closed < m.first) {
        in.fail();
    }
    const std::uint32_t n = in.count(kMaxFramesPerTurn);
    m.frames.reserve(n);
    for (std::uint32_t i = 0; i < n && in.ok(); ++i) {
        FrameCommands f;
        f.frame = in.u64();
        // Ascending and within the range, so that a computer can walk them once.
        const std::uint64_t floor = m.frames.empty() ? m.first : m.frames.back().frame + 1;
        if (f.frame < floor || f.frame >= m.closed) {
            in.fail();
        }
        f.commands = in.commands();
        m.frames.push_back(std::move(f));
    }
    return m;
}

Roster get_roster(In& in) {
    Roster m;
    const std::uint32_t n = in.count(kMaxRosterMembers);
    m.members.reserve(n);
    for (std::uint32_t i = 0; i < n && in.ok(); ++i) {
        Member member;
        member.seat = in.u8();
        member.name = in.text(kMaxNameLength);
        const std::uint8_t state = in.u8();
        if (state > static_cast<std::uint8_t>(Member::State::Lagging)) {
            in.fail();
        }
        member.state = static_cast<Member::State>(state);
        member.host = in.boolean();
        m.members.push_back(std::move(member));
    }
    return m;
}

Desync get_desync(In& in) {
    Desync m;
    m.frame = in.u64();
    const std::uint32_t n = in.count(kMaxRosterMembers);
    m.seats.reserve(n);
    for (std::uint32_t i = 0; i < n && in.ok(); ++i) {
        m.seats.push_back(in.u8());
    }
    return m;
}

std::optional<Message> get(In& in, std::uint8_t type) {
    switch (type) {
        case 0:
            return get_hello(in);
        case 1:
            return CommandBatch{in.commands()};
        case 2:
            return get_progress(in);
        case 3:
            return get_welcome(in);
        case 4:
            return get_refused(in);
        case 5:
            return get_snapshot(in);
        case 6:
            return get_turn(in);
        case 7:
            return get_roster(in);
        case 8:
            return get_desync(in);
        default:
            return std::nullopt;
    }
}

static_assert(std::variant_size_v<Message> == 9, "a new message needs its case in get()");

}  // namespace

std::vector<std::uint8_t> encode(const Message& message) {
    core::ByteWriter out;
    out.u8(static_cast<std::uint8_t>(message.index()));
    std::visit([&out](const auto& m) { put(out, m); }, message);
    return out.take();
}

std::optional<Message> decode(std::span<const std::uint8_t> bytes) {
    In in(bytes);
    const std::uint8_t type = in.u8();
    if (!in.ok()) {
        return std::nullopt;
    }
    auto message = get(in, type);
    if (!message || !in.ok() || !in.at_end()) {
        return std::nullopt;
    }
    return message;
}

sim::PlayerId command_player(const sim::Command& command) noexcept {
    return std::visit([](const auto& c) { return c.player; }, command);
}

std::uint64_t fingerprint(std::span<const std::uint8_t> data, std::uint64_t seed) noexcept {
    std::uint64_t h = seed;
    for (const std::uint8_t b : data) {
        h ^= b;
        h *= 0x100000001b3ULL;
    }
    return h;
}

}  // namespace o1602::net
