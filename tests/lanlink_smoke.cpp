/* lanlink_smoke.cpp — two real Cores syncing over REAL TCP sockets (loopback),
 * through voidmaiz/lanlink.hpp. net_smoke proves the sync over an in-memory wire;
 * this proves the pipes: the join handshake, a person's Allow, the stream
 * envelope in both directions, and a Deny. */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/lanlink.hpp"
#include "voidmaiz/net.hpp"

#include "cJSON.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

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

/* One device: a Core, a Network over it, and a LanSession carrying its frames. */
struct Device {
    std::string name;
    Core core;
    std::unique_ptr<Network> net;
    LanSession lan;
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

    /* The application's loop, exactly as a host writes it. */
    void frame() {
        long long now = now_ms();
        lan.poll(now);
        for (auto& e : lan.take_events()) {
            events.push_back(e.text);
            if (e.kind == LanEvent::Kind::Connected) net->connect(e.link, now);
            if (e.kind == LanEvent::Kind::Disconnected) net->disconnect(e.link, now);
        }
        for (auto& f : lan.take_frames()) net->receive(f.link, f.frame, now);
        Surfaces s;
        s.begin_frame();
        net->tick(now, {}, s);
        for (auto& o : net->take_outgoing()) lan.send(o.link, o.frame);
        lan.poll(now); // flush what tick produced
    }

    bool has_rune(const std::string& rune) {
        std::string st = core.export_state();
        return st.find("\"" + rune + "\"") != std::string::npos;
    }
};

template <class F>
static bool run_until(std::vector<Device*> ds, F done, int ms) {
    long long until = now_ms() + ms;
    while (now_ms() < until) {
        for (auto* d : ds) d->frame();
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

int main() {
    lan::Ipv4 loopback = *lan::Ipv4::parse("127.0.0.1");

    CHECK(lan_address_ok(loopback));
    CHECK(lan_address_ok(*lan::Ipv4::parse("192.168.1.9")));
    CHECK(!lan_address_ok(*lan::Ipv4::parse("8.8.8.8")));      // never the internet
    CHECK(!lan_address_ok(*lan::Ipv4::parse("100.64.1.1")));   // nor mobile data

    Device host("Ana", "replica-lan-ana-000000001", true);
    Device joiner("Bo", "replica-lan-bo-0000000002", false);
    host.core.dispatch("rune new card shared-note");
    host.core.dispatch("set shared-note text hello-from-ana");

    LanOptions ho;
    ho.app = "lanlink-smoke";
    ho.id = "replica-lan-ana-000000001";
    ho.name = "Ana";
    ho.host = true;
    ho.beacon_port = 47851; // test ports: no cross-talk with a real session
    ho.tcp_port = 0;        // any free port
    std::string err;
    CHECK(host.lan.start(ho, &err));
    CHECK(host.lan.tcp_port() != 0);

    LanOptions jo = ho;
    jo.id = "replica-lan-bo-0000000002";
    jo.name = "Bo";
    jo.host = false;
    jo.beacon_port = 47852;
    CHECK(joiner.lan.start(jo, &err));

    // refused before a socket is opened: not a LAN address
    CHECK(!joiner.lan.join(*lan::Ipv4::parse("8.8.8.8"), 47812, &err));
    CHECK(err.find("private network") != std::string::npos);

    // ── join: nothing moves until a PERSON allows it ─────────────────────────
    CHECK(joiner.lan.join(loopback, host.lan.tcp_port(), &err));
    CHECK(run_until({&host, &joiner}, [&] { return !host.lan.requests().empty(); }, 3000));
    auto reqs = host.lan.requests();
    CHECK(reqs.size() == 1 && reqs[0].name == "Bo");
    CHECK(!joiner.has_rune("shared-note")); // not before Allow
    CHECK(host.lan.connected() == 0);

    host.lan.allow(reqs[0].token);
    CHECK(run_until({&host, &joiner}, [&] { return joiner.has_rune("shared-note"); }, 8000));
    CHECK(host.lan.connected() == 1 && joiner.lan.connected() == 1);
    CHECK(joiner.core.export_state().find("hello-from-ana") != std::string::npos);

    // ── and back: the joiner's edit reaches the host ─────────────────────────
    joiner.core.dispatch("use team");
    joiner.core.dispatch("rune new card from-bo");
    CHECK(run_until({&host, &joiner}, [&] { return host.has_rune("from-bo"); }, 8000));

    // ── a second joiner, denied: it learns so, and receives nothing ──────────
    Device stranger("Cy", "replica-lan-cy-0000000003", false);
    LanOptions so = jo;
    so.id = "replica-lan-cy-0000000003";
    so.name = "Cy";
    so.beacon_port = 47853;
    CHECK(stranger.lan.start(so, &err));
    CHECK(stranger.lan.join(loopback, host.lan.tcp_port(), &err));
    CHECK(run_until({&host, &joiner, &stranger}, [&] { return !host.lan.requests().empty(); }, 3000));
    auto r2 = host.lan.requests();
    CHECK(r2.size() == 1 && r2[0].name == "Cy");
    if (!r2.empty()) host.lan.deny(r2[0].token);
    bool denied = run_until({&host, &joiner, &stranger}, [&] {
        for (auto& e : stranger.events)
            if (e.find("said no") != std::string::npos) return true;
        return false;
    }, 3000);
    CHECK(denied);
    CHECK(!stranger.has_rune("shared-note"));
    CHECK(host.lan.connected() == 1); // Bo is still there

    // ── leaving is noticed ────────────────────────────────────────────────────
    joiner.lan.stop();
    bool gone = run_until({&host}, [&] { return host.lan.connected() == 0; }, 3000);
    CHECK(gone);

    if (failures) {
        std::cerr << "lanlink_smoke: " << failures << " FAILED\n";
        return 1;
    }
    std::cout << "lanlink_smoke: all ok\n";
    return 0;
}
