/* lan.cpp — platform facts for LAN discovery (voidmaiz/lan.hpp). Opens no socket. */
#include "voidmaiz/lan.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#if defined(__ANDROID__)
#include <android/native_activity.h>
#include <jni.h>
#endif

namespace maiz::lan {

// ── addresses ─────────────────────────────────────────────────────────────────

std::string Ipv4::text() const {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%u.%u.%u.%u", (host >> 24) & 255u, (host >> 16) & 255u,
                  (host >> 8) & 255u, host & 255u);
    return buf;
}

std::optional<Ipv4> Ipv4::parse(std::string_view s) {
    std::uint32_t out = 0;
    int parts = 0;
    std::size_t i = 0;
    while (parts < 4) {
        if (i >= s.size() || !std::isdigit((unsigned char)s[i])) return std::nullopt;
        unsigned v = 0;
        int digits = 0;
        while (i < s.size() && std::isdigit((unsigned char)s[i])) {
            v = v * 10 + unsigned(s[i] - '0');
            if (++digits > 3 || v > 255) return std::nullopt;
            ++i;
        }
        out = (out << 8) | v;
        ++parts;
        if (parts < 4) {
            if (i >= s.size() || s[i] != '.') return std::nullopt;
            ++i;
        }
    }
    if (i != s.size()) return std::nullopt;
    return Ipv4{out};
}

Kind classify(Ipv4 a) {
    std::uint32_t h = a.host;
    if ((h >> 24) == 127) return Kind::Loopback;
    if ((h >> 16) == 0xA9FE) return Kind::LinkLocal;
    if ((h >> 24) == 10 || (h >> 20) == 0xAC1 || (h >> 16) == 0xC0A8) return Kind::Private;
    if ((h >> 22) == (0x6440u >> 6)) return Kind::CarrierNat; // 100.64.0.0/10
    return Kind::Public;
}

int Interface::prefix() const {
    int n = 0;
    for (std::uint32_t m = netmask.host; m & 0x80000000u; m <<= 1) ++n;
    return n;
}

bool same_subnet(Ipv4 a, Ipv4 b, Ipv4 mask) {
    return (a.host & mask.host) == (b.host & mask.host);
}

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

/* Names that are, with high probability, not the network a person shares with
 * the phone in their hand. A guess, so it only RANKS — it never hides. */
void guess_from_name(Interface& i) {
    std::string n = lower(i.name);
    for (const char* v : {"vethernet", "virtualbox", "vmware", "vmnet", "hyper-v", "docker",
                          "veth", "br-", "virbr", "tun", "tap", "utun", "wg", "tailscale",
                          "zerotier", "wsl", "loopback", "bluetooth", "npcap"})
        if (contains(n, v)) i.virtual_guess = true;
    for (const char* w : {"wi-fi", "wifi", "wlan", "wireless", "wlp", "ap0", "swlan"})
        if (contains(n, w)) i.wireless_guess = true;
}

void finish(Interface& i) {
    i.kind = classify(i.address);
    guess_from_name(i);
}

} // namespace

#if defined(_WIN32)

std::vector<Interface> interfaces() {
    std::vector<Interface> out;
    ULONG size = 16 * 1024;
    std::vector<unsigned char> buf;
    ULONG rc = ERROR_BUFFER_OVERFLOW;
    for (int tries = 0; tries < 3 && rc == ERROR_BUFFER_OVERFLOW; ++tries) {
        buf.resize(size);
        rc = GetAdaptersAddresses(AF_INET,
                                  GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                      GAA_FLAG_SKIP_DNS_SERVER,
                                  nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()),
                                  &size);
    }
    if (rc != NO_ERROR) return out;
    for (auto* a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()); a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        char name[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, name, sizeof name - 1, nullptr,
                            nullptr);
        for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
            Interface i;
            i.name = name;
            i.up = true;
            auto* sin = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
            i.address.host = ntohl(sin->sin_addr.s_addr);
            int p = std::clamp<int>(u->OnLinkPrefixLength, 0, 32);
            i.netmask.host = p == 0 ? 0u : ~0u << (32 - p);
            finish(i);
            if (a->IfType == IF_TYPE_IEEE80211) i.wireless_guess = true;
            if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) i.kind = Kind::Loopback;
            out.push_back(std::move(i));
        }
    }
    return out;
}

#else

