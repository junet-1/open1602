// SPDX-License-Identifier: GPL-3.0-or-later
// The multiplayer (ADR 0011): messages as bytes and back, and whole sessions of a host and other
// computers over the in-memory network, with latency, jitter, desyncs, lost connections, slow
// computers and players who try to give orders for others.

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../test_framework.hpp"

#include "o1602/assets/building_table.hpp"
#include "o1602/assets/figure_table.hpp"
#include "o1602/net/enet_transport.hpp"
#include "o1602/net/loopback.hpp"
#include "o1602/net/protocol.hpp"
#include "o1602/net/session.hpp"
#include "o1602/sim/random.hpp"
#include "o1602/sim/save_game.hpp"
#include "o1602/sim/ship.hpp"
#include "o1602/sim/simulation.hpp"

using namespace o1602;

namespace {

constexpr std::uint64_t kContent = 0x1602;
constexpr std::uint32_t kStepMs = 16;  // a frame of the program at about 60 per second

const assets::BuildingTable& buildings() {
    static const assets::BuildingTable table;
    return table;
}

const assets::FigureTable& figures() {
    static const assets::FigureTable table;
    return table;
}

/// One computer's game: the host builds it, the others load it from the host's savegame.
struct Game {
    std::unique_ptr<sim::World> world;
    std::unique_ptr<sim::Simulation> simulation;

    static Game fresh(std::size_t humans = 3) {
        Game g;
        g.world = std::make_unique<sim::World>(64, 64);
        std::vector<sim::Player> players(humans + 1);
        for (std::size_t p = 0; p < players.size(); ++p) {
            players[p].role = p < humans ? sim::PlayerRole::Human : sim::PlayerRole::Computer;
            players[p].banner = static_cast<sim::BannerId>(p);
            players[p].name = "Spieler " + std::to_string(p);
            players[p].money = 1000;
        }
        g.world->set_players(players);
        for (std::size_t p = 0; p < humans; ++p) {
            // A ship each, so that nobody is out of the game for having nothing (200 s).
            g.world->add_ship(sim::make_ship("Schiff", 10 + 4 * static_cast<std::uint32_t>(p), 10, 21,
                                             static_cast<sim::PlayerId>(p)));
        }
        g.simulation = std::make_unique<sim::Simulation>(*g.world, buildings(), figures());
        return g;
    }

    sim::Simulation* load(std::span<const std::uint8_t> save) {
        auto loaded = sim::read_game(save, buildings(), nullptr, &figures());
        if (!loaded) {
            return nullptr;
        }
        ++loads;
        if (diverge) {
            loaded.value().player(0)->money += 1;  // as a computer with another build would compute
        }
        simulation.reset();  // it refers to the world
        world = std::make_unique<sim::World>(std::move(loaded).value());
        simulation = std::make_unique<sim::Simulation>(*world, buildings(), figures());
        return simulation.get();
    }

    int loads = 0;
    bool diverge = false;
};

/// A computer joining the host: its game and its session.
struct Computer {
    Game game;
    std::unique_ptr<net::ClientSession> session;
    const net::Transport* transport = nullptr;  ///< owned by the session; to cut the connection
    std::vector<net::SessionEvent> events;

    Computer(net::LoopbackNetwork& network, std::string name, std::uint64_t token = 0, std::uint64_t content = kContent,
             net::SessionConfig config = {}) {
        net::Hello hello;
        hello.content = content;
        hello.name = std::move(name);
        hello.token = token;
        auto link = network.connect();
        transport = link.get();
        session = std::make_unique<net::ClientSession>(
            std::move(link), hello, [this](std::span<const std::uint8_t> save) { return game.load(save); }, config);
    }

    void update(std::uint32_t ms, const sim::Simulation::BeforeTick& before = {}) {
        session->update(ms, before);
        for (auto& e : session->take_events()) {
            events.push_back(std::move(e));
        }
    }

    [[nodiscard]] std::size_t count(net::SessionEvent::Kind kind) const {
        std::size_t n = 0;
        for (const auto& e : events) {
            n += e.kind == kind ? 1 : 0;
        }
        return n;
    }
};

struct Host {
    Game game = Game::fresh();
    std::unique_ptr<net::HostSession> session;
    std::vector<net::SessionEvent> events;

