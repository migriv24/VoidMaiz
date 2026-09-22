/*
 * voidmaiz/lanlink.hpp — a Void Maiz application on a real LAN (target voidmaiz_net).
 *
 * Q36, answered 2026-09-22: the author wanted to test phone-to-desktop
 * collaboration, and nothing yet moved bytes between two devices. This is the
 * piece that does, between maiz::Network (which speaks Void Palabra's sync
 * session in frames) and the sockets in voidmaiz/lan.hpp:
 *
 *   DISCOVERY   every running session beacons once a second on UDP (hosts say
 *               "I am sharing"); a session that hears a beacon answers the sender
 *               by UNICAST, because Android drops incoming broadcast but never
 *               unicast ("beacon out, unicast back", collaborative-canvas E24).
 *   JOINING     the joiner connects by TCP and introduces itself (name, id); the
 *               host's application shows it and a PERSON presses Allow or Deny.
 *               Nothing of the document moves before Allow.
 *   SYNCING     after Allow, frames travel in Palabra's stream envelope
 *               (SPEC 11.9: a u32 length before each frame), read with Palabra's
 *               own StreamReader, never hand-rolled reassembly.
 *
 * SANS-THREAD: polled once a frame from the application's thread, like Network.
 * The application moves frames between the two: `take_frames` -> Network::receive,
 * Network::take_outgoing -> `send`. Link names are "lan:<peer id>".
 *
 * UNENCRYPTED, and said so on screen: a host allows each joiner by hand, only
 * private-LAN (or loopback) addresses are accepted, but the bytes are not sealed.
 * For a network you trust, until sealing lands.
 */
#pragma once

#include "voidmaiz/lan.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace maiz {

struct LanOptions {
    std::string app;         // only peers running the same app are seen ("interactioncombinators")
    std::string id;          // this device's replica id (unique per device)
    std::string name;        // what the other screen shows
    unsigned rgb = 0x4f86d9;
    bool host = false;       // hosting: announce, and accept joiners
    std::uint16_t beacon_port = 47811;
    std::uint16_t tcp_port = 47812; // hosting; 0 = any free port
};

struct LanPeer {
    std::string id, name;
    unsigned rgb = 0;
    bool host = false;
    lan::Ipv4 addr;
    std::uint16_t port = 0;  // a host's TCP port
    long long seen_ms = 0;
};

struct LanRequest {
    int token = 0;
    std::string id, name;
    lan::Ipv4 addr;
};

struct LanEvent {
    enum class Kind { Connected, Disconnected, Denied, Error } kind = Kind::Error;
    std::string link; // "lan:<peer id>" when it is about a link
    std::string text;
};

struct LanFrame {
    std::string link, frame;
};

class LanSession {
  public:
    LanSession();
    ~LanSession();
    LanSession(const LanSession&) = delete;
    LanSession& operator=(const LanSession&) = delete;

    bool start(const LanOptions& options, std::string* error = nullptr);
    void stop();
    bool running() const { return running_; }
    bool hosting() const { return opt_.host; }
    std::uint16_t tcp_port() const;

    /* Other devices heard recently (not this one), hosts first. */
    std::vector<LanPeer> peers() const;

    // ── hosting ──
    std::vector<LanRequest> requests() const; // joiners waiting for Allow
    void allow(int token);
    void deny(int token);

    // ── joining ──
    bool join(lan::Ipv4 addr, std::uint16_t port, std::string* error = nullptr);

    // ── frames ──
    void send(const std::string& link, const std::string& frame);
    std::vector<LanFrame> take_frames();
    std::vector<LanEvent> take_events();

    /* Once a frame: beacons, accepts, connection progress, bytes both ways. */
    void poll(long long now_ms);

    int connected() const; // links past Allow

  private:
    struct Conn;
    LanOptions opt_;
    bool running_ = false;
    lan::Udp udp_;
    lan::TcpListener listener_;
    std::vector<std::unique_ptr<Conn>> conns_;
    std::map<std::string, LanPeer> peers_; // by id
    std::map<std::uint32_t, long long> replied_; // unicast replies sent, by address
    long long last_beacon_ = -1;
    int next_token_ = 1;
    std::vector<LanFrame> frames_;
    std::vector<LanEvent> events_;

    std::string beacon_json(bool reply) const;
    void on_beacon(const lan::Udp::Datagram& d, long long now);
    void pump_conn(Conn& c, long long now);
};

/* A private-LAN or loopback address: the only kind this layer talks to. */
bool lan_address_ok(lan::Ipv4 a);

} // namespace maiz