std::vector<Interface> interfaces() {
    std::vector<Interface> out;
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return out; // Android: API 24+, and our floor is 26
    for (ifaddrs* a = list; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
        if (!(a->ifa_flags & IFF_UP)) continue;
        Interface i;
        i.name = a->ifa_name ? a->ifa_name : "";
        i.up = true;
        i.address.host = ntohl(reinterpret_cast<sockaddr_in*>(a->ifa_addr)->sin_addr.s_addr);
        if (a->ifa_netmask)
            i.netmask.host =
                ntohl(reinterpret_cast<sockaddr_in*>(a->ifa_netmask)->sin_addr.s_addr);
        finish(i);
        if (a->ifa_flags & IFF_LOOPBACK) i.kind = Kind::Loopback;
        out.push_back(std::move(i));
    }
    freeifaddrs(list);
    return out;
}

#endif

std::vector<Interface> rank_for_lan(std::vector<Interface> all) {
    all.erase(std::remove_if(all.begin(), all.end(),
                             [](const Interface& i) {
                                 return !i.up || i.kind != Kind::Private || i.prefix() < 8 ||
                                        i.prefix() > 30;
                             }),
              all.end());
    std::stable_sort(all.begin(), all.end(), [](const Interface& a, const Interface& b) {
        if (a.virtual_guess != b.virtual_guess) return !a.virtual_guess;
        if (a.wireless_guess != b.wireless_guess) return a.wireless_guess;
        return false;
    });
    return all;
}

std::vector<Interface> lan_interfaces() { return rank_for_lan(interfaces()); }

// ── the join code ─────────────────────────────────────────────────────────────

namespace {

/* How many trailing octets the joiner cannot infer: the host part, rounded up
 * to whole octets. 1 for /24../30, 2 for /16../23, 3 for /8../15. */
int host_octets(int prefix) {
    if (prefix >= 24) return 1;
    if (prefix >= 16) return 2;
    if (prefix >= 8) return 3;
    return 0;
}

int prefix_of(Ipv4 mask) {
    Interface i;
    i.netmask = mask;
    return i.prefix();
}

} // namespace

std::string encode_join_code(Ipv4 host, Ipv4 netmask, std::uint16_t port,
                             std::uint16_t default_port) {
    int n = host_octets(prefix_of(netmask));
    if (n == 0 || port == 0) return {};
    std::string code;
    for (int k = n - 1; k >= 0; --k) {
        char oct[4];
        std::snprintf(oct, sizeof oct, "%03u", (host.host >> (8 * k)) & 255u);
        code += oct;
    }
    if (port != default_port) code += "-" + std::to_string(port);
    return code;
}

std::optional<Endpoint> decode_join_code(std::string_view code, Ipv4 joiner, Ipv4 netmask,
                                         std::uint16_t default_port) {
    // keypads and people add spaces; tolerate those and nothing else
    std::string c;
    for (char ch : code)
        if (ch != ' ') c += ch;
    std::string_view octs = c, port_text;
    if (auto dash = c.find('-'); dash != std::string::npos) {
        octs = std::string_view(c).substr(0, dash);
        port_text = std::string_view(c).substr(dash + 1);
    }
    if (octs.empty() || octs.size() % 3 != 0 || octs.size() > 9) return std::nullopt;
    for (char ch : octs)
        if (!std::isdigit((unsigned char)ch)) return std::nullopt;

    int n = (int)octs.size() / 3;
    // the joiner must be able to supply every octet the code leaves out
    if (n < host_octets(prefix_of(netmask))) return std::nullopt;

    std::uint32_t tail = 0;
    for (int k = 0; k < n; ++k) {
        unsigned v = unsigned(octs[3 * k] - '0') * 100 + unsigned(octs[3 * k + 1] - '0') * 10 +
                     unsigned(octs[3 * k + 2] - '0');
        if (v > 255) return std::nullopt;
        tail = (tail << 8) | v;
    }
    std::uint32_t keep = n >= 4 ? 0u : ~0u << (8 * n);
    Endpoint e;
    e.address.host = (joiner.host & keep) | tail;

    // the host part may not be the network or broadcast address of the subnet
    // the code implies
    std::uint32_t host_mask = ~keep;
    if ((e.address.host & host_mask) == 0 || (e.address.host & host_mask) == host_mask)
        return std::nullopt;

    if (port_text.empty()) {
        if (!default_port) return std::nullopt;
        e.port = default_port;
    } else {
        if (port_text.size() > 5) return std::nullopt;
        unsigned p = 0;
        for (char ch : port_text) {
            if (!std::isdigit((unsigned char)ch)) return std::nullopt;
            p = p * 10 + unsigned(ch - '0');
        }
        if (p == 0 || p > 65535) return std::nullopt;
        e.port = (std::uint16_t)p;
    }
    return e;
}

