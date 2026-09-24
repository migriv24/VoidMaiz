/* rnslink_smoke.cpp — two real Cores syncing over Reticulum (voidmaiz/rnslink.hpp),
 * over real UDP on loopback, in SEPARATE PROCESSES (Reticulum's transport is
 * process-wide, so one process is one device).
 *
 * The parent is the host. It starts itself again as a child for each other
 * device and reads what the child printed when it exits:
 *   1. a joiner: a person's Allow, then the document both ways;
 *   2. a stranger: Denied, and it receives nothing;
 *   3. the joiner again, same storage (so the same Reticulum identity): let back
 *      in WITHOUT a second request, because "allowed" is kept by proven identity.
 *
 * Every device listens on its own port. Children forward to the host; the host
 * forwards nowhere useful and answers whoever spoke to it (learn_peers), which is
 * the same trick a desktop uses to answer a phone that drops broadcast.
 *
 *   maiz_rnslink_smoke                       the whole test (the host)
 *   maiz_rnslink_smoke child <name> <replica> <dir> <listen> <host port> <seconds>
 */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/net.hpp"
#include "voidmaiz/rnslink.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

using namespace maiz;

static long long now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static const char* kCard = R"({"glyph":"card","label":"Card","fields":["text"]})";
static const char* kApp = "rnslink-smoke";
static const std::uint16_t kHostPort = 47871;

/* One device: a Core, a Network over it, and an RnsSession carrying its frames. */
struct Device {
    std::string name;
    Core core;
    std::unique_ptr<Network> net;
    RnsSession rns;
    std::vector<std::string> events;

    Device(std::string n, const std::string& replica, bool founder) : name(std::move(n)) {
        core.register_glyph(kCard);
        if (founder) {
            core.dispatch("mantle new team");
            core.dispatch("use team");
        }
        voidpalabra::Replica r;
        voidpalabra::Replica::create(replica, r, nullptr);
        NetOptions o;
        o.persist = [](const std::string&) {};
        o.timing.resend = 300;
        net = std::make_unique<Network>(core, std::move(r), std::move(o));
        net->settings().self.name = name;
    }

    /* The application's loop, exactly as a host writes it (the same as over the LAN). */
    void frame() {
        long long now = now_ms();
        rns.poll(now);
        for (auto& e : rns.take_events()) {
            events.push_back(e.text);
            if (e.kind == LanEvent::Kind::Connected) net->connect(e.link, now);
            if (e.kind == LanEvent::Kind::Disconnected) net->disconnect(e.link, now);
        }
        for (auto& f : rns.take_frames()) net->receive(f.link, f.frame, now);
        Surfaces s;
        s.begin_frame();
        net->tick(now, {}, s);
        for (auto& o : net->take_outgoing()) rns.send(o.link, o.frame);
    }

    bool has_rune(const std::string& rune) {
        std::string st = core.export_state();
        return st.find("\"" + rune + "\"") != std::string::npos;
    }

    bool saw(const std::string& text) {
        for (auto& e : events)
            if (e.find(text) != std::string::npos) return true;
        return false;
    }
};

