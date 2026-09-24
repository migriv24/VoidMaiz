/*
 * voidmaiz/rnslink.hpp — a Void Maiz application on Reticulum (target voidmaiz_net,
 * when built with MAIZ_RETICULUM).
 *
 * The author, 2026-09-23: "let's just use reticulum for everything! all our
 * netoworking needs, with palabra as its void based translation!" This is
 * LanSession's job (voidmaiz/lanlink.hpp) done over Reticulum, through Void
 * Palabra's `voidpalabra_reticulum` (Node). The shape is LanSession's on purpose:
 * the same join flow, the same LanEvent and LanFrame, link names "rns:<peer id>",
 * so an application's frame loop does not change.
 *
 *   DISCOVERY   every session announces its Reticulum destination every few
 *               seconds, carrying what a LAN beacon carries (app, id, name,
 *               colour, net, hosting). Only the same app's announces are heard:
 *               the app is the destination's aspect.
 *   JOINING     the joiner opens a LINK to the host's destination and PROVES its
 *               Reticulum identity on it (a signature, not a claim), then says
 *               hello (id, name). The host's application shows it and a PERSON
 *               presses Allow or Deny. Nothing of the document moves before Allow.
 *   SYNCING     after Allow, each Palabra frame is one message on the link:
 *               a packet when it fits, a Reticulum Resource when it does not.
 *
 * WHAT CHANGED FROM THE LAN SESSION, AND WHY:
 *   - ENCRYPTED. A Reticulum link is sealed and forward-secret; LanSession's
 *     bytes were not.
 *   - "Allowed" is remembered by PROVEN IDENTITY, not by the id a device claims:
 *     a stranger cannot walk back in by claiming an allowed device's id.
 *   - A dead link is found by Reticulum's keepalives (Palabra's link watchdog),
 *     not by an idle timer here.
 *   - No address checks: Reticulum carries no addresses. What it reaches is
 *     decided by the interface: one UDP port, broadcast on the LAN, and every
 *     peer heard also answered by unicast (so a phone that drops broadcast still
 *     hears the desktop).
 *
 * ONE PER PROCESS. Reticulum's transport is process-wide (Void Palabra's
 * okf/concepts/reticulum.md), so the node under this session is too. A session
 * can stop and start again; the node, once started, stays, with its identity.
 *
 * SANS-THREAD: polled once a frame from the application's thread.
 */
#pragma once

#include "voidmaiz/lanlink.hpp" // LanEvent, LanFrame: the same events, on purpose

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace maiz {

struct RnsOptions {
    std::string app;          // only peers running the same app are heard ("interactioncombinators")
    std::string id;           // this device's replica id
    std::string name;         // what the other screen shows
    unsigned rgb = 0x4f86d9;
    bool host = false;        // hosting: accept joiners
    std::string net;          // what this net is called, shown to joiners
    /* Where this device's Reticulum identity (its two keys) is kept. Required:
     * the identity IS the device to its peers, so it must survive a restart. */
    std::string storage_dir;
    // ── the UDP interface ──
    std::uint16_t port = 4242;                    // listen here
    std::string listen_host = "0.0.0.0";
    std::string forward_host = "255.255.255.255"; // the LAN; broadcast when it ends in .255
    std::uint16_t forward_port = 0;               // 0 = `port`
    long long announce_ms = 5000;                 // how often this device says it is here
    long long peer_ttl_ms = 30000;                // a peer unheard this long leaves the list
    int log_level = 1;                            // Reticulum's own log: 0 none .. 5 trace
};

struct RnsPeer {
    std::string id, name, net;
    unsigned rgb = 0;
    bool host = false;
    std::string destination; // what `join` takes
    std::string identity;    // its Reticulum identity hash
    long long seen_ms = 0;
};

struct RnsRequest {
    int token = 0;
    std::string id, name;
    std::string identity; // PROVEN on the link
};

class RnsSession {
  public:
    RnsSession() = default;
    ~RnsSession();
    RnsSession(const RnsSession&) = delete;
    RnsSession& operator=(const RnsSession&) = delete;

    bool start(const RnsOptions& options, std::string* error = nullptr);
    void stop();
    bool running() const { return running_; }
    void set_net(const std::string& net) { opt_.net = net; }
    const std::string& net_name() const { return opt_.net; }
    bool hosting() const { return opt_.host; }

    /* This device, as Reticulum knows it. */
    std::string identity() const;
    std::string destination() const;

    /* A device this host has already let in, by its PROVEN identity: let back
     * in without asking again. */
    bool already_allowed(const std::string& identity) const { return allowed_.count(identity) > 0; }
    /* The identities let in, to keep across runs if the application wants to. */
    const std::set<std::string>& allowed() const { return allowed_; }
    void set_allowed(std::set<std::string> identities) { allowed_ = std::move(identities); }

    /* Other devices heard recently (not this one), hosts first. */
    std::vector<RnsPeer> peers() const;

    // ── hosting ──
    std::vector<RnsRequest> requests() const; // joiners waiting for Allow
    void allow(int token);
    void deny(int token);

    // ── joining ──
    /* Ask to join the host at `destination` (from peers()). While an earlier ask
     * to the same host is still in flight, a second one is a no-op that returns
     * true: a Reticulum handshake can take ~13 s to give up, and an application
     * retrying every few seconds would otherwise stack up links to one host. */
    bool join(const std::string& destination, std::string* error = nullptr);
    /* A join to this host is in flight (asked, no answer and no failure yet). */
    bool joining(const std::string& destination) const { return joining_.count(destination) > 0; }

    // ── frames ──
    void send(const std::string& link, const std::string& frame);
    std::vector<LanFrame> take_frames();
    std::vector<LanEvent> take_events();

    /* Once a frame: the network, announces, links, messages both ways. */
    void poll(long long now_ms);

    int connected() const; // links past Allow

  private:
    struct Conn {
        enum class Phase { Hello, Waiting, Pending, Linked } phase = Phase::Waiting;
        bool joiner = false;
        std::string peer_id, peer_name, identity;
        bool said_hello = false; // host: the joiner's hello arrived
        int token = 0;
        std::string destination; // joiner: the host's
        std::string link() const { return "rns:" + peer_id; }
    };

    RnsOptions opt_;
    bool running_ = false;
    long long last_announce_ = -1;
    int next_token_ = 1;
    std::map<std::string, Conn> conns_;    // by Reticulum link id
    std::map<std::string, RnsPeer> peers_; // by destination
    std::set<std::string> allowed_;        // identities a person let in
    std::map<std::string, long long> joining_; // destination -> when asked (joins in flight)
    long long now_ms_ = 0;                     // the last poll's clock
    std::vector<LanFrame> frames_;
    std::vector<LanEvent> events_;

    std::string about_json() const;
    void on_message(const std::string& link, Conn& c, const std::string& bytes);
    void maybe_request(const std::string& link, Conn& c);
    void admit(const std::string& link, Conn& c, const std::string& how);
    void send_raw(const std::string& link, char tag, const std::string& body);
};

} // namespace maiz
