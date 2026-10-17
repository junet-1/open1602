// SPDX-License-Identifier: GPL-3.0-or-later
// Diplomacy (docs/re/diplomacy.md): treaties, events and the mood that follows from them.

#include "../test_framework.hpp"

#include "o1602/sim/diplomacy.hpp"
#include "o1602/sim/world.hpp"

using namespace o1602;
using sim::DiplomacyEventType;
using sim::Treaty;

namespace {

/// The seven slots of an original game: four players (one human), trader, pirates, natives.
std::vector<sim::Player> original_players() {
    std::vector<sim::Player> p(7);
    p[0].role = sim::PlayerRole::Human;
    for (std::size_t i = 1; i < 4; ++i) {
        p[i].role = sim::PlayerRole::Computer;
    }
    p[4].role = sim::PlayerRole::Trader;
    p[5].role = sim::PlayerRole::Pirates;
    p[6].role = sim::PlayerRole::Natives;
    for (auto& player : p) {
        player.money = 20'000;
    }
    return p;
}

}  // namespace

TEST_CASE("diplomacy: a new game starts with the original's table") {
    sim::Diplomacy d;
    d.reset(original_players());
    // Rows as the original's reset (0x00474380) and the Endlosspiel files have them.
    const int peace[7][7] = {{3, 3, 3, 3, 3, 0, 3}, {3, 3, 3, 3, 3, 0, 3}, {3, 3, 3, 3, 3, 0, 3}, {3, 3, 3, 3, 3, 0, 3},
                             {3, 3, 3, 3, 3, 3, 3}, {0, 0, 0, 0, 3, 3, 0}, {3, 3, 3, 3, 3, 0, 3}};
    const int trade[7][7] = {{0, 0, 0, 0, 3, 3, 3}, {0, 0, 0, 0, 3, 3, 3}, {0, 0, 0, 0, 3, 3, 3}, {0, 0, 0, 0, 3, 3, 3},
                             {3, 3, 3, 3, 0, 0, 0}, {0, 0, 0, 0, 0, 3, 0}, {3, 3, 3, 3, 0, 0, 0}};
    for (sim::PlayerId a = 0; a < 7; ++a) {
        for (sim::PlayerId b = 0; b < 7; ++b) {
            CHECK(static_cast<int>(d.relation(a, b).peace) == peace[a][b]);
            CHECK(static_cast<int>(d.relation(a, b).trade) == trade[a][b]);
        }
    }
    CHECK(d.hostile(0, 5));
    CHECK(d.hostile(5, 6));
    CHECK(!d.hostile(0, 1));
    CHECK(!d.hostile(0, 0));
    CHECK(d.may_trade(0, 5));   // players buy at pirate Kontors from the start
    CHECK(!d.may_trade(5, 0));  // but not the other way round
    CHECK(!d.may_trade(0, 1));  // players need a treaty among themselves
}

TEST_CASE("diplomacy: war, offers and treaties as the original's commands") {
    sim::World world(64, 64);
    world.set_players(original_players());
    sim::Diplomacy& d = world.diplomacy();
    // A trade treaty between the human and computer 1.
    CHECK(d.set_trade(world, 0, 1, Treaty::Offered, 1000));
    CHECK(d.relation(0, 1).trade == Treaty::Offered);
    CHECK(d.relation(1, 0).trade == Treaty::Received);
    CHECK(d.set_trade(world, 1, 0, Treaty::Signed, 2000));
    CHECK(d.may_trade(0, 1));
    CHECK(d.may_trade(1, 0));
    CHECK(!d.set_trade(world, 1, 0, Treaty::Signed, 2000));  // nothing changes, nothing sent
    (void)d.take_notices();

    // War ends the trade treaty and is remembered by the victim.
    CHECK(d.set_peace(world, 0, 1, Treaty::None, 5000));
    CHECK(d.hostile(0, 1));
    CHECK(d.hostile(1, 0));
    CHECK(d.relation(0, 1).trade == Treaty::None);
    CHECK(d.relation(1, 1).war_time_ms == 5000);
    const auto notices = d.take_notices();
    REQUIRE(notices.size() == 2);
    CHECK(notices[0].kind == sim::DiplomacyNotice::Kind::TradeEnded);
    CHECK(notices[1].kind == sim::DiplomacyNotice::Kind::PeaceBroken);
    CHECK(notices[1].to == 1);
    int wars = 0;
    for (const auto& e : d.events(1)) {
        wars += e.type == DiplomacyEventType::WarDeclared && e.partner == 0 ? 1 : 0;
    }
    CHECK(wars == 1);
    // A pending offer still counts as hostile.
    CHECK(d.set_peace(world, 1, 0, Treaty::Offered, 6000));
    CHECK(d.hostile(0, 1));
    CHECK(d.set_peace(world, 0, 1, Treaty::Signed, 7000));
    CHECK(!d.hostile(0, 1));

    // War with the pirates keeps the trade with them.
    world.diplomacy().relation(0, 5).peace = Treaty::Signed;
    world.diplomacy().relation(5, 0).peace = Treaty::Signed;
    CHECK(d.set_peace(world, 5, 0, Treaty::None, 8000));
    CHECK(d.may_trade(0, 5));
}

