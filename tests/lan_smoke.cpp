/* lan_smoke.cpp — the platform facts LAN discovery needs (voidmaiz/lan.hpp).
 * Opens no socket. The Android multicast lock is compiled by the NDK build and
 * exercised on a device; here we pin everything that is pure. */
#include "voidmaiz/lan.hpp"

#include <iostream>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

using namespace maiz::lan;

static Ipv4 ip(const char* s) { return *Ipv4::parse(s); }

static Interface itf(const char* name, const char* addr, const char* mask) {
    Interface i;
    i.name = name;
    i.address = ip(addr);
    i.netmask = ip(mask);
    i.up = true;
    i.kind = classify(i.address);
    // mirror what interfaces() infers from names
    std::string n = name;
    i.virtual_guess = n.find("vEthernet") != std::string::npos || n.find("docker") == 0;
    i.wireless_guess = n.find("Wi-Fi") != std::string::npos || n.find("wlan") == 0;
    return i;
}

int main() {
    // ── addresses ─────────────────────────────────────────────────────────────
    CHECK(ip("192.168.1.23").host == 0xC0A80117u);
    CHECK(ip("192.168.1.23").text() == "192.168.1.23");
    CHECK(!Ipv4::parse("192.168.1"));
    CHECK(!Ipv4::parse("192.168.1.256"));
    CHECK(!Ipv4::parse("192.168.1.2x"));
    CHECK(!Ipv4::parse("1.2.3.4.5"));
    CHECK(!Ipv4::parse("0001.2.3.4"));

    CHECK(classify(ip("127.0.0.1")) == Kind::Loopback);
    CHECK(classify(ip("169.254.10.2")) == Kind::LinkLocal);
    CHECK(classify(ip("10.0.0.5")) == Kind::Private);
    CHECK(classify(ip("172.16.0.1")) == Kind::Private);
    CHECK(classify(ip("172.31.255.1")) == Kind::Private);
    CHECK(classify(ip("172.32.0.1")) == Kind::Public);
    CHECK(classify(ip("192.168.0.9")) == Kind::Private);
    CHECK(classify(ip("100.64.0.1")) == Kind::CarrierNat);
    CHECK(classify(ip("100.127.255.1")) == Kind::CarrierNat);
    CHECK(classify(ip("100.128.0.1")) == Kind::Public);
    CHECK(classify(ip("8.8.8.8")) == Kind::Public);

    Interface home = itf("Wi-Fi", "192.168.1.23", "255.255.255.0");
    CHECK(home.prefix() == 24);
    CHECK(home.broadcast().text() == "192.168.1.255");
    CHECK(same_subnet(ip("192.168.1.5"), ip("192.168.1.200"), ip("255.255.255.0")));
    CHECK(!same_subnet(ip("192.168.1.5"), ip("192.168.2.5"), ip("255.255.255.0")));

    // ── ranking: the Wi-Fi a person shares with their phone comes first ───────
    {
        std::vector<Interface> all = {
            itf("vEthernet (WSL)", "172.24.160.1", "255.255.240.0"), // virtual, private
            itf("Loopback", "127.0.0.1", "255.0.0.0"),
            itf("Ethernet", "10.0.0.8", "255.255.255.0"),
            itf("Tailscale", "100.101.1.2", "255.192.0.0"),           // CGNAT: not a LAN
            itf("Wi-Fi", "192.168.1.23", "255.255.255.0"),
            itf("Ethernet 3", "169.254.3.3", "255.255.0.0"),          // link-local
        };
        auto lan = rank_for_lan(all);
        CHECK(lan.size() == 3);
        CHECK(lan.size() == 3 && lan[0].name == "Wi-Fi");
        CHECK(lan.size() == 3 && lan[1].name == "Ethernet");
        CHECK(lan.size() == 3 && lan[2].name == "vEthernet (WSL)"); // ranked last, not hidden
    }

    // ── the join code ─────────────────────────────────────────────────────────
    {
        Ipv4 m24 = ip("255.255.255.0"), m16 = ip("255.255.0.0");
        // a /24 home network: three digits, plus the port unless it is the default
        CHECK(encode_join_code(ip("192.168.1.23"), m24, 47801) == "023-47801");
        CHECK(encode_join_code(ip("192.168.1.23"), m24, 47801, 47801) == "023");
        // a /16 needs two octets
        CHECK(encode_join_code(ip("10.20.1.23"), m16, 5000) == "001023-5000");

        // the phone on the same Wi-Fi decodes it with its OWN address
        auto e = decode_join_code("023-47801", ip("192.168.1.77"), m24);
        CHECK(e && e->address.text() == "192.168.1.23" && e->port == 47801);
        auto d = decode_join_code("023", ip("192.168.1.77"), m24, 47801);
        CHECK(d && d->address.text() == "192.168.1.23" && d->port == 47801);
        auto s = decode_join_code(" 023 - 47801 ", ip("192.168.1.77"), m24);
        CHECK(s && s->port == 47801);
        auto w = decode_join_code("001023-5000", ip("10.20.9.9"), m16);
        CHECK(w && w->address.text() == "10.20.1.23");

        // round trip over every host octet on a /24
        for (unsigned last = 1; last < 255; ++last) {
            Ipv4 h{0xC0A80100u | last};
            auto rt = decode_join_code(encode_join_code(h, m24, 40000), ip("192.168.1.7"), m24);
            if (!rt || !(rt->address == h) || rt->port != 40000) {
                CHECK(false && "join code round trip");
                break;
            }
        }

        // refused, not guessed
        CHECK(!decode_join_code("023", ip("192.168.1.77"), m24));             // no port, no default
        CHECK(!decode_join_code("023-0", ip("192.168.1.77"), m24));           // port 0
        CHECK(!decode_join_code("023-70000", ip("192.168.1.77"), m24));       // port > 65535
        CHECK(!decode_join_code("300-5000", ip("192.168.1.77"), m24));        // octet > 255
        CHECK(!decode_join_code("23-5000", ip("192.168.1.77"), m24));         // not whole octets
        CHECK(!decode_join_code("0a3-5000", ip("192.168.1.77"), m24));
        CHECK(!decode_join_code("000-5000", ip("192.168.1.77"), m24));        // network address
        CHECK(!decode_join_code("255-5000", ip("192.168.1.77"), m24));        // broadcast
        CHECK(!decode_join_code("023-5000", ip("10.20.9.9"), m16));           // a /16 joiner cannot
                                                                               // supply the 3rd octet
        CHECK(encode_join_code(ip("10.1.2.3"), ip("128.0.0.0"), 5000).empty()); // /1: not a LAN
        CHECK(encode_join_code(ip("192.168.1.23"), m24, 0).empty());
    }

    // ── when the multicast lock is needed ─────────────────────────────────────
    CHECK(!discovery_needs_lock(false, false)); // two desktops
    CHECK(!discovery_needs_lock(true, false));  // phone ↔ desktop: beacon out, unicast back
    CHECK(!discovery_needs_lock(false, true));
    CHECK(discovery_needs_lock(true, true));    // two phones: neither hears a broadcast

    // ── the real machine: must not crash, and says what it found ─────────────
    {
        auto all = interfaces();
        auto lan = lan_interfaces();
        std::cout << "lan_smoke: " << all.size() << " interface(s), " << lan.size()
                  << " LAN candidate(s)";
        if (!lan.empty()) std::cout << "; best: " << lan[0].name << " " << lan[0].address.text()
                                    << "/" << lan[0].prefix();
        std::cout << "\n";
        for (const auto& i : lan) CHECK(i.kind == Kind::Private);
        // off Android the lock holds nothing, and says why
        MulticastLock lock(nullptr);
        CHECK(!lock.held() && !lock.error().empty());
    }

    if (failures) {
        std::cerr << "lan_smoke: " << failures << " FAILED\n";
        return 1;
    }
    std::cout << "lan_smoke: all ok\n";
    return 0;
}
