// SPDX-License-Identifier: GPL-3.0-or-later
//
// A Lua 5.4 state for the scripts of content packages (ADR 0007 section 6, docs/modding/scripts.md).
// Scripts run on every computer of a game and in every replay, so the sandbox keeps them
// deterministic and closed off: no files, no clock, no addresses, random numbers only from the
// game's generator, and limits on memory and on the instructions of each entry.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include "o1602/core/result.hpp"

namespace o1602::script {

struct Limits {
    std::size_t memory_bytes = std::size_t{32} * 1024 * 1024;  ///< everything the state holds
    std::uint64_t instructions = 10'000'000;                   ///< per run() or call()
};

/// What passes between the game and a script function: nil, a boolean, an integer, a number or a
/// string. Tables and functions stay inside the sandbox.
using Value = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

class Sandbox {
public:
    /// Uniformly distributed 32-bit numbers from the game's generator.
    using RandomSource = std::function<std::uint32_t()>;
    /// One line a script printed.
    using PrintSink = std::function<void(std::string_view)>;

    /// Throws std::bad_alloc when not even an empty state fits into the memory limit.
    explicit Sandbox(Limits limits = {});
    ~Sandbox();
    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;
    Sandbox(Sandbox&&) noexcept;
    Sandbox& operator=(Sandbox&&) noexcept;

    /// math.random draws from `source`; without one it raises an error in the script.
    void set_random(RandomSource source);
    /// print() goes to `sink`; without one it is dropped.
    void set_print(PrintSink sink);

    /// Runs a script's text (binary chunks are refused). `name` appears in its error messages.
    core::Status run(std::string_view name, std::string_view source);

    /// Whether the script defined a global function of this name.
    [[nodiscard]] bool has_function(std::string_view name) const;

    /// Calls a global function with `args` and returns its first result. NotFound when there is no
    /// such function, ScriptError when it fails or returns a table or function.
    core::Result<Value> call(std::string_view name, std::span<const Value> args = {});

    /// Bytes the state holds now.
    [[nodiscard]] std::size_t memory_used() const noexcept;
    /// Instructions the last run() or call() executed, counted in steps of kInstructionStep.
    [[nodiscard]] std::uint64_t instructions_used() const noexcept;

    /// How often the instruction limit is checked.
    static constexpr int kInstructionStep = 1000;

    struct State;

private:
    std::unique_ptr<State> m_state;
};

}  // namespace o1602::script
