// SPDX-License-Identifier: GPL-3.0-or-later
//
// Minimal dependency-free test framework. Can be replaced by Catch2/doctest later
// without touching the tests much (TEST_CASE / CHECK / REQUIRE have the same meaning).
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace o1602::test {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

struct Failure {};
struct Skip {
    std::string reason;
};

inline int& failures() {
    static int count = 0;
    return count;
}

inline void report(const char* file, int line, const char* expr) {
    std::fprintf(stderr, "  %s:%d: CHECK failed: %s\n", file, line, expr);
    ++failures();
}

}  // namespace o1602::test

#define O1602_CONCAT_(a, b) a##b
#define O1602_CONCAT(a, b) O1602_CONCAT_(a, b)

#define TEST_CASE(name)                                                                                             \
    static void O1602_CONCAT(o1602_test_, __LINE__)();                                                              \
    static const ::o1602::test::Registrar O1602_CONCAT(o1602_reg_, __LINE__)(name,                                  \
                                                                             &O1602_CONCAT(o1602_test_, __LINE__)); \
    static void O1602_CONCAT(o1602_test_, __LINE__)()

#define CHECK(expr)                                           \
    do {                                                      \
        if (!(expr))                                          \
            ::o1602::test::report(__FILE__, __LINE__, #expr); \
    } while (false)

#define REQUIRE(expr)                                         \
    do {                                                      \
        if (!(expr)) {                                        \
            ::o1602::test::report(__FILE__, __LINE__, #expr); \
            throw ::o1602::test::Failure{};                   \
        }                                                     \
    } while (false)

#define SKIP(reason)            \
    throw ::o1602::test::Skip { \
        reason                  \
    }