template <class F>
static bool run_until(Device& d, F done, int ms) {
    long long until = now_ms() + ms;
    while (now_ms() < until) {
        d.frame();
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

// ── a child: one other device ───────────────────────────────────────────────
static int child(int argc, char** argv) {
    if (argc < 8) return 2;
    const std::string name = argv[2], replica = argv[3], dir = argv[4];
    const int listen = std::atoi(argv[5]), host_port = std::atoi(argv[6]), seconds = std::atoi(argv[7]);
    auto say = [](const std::string& s) { std::printf("%s\n", s.c_str()), std::fflush(stdout); };

    Device me(name, replica, false);
    RnsOptions o;
    o.app = kApp;
    o.id = replica;
    o.name = name;
    o.storage_dir = dir;
    o.port = (std::uint16_t)listen;
    o.forward_host = "127.0.0.1";
    o.forward_port = (std::uint16_t)host_port;
    o.listen_host = "127.0.0.1";
    o.announce_ms = 1000;
    std::string err;
    if (!me.rns.start(o, &err)) return say("RESULT fail start: " + err), 1;
    say("IDENTITY " + me.rns.identity());

    bool asked = false;
    const long long until = now_ms() + seconds * 1000LL;
    long long linked_at = -1;
    while (now_ms() < until) {
        me.frame();
        if (!asked)
            for (const RnsPeer& p : me.rns.peers())
                if (p.host) {
                    if (!me.rns.join(p.destination, &err)) return say("RESULT fail join: " + err), 1;
                    asked = true;
                }
        if (me.saw("the host said no")) return say("DENIED"), say("RESULT ok"), 0;
        if (me.rns.connected() && linked_at < 0) linked_at = now_ms();
        if (me.has_rune("shared-note") && !me.has_rune("from-" + name)) {
            say("GOT shared-note");
            me.core.dispatch("use team");
            me.core.dispatch("rune new card from-" + name);
        }
        // done: we hold the host's note and the host has had time to take ours
        if (me.has_rune("from-" + name) && me.net && now_ms() - linked_at > 4000) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    for (auto& e : me.events) say("EVENT " + e);
    say(me.has_rune("shared-note") ? "RESULT ok" : "RESULT fail never received the host's note");
    me.rns.stop(); // a goodbye the host hears
    return 0;
}

// ── the host: the whole test ────────────────────────────────────────────────
struct Child {
    FILE* pipe = nullptr;
    std::string out;
    void finish() {
        char buf[512];
        while (pipe && std::fgets(buf, sizeof buf, pipe)) out += buf;
        if (pipe) pclose(pipe);
        pipe = nullptr;
    }
    bool said(const std::string& line) const { return out.find(line) != std::string::npos; }
};

static Child spawn(const std::string& self, const std::string& args) {
    // the host keeps polling while the child runs: popen returns at once, and the
    // child's few lines wait in the pipe until finish()
    std::string cmd = "\"" + self + "\" child " + args;
#ifdef _WIN32
    cmd = "\"" + cmd + "\""; // cmd.exe strips one pair of outer quotes
#endif
    Child c;
    c.pipe = popen(cmd.c_str(), "r");
    return c;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "child") return child(argc, argv);

    namespace fs = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() / ("maiz-rnslink-smoke-" + std::to_string(now_ms()));
    fs::create_directories(tmp);
    const std::string self = fs::absolute(argv[0]).string();

    Device host("Ana", "replica-rns-ana-000000001", true);
    host.core.dispatch("rune new card shared-note");
    host.core.dispatch("set shared-note text hello-from-ana");
    RnsOptions ho;
    ho.app = kApp;
    ho.id = "replica-rns-ana-000000001";
    ho.name = "Ana";
    ho.host = true;
    ho.storage_dir = (tmp / "ana").string();
    ho.port = kHostPort;
    ho.listen_host = "127.0.0.1";
    ho.forward_host = "127.0.0.1";
    ho.forward_port = kHostPort + 9; // nobody there: the host answers whoever spoke (learn_peers)
    ho.announce_ms = 1000;
    std::string err;
    CHECK(host.rns.start(ho, &err));
    if (!err.empty()) std::cerr << "start: " << err << "\n";

    // refused before anything opens
    {
        RnsSession bad;
        RnsOptions bo = ho;
        bo.app = "Not An Aspect";
        CHECK(!bad.start(bo, &err));
    }

    const std::string bo_dir = (tmp / "bo").string();
    const std::string bo_args = "Bo replica-rns-bo-0000000002 \"" + bo_dir + "\" " + std::to_string(kHostPort + 1) +
                                " " + std::to_string(kHostPort) + " 40";

    // ── 1. a joiner: nothing moves until a PERSON allows it ─────────────────
    Child bo = spawn(self, bo_args);
    CHECK(run_until(host, [&] { return !host.rns.requests().empty(); }, 30000));
    auto reqs = host.rns.requests();
    CHECK(reqs.size() == 1 && reqs.size() && reqs[0].name == "Bo");
    CHECK(host.rns.connected() == 0);
    if (!reqs.empty()) {
        CHECK(!reqs[0].identity.empty()); // proven on the link, not claimed
        host.rns.allow(reqs[0].token);
    }
    CHECK(run_until(host, [&] { return host.has_rune("from-Bo"); }, 30000)); // and back
    CHECK(run_until(host, [&] { return host.rns.connected() == 0; }, 30000)); // Bo said goodbye
    bo.finish();
    CHECK(bo.said("GOT shared-note"));
    CHECK(bo.said("RESULT ok"));
    if (!bo.said("RESULT ok")) std::cerr << "--- Bo:\n" << bo.out;

    // ── 2. a stranger, denied: it learns so, and receives nothing ───────────
    Child cy = spawn(self, "Cy replica-rns-cy-0000000003 \"" + (tmp / "cy").string() + "\" " +
                               std::to_string(kHostPort + 2) + " " + std::to_string(kHostPort) + " 30");
    CHECK(run_until(host, [&] { return !host.rns.requests().empty(); }, 30000));
    auto r2 = host.rns.requests();
    CHECK(r2.size() == 1 && r2.size() && r2[0].name == "Cy");
    if (!r2.empty()) host.rns.deny(r2[0].token);
    run_until(host, [] { return false; }, 1500); // let the answer travel
    cy.finish();
    CHECK(cy.said("DENIED"));
    CHECK(!cy.said("GOT shared-note"));
    if (!cy.said("DENIED")) std::cerr << "--- Cy:\n" << cy.out;

    // ── 3. Bo again, same identity: back in without knocking ────────────────
    // The same storage dir, so the same Reticulum identity. A NEW replica id,
    // because this child's document starts empty: a replica id names one
    // continuous history (an application persists its replica; this test does not).
    Child bo2 = spawn(self, "Bo replica-rns-bo-0000000004 \"" + bo_dir + "\" " + std::to_string(kHostPort + 1) +
                                " " + std::to_string(kHostPort) + " 40");
    bool asked_again = false;
    CHECK(run_until(host, [&] {
        if (!host.rns.requests().empty()) asked_again = true;
        return host.saw("Bo is back");
    }, 30000));
    CHECK(!asked_again);
    run_until(host, [&] { return host.rns.connected() == 0; }, 30000);
    bo2.finish();
    CHECK(bo2.said("RESULT ok"));
    if (!bo2.said("RESULT ok")) std::cerr << "--- Bo again:\n" << bo2.out;

    host.rns.stop();
    std::error_code ec;
    fs::remove_all(tmp, ec);
    if (failures) {
        std::cerr << "rnslink_smoke: " << failures << " FAILED\n";
        return 1;
    }
    std::cout << "rnslink_smoke: all ok\n";
    return 0;
}