    explicit Host(net::LoopbackNetwork& network, net::SessionConfig config = {}) {
        session = std::make_unique<net::HostSession>(network.listen(), *game.simulation, buildings(), 0, "Gastgeber",
                                                     kContent, config);
    }

    void update(std::uint32_t ms, const sim::Simulation::BeforeTick& before = {}) {
        session->update(ms, before);
        for (auto& e : session->take_events()) {
            events.push_back(std::move(e));
        }
    }

    [[nodiscard]] std::size_t count(net::SessionEvent::Kind kind) const {
        std::size_t n = 0;
        for (const auto& e : events) {
            n += e.kind == kind ? 1 : 0;
        }
        return n;
    }
};

/// Time passes for the network and every computer.
void step(net::LoopbackNetwork& network, Host& host, const std::vector<Computer*>& computers,
          std::uint32_t ms = kStepMs) {
    network.advance(ms);
    host.update(ms);
    for (Computer* c : computers) {
        c->update(ms);
    }
}

void run_for(net::LoopbackNetwork& network, Host& host, const std::vector<Computer*>& computers,
             std::uint32_t total_ms) {
    for (std::uint32_t t = 0; t < total_ms; t += kStepMs) {
        step(network, host, computers);
    }
}

/// The fingerprint each computer's game had after `frame` frames (taken just before the next one).
struct FrameProbe {
    std::uint64_t frame;
    std::optional<std::uint64_t> hash;

    sim::Simulation::BeforeTick at(const net::Session& session, const Game& game) {
        return [this, &session, &game] {
            if (session.frames() == frame && !hash) {
                hash = game.simulation->state_hash();
            }
            return true;
        };
    }
};

std::string random_name(sim::Random& r) {
    std::string name = "N";
    const std::uint32_t n = 1 + r.below(12);
    for (std::uint32_t i = 0; i < n; ++i) {
        name.push_back(static_cast<char>('a' + r.below(26)));
    }
    return name;
}

}  // namespace

TEST_CASE("net: every message goes to bytes and back") {
    std::vector<net::Message> all;
    all.emplace_back(net::Hello{net::kProtocolVersion, 77, "Merlin", 2, 1234});
    all.emplace_back(net::CommandBatch{{sim::PlayerRename{1, "Zweiter"}, sim::GameSpeedOrder{1, {}, 4}}});
    all.emplace_back(net::Progress{3, 1000, net::FrameHash{900, 0xABCDEF}});
    all.emplace_back(net::Progress{1, 5, std::nullopt});
    all.emplace_back(net::Welcome{net::kProtocolVersion, 2, 99});
    all.emplace_back(net::Refused{net::RefuseReason::Content});
    all.emplace_back(net::Snapshot{2, 400, 4, true, {1, 2, 3, 4, 5}});
    net::Turn turn;
    turn.first = 10;
    turn.closed = 20;
    turn.lead = 3;
    turn.frames = {net::FrameCommands{12, {sim::PlayerRename{0, "A"}}},
                   net::FrameCommands{19, {sim::PlayerRename{1, "B"}, sim::PlayerRename{2, "C"}}}};
    all.emplace_back(turn);
    all.emplace_back(net::Roster{{net::Member{0, "Gastgeber", net::Member::State::Playing, true},
                                  net::Member{sim::kNoPlayer, "Zuschauer", net::Member::State::Lagging, false}}});
    all.emplace_back(net::Desync{300, {1, 2}});
    for (const net::Message& m : all) {
        const auto bytes = net::encode(m);
        const auto back = net::decode(bytes);
        REQUIRE(back.has_value());
        CHECK(back->index() == m.index());
        CHECK(net::encode(*back) == bytes);  // same bytes: every field came back
    }
    const auto decoded = net::decode(net::encode(turn));
    const auto& t = std::get<net::Turn>(*decoded);
    REQUIRE(t.frames.size() == 2);
    CHECK(t.frames[1].frame == 19);
    CHECK(std::get<sim::PlayerRename>(t.frames[1].commands[1]).name == "C");
}

