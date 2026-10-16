// SPDX-License-Identifier: GPL-3.0-or-later
// Random numbers of the simulation: specified, reproducible, independent streams.

#include "../test_framework.hpp"

#include "o1602/sim/random.hpp"

using namespace o1602;

TEST_CASE("random: PCG32 reference sequence") {
    // pcg32_srandom(42, 54) of the reference implementation.
    sim::Random r(42, 54);
    CHECK(r.next() == 0xa15c02b7U);
    CHECK(r.next() == 0x7b47f409U);
    CHECK(r.next() == 0xba1d3330U);
    CHECK(r.next() == 0x83d2f293U);
    CHECK(r.next() == 0xbfa4784bU);
    CHECK(r.next() == 0xcbed606eU);
}

TEST_CASE("random: bounded draws and independent streams") {
    sim::Random r(7, 1);
    std::uint32_t counts[6] = {};
    for (int i = 0; i < 60'000; ++i) {
        const std::uint32_t v = r.below(6);
        REQUIRE(v < 6);
        ++counts[v];
    }
    for (const std::uint32_t c : counts) {
        CHECK(c > 9'500);
        CHECK(c < 10'500);
    }
    CHECK(r.below(0) == 0);
    sim::RandomStreams a(5);
    sim::RandomStreams b(5);
    const std::uint32_t first = a[sim::RandomStream::Houses].next();
    b[sim::RandomStream::Animals].next();  // drawing from another stream changes nothing here
    CHECK(b[sim::RandomStream::Houses].next() == first);
    CHECK(a[sim::RandomStream::Animals].next() != a[sim::RandomStream::Pirates].next());
}
