/*
 * voidmaiz/radio.hpp — the radios a phone has and a LAN does not: Bluetooth LE
 * and Wi-Fi Direct. The radio holiday.
 *
 * Void Hormiga's author, 2026-10-05: phones in the field must share a database
 * with no Wi-Fi network in common, and not through a hotspot ("a big waste of
 * data plans and money ... not everyone can easily do hotspot"). So: Wi-Fi
 * Direct between Android phones (fast, Android only), and Bluetooth LE
 * ("substantially slower ... however, it would be compatible with an iOS
 * device and android device in the future"). LE rather than classic Bluetooth
 * for exactly that reason: an iPhone may speak LE GATT to anything, while a
 * classic RFCOMM socket needs Apple's accessory program.
 *
 * WHAT THIS IS: bytes between two nearby devices, nothing more. It knows no
 * Reticulum and no Hormiga. voidmaiz/rnsradio.hpp turns a radio into Reticulum
 * interfaces (a pipe per LE peer, a UDP interface on a Wi-Fi Direct group), and
 * the application decides who it is willing to talk to.
 *
 *   BLUETOOTH LE   every device both ADVERTISES (a service of ours, carrying a
 *                  short `tag` the application chooses, so devices of the same
 *                  group find each other and others are ignored) and SCANS for
 *                  it. A peer found can be connected; a connection is one byte
 *                  STREAM both ways (a GATT characteristic each way, chunked to
 *                  the negotiated MTU, written one at a time so nothing is
 *                  dropped). Expect a few kilobytes a second.
 *   WI-FI DIRECT   devices advertise the same service by DNS-SD on Wi-Fi
 *                  Direct; connecting forms a GROUP (one phone is its owner,
 *                  usually at 192.168.49.1) and the system may ask the other
 *                  person to accept. Then both have an IP address on that group
 *                  and ordinary UDP works between them. This holiday reports the
 *                  group; it does not carry bytes for it. Megabytes a second.
 *
 * PERMISSIONS are the system's: Android 12+ asks for "Nearby devices"
 * (BLUETOOTH_SCAN / CONNECT / ADVERTISE; NEARBY_WIFI_DEVICES on 13+), older
 * Android for location. `request()` is the only call that can show a prompt,
 * and like the location holiday a host calls it from a person's tap.
 *
 * ONE PER DEVICE, installed by the shell (like location.hpp); polled once a
 * frame from the application's thread. ImGui-free; the Android half is
 * src/input/radio.cpp and MaizActivity.java (maizRadio*). `LoopbackRadio` is a
 * desktop stand-in: a "Bluetooth" that is a local TCP connection, throttled to
 * a chosen speed, so two processes on one computer can exercise everything
 * above the radio, progress bars included, with no phone in hand.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct ANativeActivity;

namespace maiz {

enum class RadioKind { Ble = 0, WifiDirect = 1 };

enum class RadioAccess {
    Unasked,     // nobody asked; request() shows the system's prompt
    Asking,      // the prompt is up
    Ready,       // granted, and the radio is on
    Off,         // granted, but the radio is switched off (request() asks to turn it on)
    Denied,      // refused; the system will not ask again
    Unavailable, // this device has no such radio
};
const char* radio_access_name(RadioAccess a);

struct RadioPeer {
    std::string id;   // the platform's handle (a Bluetooth address, a Wi-Fi Direct device address)
    std::string name; // what the device calls itself
    std::string tag;  // what it advertised (the application's group tag), "" if unknown
    RadioKind kind = RadioKind::Ble;
    int rssi = 0;     // signal, dBm (LE only; 0 = unknown)
    bool connected = false;
};

struct RadioEvent {
    enum class Type {
        found,        // `peer` advertising our service (with `tag`)
        lost,         // not heard for a while
        connected,    // a stream to `peer` is open (LE), either side may have started it
        disconnected, // `peer` gone; `detail` says why
        data,         // `bytes` arrived from `peer` (LE: any split of the stream)
        group,        // Wi-Fi Direct: a group formed; `detail` = "owner <ip>" | "client <owner ip>"
        group_gone,   // Wi-Fi Direct: the group dissolved
        error,        // `detail`
    };
    Type type = Type::error;
    RadioKind kind = RadioKind::Ble;
    std::string peer, name, tag, bytes, detail;
    int rssi = 0;
};

class RadioPlatform {
  public:
    virtual ~RadioPlatform() = default;
    virtual RadioAccess access(RadioKind kind) = 0;
    /* The permission prompt, and turning the radio on: call from a person's tap. */
    virtual bool request(RadioKind kind) = 0;
    /* Advertise `tag` and look for others advertising (any tag: the application
     * filters). LE: advertise + scan. Wi-Fi Direct: DNS-SD service + discovery. */
    virtual bool start(RadioKind kind, const std::string& tag) = 0;
    virtual void stop(RadioKind kind) = 0;
    virtual bool running(RadioKind kind) const = 0;
    /* LE: open a stream to `peer`. Wi-Fi Direct: ask to form a group with it. */
    virtual bool connect(RadioKind kind, const std::string& peer) = 0;
    virtual void disconnect(RadioKind kind, const std::string& peer) = 0;
    /* LE: queue bytes on the stream to `peer`. False if it is not connected or
     * the queue is full (the caller tries again; nothing was queued). */
    virtual bool send(const std::string& peer, const std::string& bytes) = 0;
    /* Bytes queued to `peer` and not yet on the air: what a progress bar and a
     * sender's patience both need. */
    virtual std::size_t backlog(const std::string& peer) const = 0;
    virtual void poll(std::vector<RadioEvent>& out) = 0;
};

/* The process-wide radio (null until a shell installs one). */
void install_radio(std::unique_ptr<RadioPlatform> platform);
RadioPlatform* radio();

/* Android: Bluetooth LE and Wi-Fi Direct through MaizActivity. Null off Android. */
std::unique_ptr<RadioPlatform> android_radio(ANativeActivity* activity);

/* The desktop stand-in: "Bluetooth LE" as a local TCP stream. One side listens
 * on `port`, the other connects to it (both may do both: give two ports).
 * `bytes_per_second` throttles what is written (0 = unthrottled), so a test
 * sees a Bluetooth-slow link. It finds the other side as soon as the TCP
 * connection is up, under the name and tag the other side sent first. No
 * Wi-Fi Direct. Threads of its own; poll() hands their results over. */
struct LoopbackRadioOptions {
    int listen_port = 0;  // 0 = do not listen
    int connect_port = 0; // 0 = do not connect
    std::string name = "desktop";
    double bytes_per_second = 6000;
};
std::unique_ptr<RadioPlatform> loopback_radio(const LoopbackRadioOptions& options);

} // namespace maiz