TEST_CASE("net: damaged or hostile bytes give no message") {
    net::Turn turn;
    turn.first = 10;
    turn.closed = 20;
    turn.frames = {net::FrameCommands{12, {sim::PlayerRename{0, "A"}}}};
    const auto bytes = net::encode(turn);
    for (std::size_t n = 0; n < bytes.size(); ++n) {
        CHECK(!net::decode(std::span(bytes.data(), n)));  // cut off anywhere
    }
    auto longer = bytes;
    longer.push_back(0);
    CHECK(!net::decode(longer));                          // bytes left over
    CHECK(!net::decode(std::vector<std::uint8_t>{200}));  // unknown type

    net::Turn backwards = turn;
    backwards.frames = {net::FrameCommands{15, {}}, net::FrameCommands{12, {}}};
    CHECK(!net::decode(net::encode(backwards)));  // frames not ascending
    net::Turn outside = turn;
    outside.frames = {net::FrameCommands{20, {}}};
    CHECK(!net::decode(net::encode(outside)));  // a frame not yet closed
    net::Hello hello;
    hello.name = std::string(net::kMaxNameLength + 1, 'x');
    CHECK(!net::decode(net::encode(hello)));  // name too long

    // Random bytes behind every type: decoding never crashes and keeps the limits.
    sim::Random r(1602, 7);
    for (int i = 0; i < 20000; ++i) {
        std::vector<std::uint8_t> noise(1 + r.below(64));
        for (auto& b : noise) {
            b = static_cast<std::uint8_t>(r.next());
        }
        noise[0] = static_cast<std::uint8_t>(r.below(10));
        if (const auto m = net::decode(noise)) {
            CHECK(net::encode(*m).size() <= noise.size() + 1024);
        }
    }
}

TEST_CASE("net: two computers join, everyone plays, all games stay the same") {
    net::LoopbackNetwork network(40, 25);
    Host host(network);
    Computer a(network, "Anna");
    Computer b(network, "Bernd");
    run_for(network, host, {&a, &b}, 1000);

    REQUIRE(a.session->playing());
    REQUIRE(b.session->playing());
    CHECK(a.session->local_seat() == 1);
    CHECK(b.session->local_seat() == 2);
    CHECK(a.game.loads == 1);
    REQUIRE(host.session->roster().size() == 3);
    CHECK(host.session->roster()[0].host);
    CHECK(a.session->roster() == host.session->roster());
    CHECK(host.game.world->players()[1].name == "Anna");  // each player's name, as a command
    CHECK(b.game.world->players()[1].name == "Anna");

    // Every player gives orders now and then, also speed changes and pauses.
    sim::Random r(42, 1);
    Computer* computers[] = {&a, &b};
    for (int i = 0; i < 2500; ++i) {
        const std::uint32_t who = r.below(40);
        if (who == 0) {
            host.session->submit(sim::PlayerRename{0, random_name(r)});
        } else if (who <= 2) {
            Computer& c = *computers[who - 1];
            c.session->submit(sim::PlayerRename{c.session->local_seat(), random_name(r)});
        } else if (who == 3) {
            const auto op = static_cast<sim::GameSpeedOrder::Op>(r.below(3));
            a.session->submit(sim::GameSpeedOrder{1, op, 1 + r.below(4)});
        }
        step(network, host, {&a, &b});
    }
    host.session->submit(sim::GameSpeedOrder{0, sim::GameSpeedOrder::Op::Resume, 1});

    // Now the fingerprint of every game at the same frame, a few seconds ahead.
    FrameProbe ph{host.session->frames() + 60, {}};
    FrameProbe pa = ph;
    FrameProbe pb = ph;
    for (int i = 0; i < 400 && !(ph.hash && pa.hash && pb.hash); ++i) {
        network.advance(kStepMs);
        host.update(kStepMs, ph.at(*host.session, host.game));
        a.update(kStepMs, pa.at(*a.session, a.game));
        b.update(kStepMs, pb.at(*b.session, b.game));
    }
    REQUIRE(ph.hash.has_value());
    CHECK(pa.hash == ph.hash);
    CHECK(pb.hash == ph.hash);
    CHECK(host.count(net::SessionEvent::Kind::Desync) == 0);
    CHECK(host.session->fingerprints_compared() > 10);
    CHECK(host.session->fingerprints_matched() == host.session->fingerprints_compared());
    CHECK(a.game.loads == 1);                                         // never had to load the game anew
    CHECK(host.session->kept_frames() <= host.session->lead() + 2u);  // nothing piles up
}

