// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstring>
#include <exception>

#include "test_framework.hpp"

int main(int argc, char** argv) {
    using namespace o1602::test;
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    int skipped = 0;
    int failed_tests = 0;
    for (const auto& test : registry()) {
        if (filter != nullptr && std::strstr(test.name, filter) == nullptr) {
            continue;
        }
        const int before = failures();
        try {
            test.fn();
        } catch (const Skip& skip) {
            std::printf("[ SKIP ] %s (%s)\n", test.name, skip.reason.c_str());
            ++skipped;
            continue;
        } catch (const Failure&) {
            // already reported
        } catch (const std::exception& e) {
            std::fprintf(stderr, "  unexpected exception: %s\n", e.what());
            ++failures();
        }
        ++run;
        const bool ok = failures() == before;
        if (!ok) {
            ++failed_tests;
        }
        std::printf("[%s] %s\n", ok ? "  OK  " : " FAIL ", test.name);
    }
    std::printf("\n%d run, %d failed, %d skipped\n", run, failed_tests, skipped);
    return failed_tests == 0 ? 0 : 1;
}
