// SPDX-License-Identifier: GPL-3.0-or-later
//
// What happened in the game for the interface (messages, floating texts, sounds): the simulation
// collects events tick by tick, the interface takes them once per frame. A game without an
// interface (a server, a test, a tool) never takes them, so every buffer has a bound.
#pragma once

#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace o1602::sim {

template <class T>
class EventBuffer {
public:
    /// Events nobody takes are dropped beyond this, the older half at once.
    static constexpr std::size_t kMaxPending = 8192;

    /// Adds an event; drops the older half first if the buffer is full.
    void push(T event) {
        trim();
        m_events.push_back(std::move(event));
    }
    /// Adds events in their order.
    void append(std::vector<T>&& events) {
        m_events.insert(m_events.end(), std::make_move_iterator(events.begin()), std::make_move_iterator(events.end()));
        trim();
    }
    /// For the functions of the subsystems that append to a vector. Indexes into it stay valid
    /// until the next push(), append() or trim().
    [[nodiscard]] std::vector<T>* sink() noexcept { return &m_events; }
    [[nodiscard]] const std::vector<T>& events() const noexcept { return m_events; }
    [[nodiscard]] std::size_t size() const noexcept { return m_events.size(); }

    /// Drops the older half once more than kMaxPending wait.
    void trim() {
        if (m_events.size() > kMaxPending) {
            m_events.erase(m_events.begin(), m_events.end() - static_cast<std::ptrdiff_t>(kMaxPending / 2));
        }
    }
    /// The events since the last call, oldest first.
    [[nodiscard]] std::vector<T> take() noexcept {
        std::vector<T> events;
        events.swap(m_events);
        return events;
    }

private:
    std::vector<T> m_events;
};

}  // namespace o1602::sim