TEST_CASE("net: nobody gives orders for another player") {
    net::LoopbackNetwork network(10, 0);
    Host host(network);
    Computer a(network, "Anna");
    run_for(network, host, {&a}, 500);
    REQUIRE(a.session->playing());
    const std::string before = host.game.world->players()[0].name;

    a.session->submit(sim::PlayerRename{0, "Gekapert"});  // the computer itself refuses
    // A computer that skips its own check: the host drops the command.
    auto raw = network.connect();
    std::vector<net::TransportEvent> ev;
    network.advance(50);
    raw->poll(ev);
    raw->send(0, net::encode(net::Hello{net::kProtocolVersion, kContent, "Mallory", sim::kNoPlayer, 0}));
    for (int i = 0; i < 20; ++i) {
        step(network, host, {&a});
    }
    raw->send(0, net::encode(net::CommandBatch{{sim::PlayerRename{0, "Gekapert"}, sim::PlayerRename{1, "Gekapert"}}}));
    run_for(network, host, {&a}, 500);
    CHECK(host.game.world->players()[0].name == before);
    CHECK(host.game.world->players()[1].name == "Anna");
    CHECK(a.game.world->players()[0].name == before);

    // A host's own player cannot order for others either.
    host.session->submit(sim::PlayerRename{1, "Gekapert"});
    run_for(network, host, {&a}, 300);
    CHECK(a.game.world->players()[1].name == "Anna");

    // A flood: no more than the limit per frame reaches the game.
    std::uint64_t before_frames = host.session->closed();
    raw->send(0, net::encode(net::CommandBatch{std::vector<sim::Command>(1000, sim::PlayerRename{2, "Flut"})}));
    run_for(network, host, {&a}, 300);
    CHECK(host.session->closed() > before_frames);

    // Garbage: the computer is sent away, the game goes on.
    raw->send(0, std::vector<std::uint8_t>{0xEE, 1, 2});
    run_for(network, host, {&a}, 300);
    CHECK(host.session->roster().size() == 2);
    CHECK(!a.session->ended());
}

TEST_CASE("net: a computer that diverges is noticed and gets the game anew") {
    net::LoopbackNetwork network(30, 10);
    net::SessionConfig config;
    config.hash_interval = 20;
    Host host(network, config);
    Computer a(network, "Anna", 0, kContent, config);
    Computer b(network, "Bernd", 0, kContent, config);
    run_for(network, host, {&a, &b}, 1500);
    REQUIRE(a.session->playing());
    CHECK(host.count(net::SessionEvent::Kind::Desync) == 0);

    // Something changes on one computer only, as a bug in the simulation would.
    a.game.world->player(0)->money += 1;
    run_for(network, host, {&a, &b}, 3000);
    CHECK(host.count(net::SessionEvent::Kind::Desync) == 1);
    CHECK(a.count(net::SessionEvent::Kind::Desync) == 1);
    CHECK(b.count(net::SessionEvent::Kind::Desync) == 1);  // everyone hears of it
    CHECK(a.game.loads == 2);
    CHECK(b.game.loads == 1);
    CHECK(a.game.world->player(0)->money == host.game.world->player(0)->money);
    const auto matched = host.session->fingerprints_matched();
    run_for(network, host, {&a, &b}, 2000);
    CHECK(host.session->fingerprints_matched() > matched);  // equal again
    CHECK(host.count(net::SessionEvent::Kind::Desync) == 1);

    // A computer that diverges again and again, even right after loading (another build), is sent
    // away after a few tries.
    a.game.diverge = true;
    a.game.world->player(0)->money += 1;
    run_for(network, host, {&a, &b}, 6000);
    CHECK(a.game.loads == 2 + static_cast<int>(config.max_resyncs));
    CHECK(a.session->ended());
    CHECK(a.session->end_reason() == net::EndReason::Desync);
    CHECK(!b.session->ended());
}