TEST_CASE("diplomacy: events fade and weigh in the mood") {
    sim::World world(64, 64);
    world.set_players(original_players());
    sim::Diplomacy& d = world.diplomacy();
    CHECK(sim::diplomacy_event_value(DiplomacyEventType::WarDeclared, 0) == -30);
    // Age in 12.8 s steps: 75 steps -> -18, 200 steps -> 0.
    CHECK(sim::diplomacy_event_value(DiplomacyEventType::WarDeclared, 75 * 12'800) == -18);
    CHECK(sim::diplomacy_event_value(DiplomacyEventType::WarDeclared, 200 * 12'800) == 0);
    CHECK(sim::diplomacy_event_value(DiplomacyEventType::PeaceBought, 0) == 70);
    CHECK(sim::diplomacy_event_value(DiplomacyEventType::TradeOffered, 0) == 0);

    d.add_event(1, 0, DiplomacyEventType::HostileGesture, 0);
    CHECK(d.mood(world, 1, 0, 0, 0) == -30);
    CHECK(d.mood(world, 1, 0, 0, 80 * 12'800) == -15);
    // Damage counts against, a quarter, at most 100.
    d.add_damage(1, 0, 200);
    CHECK(d.mood(world, 1, 0, 0, 200 * 12'800) == -50);
    d.add_damage(1, 0, 10'000);
    CHECK(d.mood(world, 1, 0, 0, 200 * 12'800) == -100);
    // The counters fade by 1/256 every 5 s.
    const std::uint32_t before = d.relation(1, 0).damage;
    d.decay(1);
    CHECK(d.relation(1, 0).damage == before * 255 / 256);

    // Pirates: a bought peace replaces bribes against the payer and forgets the damage.
    d.add_damage(5, 0, 400);
    d.add_event(5, 0, DiplomacyEventType::BribedAgainst, 0);
    d.add_event(5, 0, DiplomacyEventType::PeaceBought, 0);
    CHECK(d.relation(5, 0).damage == 0);
    int bribes = 0;
    for (const auto& e : d.events(5)) {
        bribes += e.type == DiplomacyEventType::BribedAgainst ? 1 : 0;
    }
    CHECK(bribes == 0);
    // Types 6-11 are refreshed in place, not repeated.
    d.add_event(5, 0, DiplomacyEventType::PeaceBought, 1000);
    int bought = 0;
    for (const auto& e : d.events(5)) {
        bought += e.type == DiplomacyEventType::PeaceBought ? 1 : 0;
    }
    CHECK(bought == 1);
}

TEST_CASE("diplomacy: the panel's orders") {
    sim::World world(64, 64);
    world.set_players(original_players());
    using Order = sim::DiplomacyOrder;
    // Accepting needs an offer; natives and pirates cannot be addressed.
    CHECK(!sim::apply_diplomacy(world, Order{0, 1, Order::Kind::Trade, Treaty::Signed}));
    CHECK(!sim::apply_diplomacy(world, Order{0, 6, Order::Kind::Peace, Treaty::None}));
    CHECK(!sim::apply_diplomacy(world, Order{0, 5, Order::Kind::Peace, Treaty::Offered}));
    CHECK(sim::apply_diplomacy(world, Order{0, 1, Order::Kind::Trade, Treaty::Offered}));
    CHECK(sim::apply_diplomacy(world, Order{1, 0, Order::Kind::Trade, Treaty::Signed}));
    CHECK(world.diplomacy().may_trade(0, 1));
    // Tribute: the money moves, the partner likes it.
    Order tribute{0, 1, Order::Kind::Tribute};
    tribute.gold = 1700;
    CHECK(sim::apply_diplomacy(world, tribute));
    CHECK(world.players()[0].money == 18'300);
    CHECK(world.players()[1].money == 21'700);
    CHECK(world.diplomacy().relation(1, 0).tribute == 3200);
    CHECK(world.diplomacy().mood(world, 1, 0, 0, 0) > 0);
    tribute.gold = 30'000;
    CHECK(!sim::apply_diplomacy(world, tribute));
    // Breaking the peace is war.
    CHECK(sim::apply_diplomacy(world, Order{0, 2, Order::Kind::Peace, Treaty::None}));
    CHECK(world.diplomacy().hostile(2, 0));
}
