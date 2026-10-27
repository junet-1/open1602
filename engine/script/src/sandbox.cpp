// SPDX-License-Identifier: GPL-3.0-or-later
#include "o1602/script/sandbox.hpp"

#include <cstdlib>
#include <initializer_list>
#include <new>
#include <utility>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace o1602::script {

// Lua reports errors with longjmp, which skips C++ destructors. So the functions below that Lua calls
// hold no C++ objects that own anything while they call into Lua, and the callbacks of the game run
// inside a try block that ends before an error is raised.

struct Sandbox::State {
    Limits limits;
    lua_State* lua = nullptr;
    std::size_t used = 0;        ///< bytes allocated for the state
    std::uint64_t executed = 0;  ///< instructions of the current run() or call()
    RandomSource random;
    PrintSink print;

    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    ~State() {
        if (lua != nullptr) {
            lua_close(lua);
        }
    }
};

namespace {

using State = Sandbox::State;

constexpr int kMaxArguments = 200;

State& state_of(lua_State* lua) {
    void* data = nullptr;
    lua_getallocf(lua, &data);
    return *static_cast<State*>(data);
}

// The allocator of the state, which keeps it below the memory limit.
void* allocate(void* data, void* block, std::size_t old_size, std::size_t new_size) {
    State& state = *static_cast<State*>(data);
    if (block == nullptr) {
        old_size = 0;  // Lua passes the kind of object here when it asks for a new block
    }
    if (new_size == 0) {
        std::free(block);  // NOLINT(cppcoreguidelines-no-malloc): Lua's allocator interface
        state.used -= old_size;
        return nullptr;
    }
    if (new_size > old_size && new_size - old_size > state.limits.memory_bytes - state.used) {
        return nullptr;  // Lua turns this into a "not enough memory" error
    }
    void* moved = std::realloc(block, new_size);  // NOLINT(cppcoreguidelines-no-malloc)
    if (moved == nullptr) {
        return nullptr;
    }
    state.used = state.used - old_size + new_size;
    return moved;
}

// Count hook: adds up the instructions and stops a script that runs past its limit. After the limit
// every further instruction fails too, so a pcall in the script cannot swallow the error and go on.
void count_instructions(lua_State* lua, lua_Debug* /*debug*/) {
    State& state = state_of(lua);
    state.executed += static_cast<std::uint64_t>(lua_gethookcount(lua));
    if (state.executed > state.limits.instructions) {
        lua_sethook(lua, count_instructions, LUA_MASKCOUNT, 1);
        luaL_error(lua, "the script ran too long (more than %I instructions)",
                   static_cast<lua_Integer>(state.limits.instructions));
    }
}

/// Pushes the text of a value as tostring() would, but without addresses: a table, function,
/// coroutine or userdata without __tostring becomes its type name. An address differs from
/// computer to computer and from run to run.
void push_text(lua_State* lua, int index) {
    index = lua_absindex(lua, index);
    switch (lua_type(lua, index)) {
        case LUA_TNIL:
        case LUA_TBOOLEAN:
        case LUA_TNUMBER:
        case LUA_TSTRING:
            luaL_tolstring(lua, index, nullptr);
            return;
        default:
            break;
    }
    if (luaL_getmetafield(lua, index, "__tostring") != LUA_TNIL) {
        lua_pop(lua, 1);
        luaL_tolstring(lua, index, nullptr);
        return;
    }
    lua_pushstring(lua, luaL_typename(lua, index));
}

int safe_tostring(lua_State* lua) {
    luaL_checkany(lua, 1);
    push_text(lua, 1);
    return 1;
}

int safe_print(lua_State* lua) {
    const int count = lua_gettop(lua);
    luaL_Buffer line;
    luaL_buffinit(lua, &line);
    for (int i = 1; i <= count; ++i) {
        if (i > 1) {
            luaL_addchar(&line, '\t');
        }
        push_text(lua, i);
        luaL_addvalue(&line);
    }
    luaL_pushresult(&line);
    std::size_t length = 0;
    const char* text = lua_tolstring(lua, -1, &length);
    const State& state = state_of(lua);
    if (state.print) {
        try {
            state.print(std::string_view(text, length));
        } catch (...) {  // NOLINT(bugprone-empty-catch): a failing log must not stop the script
        }
    }
    return 0;
}

std::uint32_t draw(lua_State* lua) {
    const State& state = state_of(lua);
    std::uint32_t number = 0;
    bool drawn = false;
    if (state.random) {
        try {
            number = state.random();
            drawn = true;
        } catch (...) {  // NOLINT(bugprone-empty-catch): reported below, outside the catch block
        }
    }
    if (!drawn) {
        luaL_error(lua, "math.random has no random numbers here");
    }
    return number;
}

/// Uniform in [0, span], without modulo bias.
lua_Unsigned uniform(lua_State* lua, lua_Unsigned span) {
    if (span <= 0xFFFF'FFFFU) {
        const std::uint64_t count = span + 1;
        const std::uint64_t limit = (std::uint64_t{1} << 32U) % count;  // draws below it favour small results
        for (;;) {
            const std::uint64_t r = draw(lua);
            if (r >= limit) {
                return r % count;
            }
        }
    }
    for (;;) {
        const lua_Unsigned r = (static_cast<lua_Unsigned>(draw(lua)) << 32U) | draw(lua);
        if (span == ~lua_Unsigned{0}) {
            return r;
        }
        const lua_Unsigned count = span + 1;
        if (r >= (lua_Unsigned{0} - count) % count) {
            return r % count;
        }
    }
}

// math.random as in Lua 5.4, but with the game's numbers: random() a float in [0, 1), random(m)
// an integer in [1, m], random(m, n) one in [m, n], random(0) an integer with all bits random.
int game_random(lua_State* lua) {
    lua_Integer low = 1;
    lua_Integer up = 0;
    switch (lua_gettop(lua)) {
        case 0:
            lua_pushnumber(lua, static_cast<lua_Number>(draw(lua)) * 0x1p-32);
            return 1;
        case 1:
            up = luaL_checkinteger(lua, 1);
            if (up == 0) {
                lua_pushinteger(lua, static_cast<lua_Integer>(uniform(lua, ~lua_Unsigned{0})));
                return 1;
            }
            break;
        case 2:
            low = luaL_checkinteger(lua, 1);
            up = luaL_checkinteger(lua, 2);
            break;
        default:
            return luaL_error(lua, "wrong number of arguments");
    }
    luaL_argcheck(lua, low <= up, 1, "interval is empty");
    const lua_Unsigned span = static_cast<lua_Unsigned>(up) - static_cast<lua_Unsigned>(low);
    lua_pushinteger(lua, static_cast<lua_Integer>(static_cast<lua_Unsigned>(low) + uniform(lua, span)));
    return 1;
}

/// Whether a format string holds a %p conversion, which prints an address.
bool formats_pointer(const char* format, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
        if (format[i] != '%') {
            continue;
        }
        ++i;
        while (i < length && (format[i] == '-' || format[i] == '+' || format[i] == ' ' || format[i] == '#' ||
                              format[i] == '.' || (format[i] >= '0' && format[i] <= '9'))) {
            ++i;
        }
        if (i < length && format[i] == 'p') {
            return true;
        }
    }
    return false;
}