// ── discovery on Android ──────────────────────────────────────────────────────

bool discovery_needs_lock(bool self_is_android, bool peer_is_android) {
    return self_is_android && peer_is_android;
}

#if defined(__ANDROID__)

namespace {

/* One JNI env for this thread, attaching if needed; detaches on scope exit
 * only if it attached. */
struct Env {
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;
    explicit Env(ANativeActivity* a) : vm(a ? a->vm : nullptr) {
        if (!vm) return;
        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
            else env = nullptr;
        }
    }
    ~Env() {
        if (attached) vm->DetachCurrentThread();
    }
    /* A pending Java exception (a SecurityException for a missing permission)
     * becomes an error string and is cleared: it must never unwind into C++. */
    bool failed(std::string& error, const char* what) {
        if (!env->ExceptionCheck()) return false;
        env->ExceptionClear();
        error = std::string(what) +
                " threw (is android.permission.CHANGE_WIFI_MULTICAST_STATE in the manifest?)";
        return true;
    }
};

} // namespace

MulticastLock::MulticastLock(ANativeActivity* activity, const char* tag) : activity_(activity) {
    Env e(activity);
    if (!e.env) {
        error_ = "no JNI environment for this thread";
        return;
    }
    JNIEnv* env = e.env;
    jobject act = activity->clazz; // the NativeActivity instance (a Context)
    jclass act_cls = env->GetObjectClass(act);
    jmethodID get_app = env->GetMethodID(act_cls, "getApplicationContext", "()Landroid/content/Context;");
    if (e.failed(error_, "getApplicationContext")) return;
    jobject app = env->CallObjectMethod(act, get_app);
    if (e.failed(error_, "getApplicationContext") || !app) return;

    // the APPLICATION context: a WifiManager from an Activity context leaks on older Androids
    jclass app_cls = env->GetObjectClass(app);
    jmethodID get_svc =
        env->GetMethodID(app_cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring wifi = env->NewStringUTF("wifi");
    jobject wm = env->CallObjectMethod(app, get_svc, wifi);
    env->DeleteLocalRef(wifi);
    if (e.failed(error_, "getSystemService(\"wifi\")") || !wm) {
        if (error_.empty()) error_ = "no WifiManager on this device";
        return;
    }

    jclass wm_cls = env->GetObjectClass(wm);
    jmethodID create = env->GetMethodID(wm_cls, "createMulticastLock",
                                        "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;");
    jstring jtag = env->NewStringUTF(tag ? tag : "voidmaiz-lan");
    jobject lock = env->CallObjectMethod(wm, create, jtag);
    env->DeleteLocalRef(jtag);
    if (e.failed(error_, "createMulticastLock") || !lock) return;

    jclass lock_cls = env->GetObjectClass(lock);
    jmethodID counted = env->GetMethodID(lock_cls, "setReferenceCounted", "(Z)V");
    jmethodID acquire = env->GetMethodID(lock_cls, "acquire", "()V");
    env->CallVoidMethod(lock, counted, JNI_FALSE);
    if (e.failed(error_, "setReferenceCounted")) return;
    env->CallVoidMethod(lock, acquire);
    if (e.failed(error_, "MulticastLock.acquire")) return;

    lock_ = env->NewGlobalRef(lock);
    held_ = true;
}

MulticastLock::~MulticastLock() {
    if (!lock_) return;
    Env e(activity_);
    if (e.env) {
        auto lock = static_cast<jobject>(lock_);
        jclass lock_cls = e.env->GetObjectClass(lock);
        jmethodID release = e.env->GetMethodID(lock_cls, "release", "()V");
        e.env->CallVoidMethod(lock, release);
        if (e.env->ExceptionCheck()) e.env->ExceptionClear();
        e.env->DeleteGlobalRef(lock);
    }
}

#else

MulticastLock::MulticastLock(ANativeActivity* activity, const char*) : activity_(activity) {
    error_ = "not Android: nothing to hold (this platform does not filter incoming broadcast)";
}

MulticastLock::~MulticastLock() = default;

#endif

} // namespace maiz::lan
