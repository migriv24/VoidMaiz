/*
 * voidmaiz/lan.hpp — what a device needs to know to be found on a LAN (UI-free).
 *
 * okf/concepts/collaborative-canvas.md §8 (E24–E30) and the author's call
 * (2026-09-20): "we need to build some networking abilities for android. if it
 * doesn't exist, it SHOULD."
 *
 * THE LINE, restated for this header. Void Palabra owns the transport: sockets,
 * frames, keys, the sync session (Q30). This header opens NO socket. It holds
 * the platform facts every Void Maiz application needs BEFORE a transport can do
 * anything, and that every platform shell would otherwise rediscover:
 *
 *   1. Which addresses this device has, which one is probably the LAN, and which
 *      are virtual adapters that would swallow a broadcast (E29).
 *   2. A JOIN CODE: a host's address and port as a few digits, relative to the
 *      joiner's own network, so a phone can join by typing six digits on an
 *      ImGui keypad — no QR camera, no platform keyboard (both are Java; Q29).
 *   3. On Android, the one platform call LAN discovery cannot do without: a
 *      Wi-Fi MULTICAST LOCK. Android's Wi-Fi stack drops incoming broadcast and
 *      multicast unless an app holds one, and the lock is a Java API. It is
 *      reached here through JNI from C++ — no Java source, but it is a call into
 *      the Java runtime, which is exactly Q29's option (a). It is therefore
 *      OPT-IN; see `discovery_needs_lock` for when an application needs it.
 *
 * WHY DISCOVERY CAN USUALLY SKIP THE LOCK. The filter drops what a phone
 * RECEIVES by broadcast. It does not stop a phone SENDING a broadcast, and it
 * never touches unicast. So if every device beacons, and a device that hears a
 * beacon answers the sender by UNICAST, a desktop hears the phone and the phone
 * hears the desktop's answer — with no lock. Only two phones (neither of which
 * hears the other's beacon) need the lock, or a join code.
 *
 * Its own target, `voidmaiz_lan`, so an application that never networks links
 * no socket library (Windows: iphlpapi and ws2_32).
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ANativeActivity; // <android/native_activity.h>; only used on Android

namespace maiz::lan {

/* ── addresses ────────────────────────────────────────────────────────────── */

struct Ipv4 {
    std::uint32_t host = 0; // host byte order: 192.168.1.23 = 0xC0A80117
    std::string text() const;
    static std::optional<Ipv4> parse(std::string_view dotted);
    bool operator==(const Ipv4&) const = default;
};

enum class Kind {
    Loopback,  // 127/8
    LinkLocal, // 169.254/16: no DHCP answered; almost never the network you want
    Private,   // 10/8, 172.16/12, 192.168/16: a LAN
    CarrierNat,// 100.64/10: mobile data, some VPNs (Tailscale lives here)
    Public,
};
Kind classify(Ipv4 a);

struct Interface {
    std::string name;   // "wlan0", "en0", "Wi-Fi", "vEthernet (WSL)"
    Ipv4 address, netmask;
    Ipv4 broadcast() const { return {address.host | ~netmask.host}; }
    int prefix() const; // 24 for 255.255.255.0
    Kind kind = Kind::Public;
    bool up = false;
    bool virtual_guess = false; // a name that looks like a VM/VPN/container adapter
    bool wireless_guess = false;
};

/* Every IPv4 interface that is up. Never throws; an empty list on failure. */
std::vector<Interface> interfaces();

/* The interfaces a LAN beacon should go out on, best first: up, private, not
 * loopback/link-local, real before virtual, wireless before wired (a phone and a
 * laptop meet on Wi-Fi). Bind and broadcast PER INTERFACE, not on 0.0.0.0 alone:
 * a desktop with a VPN or WSL adapter otherwise sends its beacon into the
 * adapter and nobody on the Wi-Fi hears it (E29). */
std::vector<Interface> lan_interfaces();
std::vector<Interface> rank_for_lan(std::vector<Interface> all); // the pure half, testable

/* Are two addresses on one subnet under this mask? */
bool same_subnet(Ipv4 a, Ipv4 b, Ipv4 mask);

/* ── the join code ────────────────────────────────────────────────────────────
 * The host shows it; the joiner types it. It carries only the host-part octets
 * the joiner cannot infer (1 on a /24, 2 on a /16), plus a port, as digits:
 *
 *     /24, port 47801  →  "023-47801"   (host octet 023; the joiner supplies 192.168.1)
 *     /16              →  "001023-47801"
 *
 * The port may be omitted when it is the application's default ("023"), which
 * is the six-or-fewer digits a keypad wants. Decoding needs the JOINER's own
 * address and mask, and fails (rather than guessing) when the joiner is on a
 * network the code cannot describe. It is not a secret and not a credential —
 * the pairing that follows (the transport's short authentication string) is
 * what proves who you reached. */
struct Endpoint {
    Ipv4 address;
    std::uint16_t port = 0;
    bool operator==(const Endpoint&) const = default;
};
std::string encode_join_code(Ipv4 host, Ipv4 netmask, std::uint16_t port,
                             std::uint16_t default_port = 0);
std::optional<Endpoint> decode_join_code(std::string_view code, Ipv4 joiner, Ipv4 netmask,
                                         std::uint16_t default_port = 0);