// string.format without addresses: no %p, and tables and functions print as their type name.
int safe_format(lua_State* lua) {
    std::size_t length = 0;
    const char* format = luaL_checklstring(lua, 1, &length);
    if (formats_pointer(format, length)) {
        return luaL_error(lua, "string.format: '%%p' is not available in scripts");
    }
    const int count = lua_gettop(lua);
    for (int i = 2; i <= count; ++i) {
        switch (lua_type(lua, i)) {
            case LUA_TTABLE:
            case LUA_TFUNCTION:
            case LUA_TUSERDATA:
            case LUA_TLIGHTUSERDATA:
            case LUA_TTHREAD:
                push_text(lua, i);
                lua_replace(lua, i);
                break;
            default:
                break;
        }
    }
    lua_pushvalue(lua, lua_upvalueindex(1));
    lua_insert(lua, 1);
    lua_call(lua, count, 1);
    return 1;
}

// setmetatable without finalizers and weak tables: when the garbage collector runs differs between
// systems, so what those do would differ too.
int safe_setmetatable(lua_State* lua) {
    if (lua_type(lua, 2) == LUA_TTABLE) {
        for (const char* field : {"__gc", "__mode"}) {
            lua_pushstring(lua, field);
            if (lua_rawget(lua, 2) != LUA_TNIL) {
                return luaL_error(lua, "setmetatable: '%s' is not available in scripts", field);
            }
            lua_pop(lua, 1);
        }
    }
    const int count = lua_gettop(lua);
    lua_pushvalue(lua, lua_upvalueindex(1));
    lua_insert(lua, 1);
    lua_call(lua, count, 1);
    return 1;
}