TEST_CASE("net: into a running game, out and back into the same slot") {
    net::LoopbackNetwork network(25, 10);
    Host host(network);
    Computer a(network, "Anna");
    run_for(network, host, {&a}, 5000);
    host.session->submit(sim::PlayerRename{0, "Später"});

    auto b = std::make_unique<Computer>(network, "Bernd");
    run_for(network, host, {&a, b.get()}, 1500);
    REQUIRE(b->session->playing());
    CHECK(b->session->local_seat() == 2);
    CHECK(b->game.world->players()[0].name == "Später");
    const std::uint64_t token = b->session->token();
    CHECK(token != 0);

    // The connection breaks; the game goes on without Bernd, his slot waits for him.
    network.cut(*b->transport);
    run_for(network, host, {&a, b.get()}, 500);
    CHECK(b->session->ended());
    CHECK(b->session->end_reason() == net::EndReason::HostLeft);
    CHECK(host.count(net::SessionEvent::Kind::Left) == 1);
    CHECK(host.session->roster().size() == 2);

    // Someone else comes meanwhile: slot 2 is kept for Bernd only while he is away? No: it is free.
    // Bernd comes back with his token and plays slot 2 again.
    b = std::make_unique<Computer>(network, "Bernd", token);
    run_for(network, host, {&a, b.get()}, 1500);
    REQUIRE(b->session->playing());
    CHECK(b->session->local_seat() == 2);
    CHECK(b->session->token() == token);
    b->session->submit(sim::PlayerRename{2, "Bernd wieder da"});
    run_for(network, host, {&a, b.get()}, 1000);
    CHECK(host.game.world->players()[2].name == "Bernd wieder da");
    CHECK(a.game.world->players()[2].name == "Bernd wieder da");
    CHECK(host.count(net::SessionEvent::Kind::Desync) == 0);
    CHECK(host.session->fingerprints_matched() == host.session->fingerprints_compared());
}

TEST_CASE("net: without a free slot a computer watches; other data or versions are refused") {
    net::LoopbackNetwork network(5, 0);
    Host host(network);
    Computer a(network, "Anna");
    Computer b(network, "Bernd");
    Computer c(network, "Clara");  // three humans in the world, all taken
    run_for(network, host, {&a, &b, &c}, 1000);
    REQUIRE(c.session->playing());
    CHECK(c.session->local_seat() == sim::kNoPlayer);
    c.session->submit(sim::PlayerRename{3, "Zuschauer"});
    run_for(network, host, {&a, &b, &c}, 500);
    CHECK(host.game.world->players()[3].name == "Spieler 3");
    CHECK(host.session->roster().size() == 4);

    Computer other(network, "Dora", 0, kContent + 1);
    run_for(network, host, {&a, &b, &c, &other}, 500);
    CHECK(other.session->ended());
    CHECK(other.session->end_reason() == net::EndReason::Refused);
    REQUIRE(!other.events.empty());
    CHECK(other.events.back().refused == net::RefuseReason::Content);
    CHECK(!other.session->playing());

    net::SessionConfig closed;
    closed.spectators = false;
    net::LoopbackNetwork network2(5, 0);
    Host host2(network2, closed);
    Computer x(network2, "X");
    Computer y(network2, "Y");
    Computer z(network2, "Z");
    run_for(network2, host2, {&x, &y, &z}, 500);
    CHECK(z.session->ended());
    CHECK(z.events.back().refused == net::RefuseReason::Full);

    // No host at all.
    net::LoopbackNetwork empty(5, 0);
    Computer lonely(empty, "Niemand");
    empty.advance(50);
    lonely.update(50);
    CHECK(lonely.session->ended());
    CHECK(lonely.session->end_reason() == net::EndReason::ConnectFailed);
}

