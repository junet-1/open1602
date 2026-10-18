// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/net/session.hpp"

#include <algorithm>
#include <utility>

namespace o1602::net {

namespace {

constexpr std::uint32_t kTickMs = sim::Simulation::kTickMs;
constexpr std::uint32_t kMaxOwedMs = kTickMs * sim::Simulation::kMaxTicksPerAdvance;

}  // namespace

const char* describe(EndReason reason) noexcept {
    switch (reason) {
        case EndReason::None:
            return "";
        case EndReason::ConnectFailed:
            return "Der Gastgeber ist nicht erreichbar.";
        case EndReason::HostLeft:
            return "Die Verbindung zum Gastgeber ist beendet.";
        case EndReason::Refused:
            return "Der Gastgeber hat abgelehnt.";
        case EndReason::ProtocolError:
            return "Unverständliche Nachricht im Netz.";
        case EndReason::LoadFailed:
            return "Das Spiel des Gastgebers ließ sich nicht laden.";
        case EndReason::Desync:
            return "Dieser Rechner weicht immer wieder vom Gastgeber ab.";
    }
    return "";
}

const char* describe(RefuseReason reason) noexcept {
    switch (reason) {
        case RefuseReason::Version:
            return "Der Gastgeber hat eine andere Version von Open1602.";
        case RefuseReason::Content:
            return "Der Gastgeber hat andere Spieldaten.";
        case RefuseReason::Full:
            return "Kein Platz mehr frei.";
        case RefuseReason::Closed:
            return "Der Gastgeber nimmt niemanden mehr ins laufende Spiel.";
        case RefuseReason::Desync:
            return "Dieser Rechner weicht immer wieder vom Gastgeber ab.";
        case RefuseReason::Invalid:
            return "Dieser Rechner hat Unverständliches geschickt.";
    }
    return "";
}

Session::Session(std::unique_ptr<Transport> transport, SessionConfig config)
    : m_transport(std::move(transport)), m_config(config) {
    m_config.hash_interval = std::max<std::uint32_t>(m_config.hash_interval, 1);
    m_config.min_lead = std::max<std::uint16_t>(m_config.min_lead, 1);
    m_config.max_lead = std::max(m_config.max_lead, m_config.min_lead);
}

double Session::tick_fraction() const noexcept {
    return std::min(0.999, static_cast<double>(m_pending_ms) / kTickMs);
}

std::vector<SessionEvent> Session::take_events() {
    std::vector<SessionEvent> events;
    events.swap(m_events);
    return events;
}

std::uint32_t Session::frames_due(std::uint32_t real_ms, const sim::Simulation& simulation) {
    // As Simulation::advance(): a stall counts as one tick, at most one more step is owed. The
    // frames of a pause run at normal speed: they carry the commands given meanwhile.
    const std::uint32_t speed = simulation.paused() ? 1 : std::max<std::uint32_t>(simulation.speed(), 1);
    const std::uint64_t game_ms = real_ms > sim::Simulation::kMaxCatchUpMs ? kTickMs : std::uint64_t{real_ms} * speed;
    m_pending_ms = static_cast<std::uint32_t>(std::min<std::uint64_t>(m_pending_ms + game_ms, 2 * kMaxOwedMs));
    return std::min(m_pending_ms / kTickMs, sim::Simulation::kMaxTicksPerAdvance);
}

void Session::ran(std::uint32_t frames) {
    const std::uint64_t spent = std::uint64_t{frames} * kTickMs;
    m_pending_ms = spent >= m_pending_ms ? 0 : static_cast<std::uint32_t>(m_pending_ms - spent);
    m_pending_ms = std::min(m_pending_ms, kMaxOwedMs);
}

std::optional<FrameHash> Session::run_frame(sim::Simulation& simulation, std::span<const sim::Command> commands) {
    // Scheduled for the tick that runs now: tick() carries them out first, by player and then in
    // this order, also in the frames of a pause, when the game time does not move on.
    const std::uint64_t now = simulation.next_tick();
    for (const sim::Command& c : commands) {
        simulation.schedule(now, c);
    }
    simulation.tick();
    ++m_frames;
    if (m_frames % m_config.hash_interval != 0) {
        return std::nullopt;
    }
    const auto hash = simulation.state_hash();
    // A game that cannot be written has no fingerprint; such a frame is not compared.
    return hash ? std::optional<FrameHash>(FrameHash{m_frames, *hash}) : std::nullopt;
}

void Session::end(EndReason reason) {
    if (m_ended) {
        return;
    }
    m_ended = true;
    m_end_reason = reason;
    SessionEvent e;
    e.kind = SessionEvent::Kind::Ended;
    e.reason = reason;
    event(std::move(e));
}

void Session::send(LinkId link, const Message& message) {
    const std::vector<std::uint8_t> bytes = encode(message);
    m_transport->send(link, bytes);
}

}  // namespace o1602::net