void remove_fields(lua_State* lua, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        lua_pushnil(lua);
        lua_setfield(lua, -2, name);
    }
}

/// Replaces field `name` of the table on top with `function`, which gets the old value as upvalue.
void wrap_field(lua_State* lua, const char* name, lua_CFunction function) {
    lua_getfield(lua, -1, name);
    lua_pushcclosure(lua, function, 1);
    lua_setfield(lua, -2, name);
}

// Opens the libraries a script may use: base, coroutine, table, string, math and utf8. No io, os,
// package (require) and debug; base without the functions that read files, load binary chunks or
// look at the garbage collector.
int open_libraries(lua_State* lua) {
    static const luaL_Reg kLibraries[] = {
        {LUA_GNAME, luaopen_base},        {LUA_COLIBNAME, luaopen_coroutine}, {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string}, {LUA_MATHLIBNAME, luaopen_math},    {LUA_UTF8LIBNAME, luaopen_utf8},
    };
    for (const luaL_Reg& library : kLibraries) {
        luaL_requiref(lua, library.name, library.func, 1);
        lua_pop(lua, 1);
    }

    lua_pushglobaltable(lua);
    remove_fields(lua, {"dofile", "loadfile", "load", "collectgarbage", "warn"});
    lua_pushcfunction(lua, safe_print);
    lua_setfield(lua, -2, "print");
    lua_pushcfunction(lua, safe_tostring);
    lua_setfield(lua, -2, "tostring");
    wrap_field(lua, "setmetatable", safe_setmetatable);

    lua_getfield(lua, -1, LUA_STRLIBNAME);
    remove_fields(lua, {"dump"});
    wrap_field(lua, "format", safe_format);
    lua_pop(lua, 1);

    lua_getfield(lua, -1, LUA_MATHLIBNAME);
    remove_fields(lua, {"randomseed"});
    lua_pushcfunction(lua, game_random);
    lua_setfield(lua, -2, "random");
    lua_pop(lua, 2);
    return 0;
}

struct RunRequest {
    std::string_view name;
    std::string_view source;
};

int run_chunk(lua_State* lua) {
    const auto& request = *static_cast<const RunRequest*>(lua_touserdata(lua, 1));
    lua_pushliteral(lua, "=");
    lua_pushlstring(lua, request.name.data(), request.name.size());
    lua_concat(lua, 2);
    if (luaL_loadbufferx(lua, request.source.data(), request.source.size(), lua_tostring(lua, -1), "t") != LUA_OK) {
        return lua_error(lua);
    }
    lua_call(lua, 0, 0);
    return 0;
}

struct CallRequest {
    std::string_view name;
    std::span<const Value> args;
    bool missing = false;
};

/// Pushes the global `name` and reports whether it is a function, without metamethods of _G.
bool push_global_function(lua_State* lua, std::string_view name) {
    lua_pushglobaltable(lua);
    lua_pushlstring(lua, name.data(), name.size());
    const bool function = lua_rawget(lua, -2) == LUA_TFUNCTION;
    lua_remove(lua, -2);
    return function;
}

int find_function(lua_State* lua) {
    const auto& name = *static_cast<const std::string_view*>(lua_touserdata(lua, 1));
    lua_pushboolean(lua, static_cast<int>(push_global_function(lua, name)));
    return 1;
}

void push_value(lua_State* lua, const Value& value) {
    if (const auto* flag = std::get_if<bool>(&value)) {
        lua_pushboolean(lua, static_cast<int>(*flag));
    } else if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        lua_pushinteger(lua, *integer);
    } else if (const auto* number = std::get_if<double>(&value)) {
        lua_pushnumber(lua, *number);
    } else if (const auto* text = std::get_if<std::string>(&value)) {
        lua_pushlstring(lua, text->data(), text->size());
    } else {
        lua_pushnil(lua);
    }
}

// Calls the function and leaves its first result on the stack.
int call_function(lua_State* lua) {
    auto& request = *static_cast<CallRequest*>(lua_touserdata(lua, 1));
    if (!push_global_function(lua, request.name)) {
        request.missing = true;
        lua_pushlstring(lua, request.name.data(), request.name.size());
        return luaL_error(lua, "the script has no function '%s'", lua_tostring(lua, -1));
    }
    if (request.args.size() > static_cast<std::size_t>(kMaxArguments)) {
        return luaL_error(lua, "more than %d arguments", kMaxArguments);
    }
    const auto count = static_cast<int>(request.args.size());
    luaL_checkstack(lua, count, "too many arguments");
    for (const Value& value : request.args) {
        push_value(lua, value);
    }
    lua_call(lua, count, 1);
    return 1;
}