TEST_CASE("net: the host waits for a slow computer, then goes on without a frozen one") {
    net::LoopbackNetwork network(20, 0);
    net::SessionConfig config;
    config.stall_ms = 2000;
    config.resync_behind = 100;
    Host host(network, config);
    Computer a(network, "Anna", 0, kContent, config);
    Computer b(network, "Bernd", 0, kContent, config);
    run_for(network, host, {&a, &b}, 1000);
    REQUIRE(b.session->playing());

    // Bernd's computer stands still (debugger, window dragged): the others wait for a while.
    const std::uint64_t at = host.session->frames();
    const std::uint64_t ahead = host.session->lead() + config.max_ahead_ms / sim::Simulation::kTickMs;
    run_for(network, host, {&a}, 3000);
    CHECK(host.session->frames() <= at + ahead + 2);
    CHECK(host.session->waiting());
    CHECK(host.count(net::SessionEvent::Kind::Lagging) == 0);
    // ... and then go on without it.
    run_for(network, host, {&a}, 6000);
    CHECK(host.count(net::SessionEvent::Kind::Lagging) == 1);
    CHECK(host.session->frames() > at + ahead + 20);
    CHECK(!host.session->waiting());
    CHECK(a.session->frames() + 10 > host.session->frames());

    // Back, far behind: Bernd gets the game as it is now instead of running every frame.
    run_for(network, host, {&a, &b}, 3000);
    CHECK(b.game.loads == 2);
    CHECK(b.session->frames() + 10 > host.session->frames());
    CHECK(host.session->roster()[2].state == net::Member::State::Playing);
    CHECK(host.count(net::SessionEvent::Kind::Desync) == 0);
}

TEST_CASE("net: a long session keeps its memory and its frames in step") {
    net::LoopbackNetwork network(60, 40);
    net::SessionConfig config;
    config.hash_interval = 50;
    Host host(network, config);
    Computer a(network, "Anna", 0, kContent, config);
    std::size_t most_kept = 0;
    std::uint64_t most_in_hand = 0;
    sim::Random r(9, 9);
    // Ten minutes of game at speed 4 and 8.
    host.session->submit(sim::GameSpeedOrder{0, sim::GameSpeedOrder::Op::SetSpeed, 4});
    for (std::uint32_t t = 0; t < 10 * 60 * 1000; t += kStepMs) {
        if (t == 5 * 60 * 1000) {
            a.session->submit(sim::GameSpeedOrder{1, sim::GameSpeedOrder::Op::SetSpeed, 8});
        }
        if (r.below(100) == 0) {
            a.session->submit(sim::PlayerRename{1, random_name(r)});
        }
        step(network, host, {&a});
        most_kept = std::max(most_kept, host.session->kept_frames());
        most_in_hand = std::max(most_in_hand, a.session->frames_in_hand());
    }
    CHECK(host.session->frames() > 10 * 60 * 20 * 4);  // the speed was kept, nobody slowed it
    CHECK(most_kept <= config.max_lead + 2u);
    CHECK(most_in_hand <= config.max_lead + 40u);
    CHECK(host.session->fingerprints_compared() > 100);
    CHECK(host.session->fingerprints_matched() == host.session->fingerprints_compared());
    CHECK(a.game.loads == 1);
    // About one message per frame step and computer, not per tick: speed costs no bandwidth.
    CHECK(network.messages_sent() < 2ULL * 10 * 60 * 1000 / kStepMs * 2);
}

TEST_CASE("net: over real UDP on this computer (ENet)") {
    constexpr std::uint16_t kPort = 46021;
    auto listening = net::enet_listen(kPort, 4);
    if (!listening) {
        SKIP("UDP port 46021 is not free here");
    }
    auto connecting = net::enet_connect("127.0.0.1", kPort);
    REQUIRE(connecting.ok());

    Game host_game = Game::fresh();
    net::HostSession host(std::move(listening).value(), *host_game.simulation, buildings(), 0, "Gastgeber", kContent);
    Game game;
    net::Hello hello;
    hello.content = kContent;
    hello.name = "Anna";
    net::ClientSession client(std::move(connecting).value(), hello,
                              [&game](std::span<const std::uint8_t> save) { return game.load(save); });
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5) &&
           !(client.playing() && client.frames() > 20)) {
        host.update(5);
        client.update(5);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(client.playing());
    CHECK(client.local_seat() == 1);
    CHECK(client.frames() > 20);
    CHECK(game.world->players()[1].name == "Anna");
    CHECK(!client.ended());
}