/* ── discovery on Android ─────────────────────────────────────────────────── */

/* Does this device need the multicast lock to be DISCOVERED BY and to DISCOVER
 * peers of this kind? Only when both ends drop incoming broadcast, i.e. two
 * Android devices. A phone and a desktop meet without it (see the header). */
bool discovery_needs_lock(bool self_is_android, bool peer_is_android);

/* RAII Wi-Fi multicast lock. On Android: WifiManager.createMulticastLock(tag),
 * not reference-counted, acquired in the constructor and released in the
 * destructor. Requires the manifest permission CHANGE_WIFI_MULTICAST_STATE; a
 * missing permission is reported in `error()`, never thrown. Everywhere else it
 * holds nothing and says so. Construct it on a thread the JavaVM can attach.
 * Hold it only while discovering: it costs battery. */
class MulticastLock {
  public:
    explicit MulticastLock(ANativeActivity* activity, const char* tag = "voidmaiz-lan");
    ~MulticastLock();
    MulticastLock(const MulticastLock&) = delete;
    MulticastLock& operator=(const MulticastLock&) = delete;

    bool held() const { return held_; }
    const std::string& error() const { return error_; }

  private:
    ANativeActivity* activity_ = nullptr;
    void* lock_ = nullptr; // a JNI global reference on Android
    bool held_ = false;
    std::string error_;
};

/* ── sockets (Q36, answered 2026-09-22) ──────────────────────────────────────
 * The author, wanting to test phone-to-desktop collaboration: the LAN socket
 * layer belongs here, beside the platform facts. Void Palabra owns the protocol
 * (frames, the stream envelope, the session); these are the pipes. Everything is
 * NON-BLOCKING and polled from the frame loop: a frame never waits on the network.
 *
 * UNENCRYPTED. A host allows each joiner by hand, and only private LAN addresses
 * are accepted, but the bytes are not sealed: this is for a network you trust.
 * Sealing (Hormiga's X25519 room key, libsodium on Android) is the next step. */

/* One datagram socket bound to a port, broadcast-capable. */
class Udp {
  public:
    Udp() = default;
    ~Udp();
    Udp(const Udp&) = delete;
    Udp& operator=(const Udp&) = delete;

    /* Bind to `port` on every interface (address reuse on, so two copies on one
     * machine can both listen). False with a reason on failure. */
    bool open(std::uint16_t port, std::string* error = nullptr);
    void close();
    bool is_open() const { return sock_ != invalid_; }

    bool send_to(Ipv4 to, std::uint16_t port, const std::string& bytes);
    /* To 255.255.255.255 AND each LAN interface's own broadcast address: a
     * desktop with a VPN adapter otherwise sends only into the adapter. */
    void broadcast(std::uint16_t port, const std::string& bytes);

    struct Datagram {
        Ipv4 from;
        std::uint16_t port = 0;
        std::string bytes;
    };
    /* Every datagram waiting right now; never blocks. */
    std::vector<Datagram> receive();

  private:
    static constexpr long long invalid_ = -1;
    long long sock_ = invalid_;
};

/* One TCP connection, non-blocking. `write` queues; `pump` moves bytes both ways
 * and must be called every frame. */
class Tcp {
  public:
    Tcp() = default;
    ~Tcp();
    Tcp(const Tcp&) = delete;
    Tcp& operator=(const Tcp&) = delete;

    /* Start connecting; true if the attempt began (it completes during pump). */
    bool connect(Ipv4 to, std::uint16_t port, std::string* error = nullptr);
    void adopt(long long accepted_socket, Ipv4 peer, std::uint16_t peer_port); // from a Listener

    /* Moves queued bytes out and available bytes in. Returns false once the
     * connection is closed or failed (see error()). */
    bool pump();
    void write(const std::string& bytes);
    std::string take_read(); // everything read since the last call

    bool connected() const { return state_ == State::Connected; }
    bool closed() const { return state_ == State::Closed; }
    const std::string& error() const { return error_; }
    Ipv4 peer() const { return peer_; }
    void close();

  private:
    enum class State { Idle, Connecting, Connected, Closed } state_ = State::Idle;
    long long sock_ = -1;
    Ipv4 peer_;
    std::uint16_t peer_port_ = 0;
    std::string out_, in_, error_;
};

class TcpListener {
  public:
    TcpListener() = default;
    ~TcpListener();
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    /* Listen on `port` (0 = let the system choose). */
    bool open(std::uint16_t port, std::string* error = nullptr);
    void close();
    std::uint16_t port() const { return port_; }
    /* A waiting connection, or nullptr; never blocks. */
    std::unique_ptr<Tcp> accept();

  private:
    long long sock_ = -1;
    std::uint16_t port_ = 0;
};

/* The Android manifest lines a LAN-capable Void Maiz application needs. All
 * four are "normal" permissions: granted at install, no runtime prompt, and no
 * Java code — so a `hasCode=false` NativeActivity APK keeps that property. The
 * InteractionCombinators APK shipped with none of them (measured 2026-09-20). */
inline constexpr const char* android_manifest_permissions[] = {
    "android.permission.INTERNET",
    "android.permission.ACCESS_NETWORK_STATE",
    "android.permission.ACCESS_WIFI_STATE",
    "android.permission.CHANGE_WIFI_MULTICAST_STATE",
};

} // namespace maiz::lan
