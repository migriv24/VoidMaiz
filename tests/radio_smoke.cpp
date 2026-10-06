/* radio_smoke.cpp — Reticulum over a radio (voidmaiz/radio.hpp, rnsradio.hpp),
 * in two processes (one Reticulum per process), with no UDP at all.
 *
 * The radio is the desktop stand-in, loopback_radio: "Bluetooth LE" as a local
 * TCP stream throttled to 8 KB/s. The bridge makes the connected peer a Palabra
 * pipe and frames packets with HDLC. So this runs, end to end, everything a
 * phone's Bluetooth path runs except the radio itself: announce heard over the
 * pipe, a link opened and identified, a 30 KB message out (a Resource, with
 * transfers() moving) and echoed back.
 *
 * It also checks HDLC on its own first: frames survive any split of the stream,
 * and the bytes Reticulum treats specially (0x7E, 0x7D) round-trip.
 *
 *   maiz_radio_smoke                         the test (device A, listens)
 *   maiz_radio_smoke child <dir> <port>      device B (connects), run by A
 */
#include "voidmaiz/radio.hpp"
#include "voidmaiz/rnsradio.hpp"

#include "voidpalabra/reticulum.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace rns = voidpalabra::reticulum;
using Clock = std::chrono::steady_clock;

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

static std::string big(std::size_t n) {
    std::string s(n, '\0');
    for (std::size_t i = 0; i < n; ++i) s[i] = (char)((i * 131 + 7) & 0xFF); // every byte value, 0x7E and 0x7D included
    return s;
}

static long long ms(Clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
}

/* One device: a node whose only interfaces are the radio's, announcing, and
 * either asking (B) or echoing (A). Returns 0 when its part is done. */
static int device(bool asker, const std::string& dir, int port, int seconds) {
    maiz::LoopbackRadioOptions ro;
    ro.name = asker ? "B" : "A";
    ro.bytes_per_second = 8000;
    if (asker) ro.connect_port = port;
    else ro.listen_port = port;
    auto radio = maiz::loopback_radio(ro);
    rns::Options o;
    o.storage_dir = dir;
    o.app_name = "maizradio";
    o.aspects = "smoke";
    o.announce_data = ro.name;
    rns::Node& node = rns::Node::instance();
    std::string why;
    if (!node.start(o, &why)) {
        std::printf("RESULT fail start: %s\n", why.c_str());
        return 1;
    }
    radio->start(maiz::RadioKind::Ble, "smoke-tag");
    maiz::RadioBridge bridge(*radio);
    const std::string message = big(30000);
    const auto t0 = Clock::now();
    long long next_announce = 0;
    bool asked = false, got_back = false, saw_transfer = false, saw_tag = false;
    while (ms(t0) < seconds * 1000LL) {
        std::vector<maiz::RadioEvent> rev;
        radio->poll(rev);
        for (const auto& e : rev)
            if (e.type == maiz::RadioEvent::Type::found && e.tag == "smoke-tag") saw_tag = true;
        bridge.handle(rev);
        node.loop();
        bridge.pump();
        if (ms(t0) >= next_announce) {
            node.announce();
            next_announce = ms(t0) + 1500;
        }
        if (!node.transfers().empty()) saw_transfer = true;
        std::vector<rns::Event> ev;
        node.poll(ev);
        for (const auto& e : ev) {
            if (e.type == rns::Event::Type::announce && asker && !asked) {
                node.open(e.destination, &why);
                asked = true;
            } else if (e.type == rns::Event::Type::link_established && asker) {
                node.send(e.link, message, &why);
            } else if (e.type == rns::Event::Type::data) {
                if (asker) {
                    got_back = e.bytes == message;
                } else {
                    node.send(e.link, e.bytes); // echo
                    std::printf("ECHOED %zu\n", e.bytes.size());
                    std::fflush(stdout);
                }
            }
        }
        if (asker && got_back) {
            std::printf("TAG %d\nTRANSFER %d\nRESULT ok\n", saw_tag ? 1 : 0, saw_transfer ? 1 : 0);
            std::fflush(stdout);
            // stay a moment so the echo's proof gets home
            const auto t1 = Clock::now();
            while (ms(t1) < 1500) {
                std::vector<maiz::RadioEvent> r2;
                radio->poll(r2);
                bridge.handle(r2);
                node.loop();
                bridge.pump();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::printf("RESULT %s\n", asker ? "fail no echo came back" : "done");
    return asker ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc >= 4 && std::string(argv[1]) == "child") return device(true, argv[2], std::atoi(argv[3]), 60);

    // ── HDLC on its own ──
    {
        const std::string p1 = big(700), p2 = "\x7e\x7d\x7e", p3 = "plain";
        const std::string stream = rns::Hdlc::frame(p1) + rns::Hdlc::frame(p2) + rns::Hdlc::frame(p3);
        for (std::size_t step : {1, 3, 64, 4096}) {
            rns::Hdlc::Decoder d;
            std::vector<std::string> got;
            for (std::size_t i = 0; i < stream.size(); i += step)
                for (auto& p : d.feed(stream.substr(i, step))) got.push_back(p);
            CHECK(got.size() == 3);
            if (got.size() == 3) {
                CHECK(got[0] == p1);
                CHECK(got[1] == p2);
                CHECK(got[2] == p3);
            }
        }
        CHECK(rns::Hdlc::frame("a\x7e" "b").find('\x7e', 1) == rns::Hdlc::frame("a\x7e" "b").size() - 1);
    }

    // ── two devices over the loopback radio ──
    namespace fs = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() / "maiz_radio_smoke";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    const int port = 47200 + (int)(std::chrono::steady_clock::now().time_since_epoch().count() % 300);
    std::string cmd = "\"" + std::string(argv[0]) + "\" child \"" + (tmp / "b").string() + "\" " + std::to_string(port);
#ifdef _WIN32
    FILE* child = popen(("\"" + cmd + "\"").c_str(), "r"); // cmd.exe strips one pair of outer quotes
#else
    FILE* child = popen(cmd.c_str(), "r");
#endif
    CHECK(child != nullptr);
    // this process is A: it listens and echoes until B is done (B exits; A times out)
    std::thread a([&] { device(false, (tmp / "a").string(), port, 45); });
    std::string out;
    char buf[256];
    while (child && std::fgets(buf, sizeof buf, child)) out += buf;
    if (child) pclose(child);
    a.join();
    std::cout << out;
    CHECK(out.find("RESULT ok") != std::string::npos);
    CHECK(out.find("TAG 1") != std::string::npos);      // the radio's advertised tag arrived
    CHECK(out.find("TRANSFER 1") != std::string::npos); // a progress bar would have moved
    if (failures) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "radio_smoke: all passed\n";
    return 0;
}
