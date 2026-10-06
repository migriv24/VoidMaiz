/*
 * voidmaiz/rnsradio.hpp — a phone's radios as Reticulum interfaces (target
 * voidmaiz_net, when built with Reticulum: VOIDMAIZ_RETICULUM).
 *
 * voidmaiz/radio.hpp moves bytes between nearby devices; Void Palabra's Node is
 * Reticulum. This is the seam between them, written once so no application has
 * to know what HDLC is:
 *
 *   BLUETOOTH LE   each connected peer becomes a Palabra PIPE named
 *                  "ble:<peer>". Packets the node sends are HDLC-framed onto
 *                  the peer's byte stream; bytes that arrive are unframed into
 *                  packets for the node. The pipe declares LE's real speed, so
 *                  Reticulum's timeouts wait as long as LE needs.
 *   WI-FI DIRECT   a group, once formed, is ordinary IP: a UDP interface named
 *                  "p2p" on its own port, sending to the group owner (a client)
 *                  or the group's broadcast (the owner), and learning every peer
 *                  that speaks (Palabra's learn_peers). It goes when the group
 *                  dissolves.
 *
 * WHO TALKS TO WHOM is the application's (which peers to connect, what to
 * advertise as the tag): this only carries Reticulum over what is connected.
 * Reticulum then does what it always does across interfaces: announces,
 * links, encryption, large messages as Resources, whichever interface works.
 *
 * Sans-thread: call handle() with the radio's events and pump() after every
 * Node::loop(), from the application's one thread.
 */
#pragma once

#include "voidmaiz/radio.hpp"

#include "voidpalabra/reticulum.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace maiz {

struct RadioBridgeOptions {
    std::uint32_t ble_bitrate = 48000;  // bits/s Reticulum is told LE carries
    std::uint16_t p2p_port = 4243;      // the Wi-Fi Direct group's UDP port
};

class RadioBridge {
  public:
    explicit RadioBridge(RadioPlatform& radio, RadioBridgeOptions options = {}) : radio_(radio), opt_(options) {}

    /* The radio's events, as the application polled them (it also wants them
     * for its own screen: who is near, signal strength). */
    void handle(const std::vector<RadioEvent>& events);
    /* After Node::loop(): what the node wants to send goes to the radio. */
    void pump();
    /* Drop every interface this bridge added (the radio was switched off). */
    void clear();

    std::vector<std::string> pipes() const;
    const std::string& group() const { return group_; } // "" | "owner <ip>" | "client <owner ip>"
    /* Bytes waiting to reach `peer` (framed, not yet accepted by the radio,
     * plus what the radio still holds): a progress bar's "still to go". */
    std::size_t waiting(const std::string& peer) const;

  private:
    struct Pipe {
        voidpalabra::reticulum::Hdlc::Decoder decoder;
        std::string out; // framed, not yet accepted by the radio
    };
    RadioPlatform& radio_;
    RadioBridgeOptions opt_;
    std::map<std::string, Pipe> pipes_; // by peer
    std::string group_;
};

} // namespace maiz