/// The message of a failed protected call, which is on top of the stack.
std::string error_message(lua_State* lua, int status, const Limits& limits) {
    if (status == LUA_ERRMEM) {
        return "the script needs more than " + std::to_string(limits.memory_bytes) + " bytes of memory";
    }
    if (lua_type(lua, -1) == LUA_TSTRING || lua_type(lua, -1) == LUA_TNUMBER) {
        std::size_t length = 0;
        const char* text = lua_tolstring(lua, -1, &length);
        return {text, length};
    }
    return std::string("the script raised an error that is no text but a ") + luaL_typename(lua, -1);
}

/// Runs `function` with `request` as its argument under the instruction limit. On success its
/// `results` results are on the stack, on failure nothing is.
core::Status protected_call(State& state, lua_CFunction function, void* request, int results) {
    lua_State* lua = state.lua;
    state.executed = 0;
    lua_sethook(lua, count_instructions, LUA_MASKCOUNT, Sandbox::kInstructionStep);
    lua_pushcfunction(lua, function);
    lua_pushlightuserdata(lua, request);
    const int status = lua_pcall(lua, 1, results, 0);
    if (status == LUA_OK) {
        return core::Ok{};
    }
    std::string message = error_message(lua, status, state.limits);
    lua_pop(lua, 1);
    return core::make_error(core::ErrorCode::ScriptError, std::move(message));
}

}  // namespace

Sandbox::Sandbox(Limits limits) : m_state(std::make_unique<State>()) {
    m_state->limits = limits;
    m_state->lua = lua_newstate(allocate, m_state.get());
    if (m_state->lua == nullptr) {
        throw std::bad_alloc();
    }
    lua_pushcfunction(m_state->lua, open_libraries);
    if (lua_pcall(m_state->lua, 0, 0, 0) != LUA_OK) {
        throw std::bad_alloc();  // opening the libraries can only fail for lack of memory
    }
}

Sandbox::~Sandbox() = default;
Sandbox::Sandbox(Sandbox&&) noexcept = default;
Sandbox& Sandbox::operator=(Sandbox&&) noexcept = default;

void Sandbox::set_random(RandomSource source) {
    m_state->random = std::move(source);
}

void Sandbox::set_print(PrintSink sink) {
    m_state->print = std::move(sink);
}

core::Status Sandbox::run(std::string_view name, std::string_view source) {
    RunRequest request{name, source};
    return protected_call(*m_state, run_chunk, &request, 0);
}

bool Sandbox::has_function(std::string_view name) const {
    lua_State* lua = m_state->lua;
    lua_pushcfunction(lua, find_function);
    lua_pushlightuserdata(lua, &name);
    if (lua_pcall(lua, 1, 1, 0) != LUA_OK) {
        lua_pop(lua, 1);
        return false;
    }
    const bool found = lua_toboolean(lua, -1) != 0;
    lua_pop(lua, 1);
    return found;
}

core::Result<Value> Sandbox::call(std::string_view name, std::span<const Value> args) {
    CallRequest request{name, args};
    core::Status status = protected_call(*m_state, call_function, &request, 1);
    if (!status) {
        if (request.missing) {
            return core::make_error(core::ErrorCode::NotFound, status.error().message);
        }
        return status.error();
    }
    lua_State* lua = m_state->lua;
    Value result;
    switch (lua_type(lua, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            result = lua_toboolean(lua, -1) != 0;
            break;
        case LUA_TNUMBER:
            if (lua_isinteger(lua, -1) != 0) {
                result = static_cast<std::int64_t>(lua_tointeger(lua, -1));
            } else {
                result = static_cast<double>(lua_tonumber(lua, -1));
            }
            break;
        case LUA_TSTRING: {
            std::size_t length = 0;
            const char* text = lua_tolstring(lua, -1, &length);
            result = std::string(text, length);
            break;
        }
        default: {
            std::string message = "function '" + std::string(name) + "' returned a " + luaL_typename(lua, -1) +
                                  "; only nil, booleans, numbers and strings come back to the game";
            lua_pop(lua, 1);
            return core::make_error(core::ErrorCode::ScriptError, std::move(message));
        }
    }
    lua_pop(lua, 1);
    return result;
}

std::size_t Sandbox::memory_used() const noexcept {
    return m_state->used;
}

std::uint64_t Sandbox::instructions_used() const noexcept {
    return m_state->executed;
}

}  // namespace o1602::script
