/* lanlink.cpp — a Void Maiz application on a real LAN (voidmaiz/lanlink.hpp). */
#include "voidmaiz/lanlink.hpp"

#include "voidpalabra/sync.hpp" // stream_frame, StreamReader: SPEC 11.9

#include "cJSON.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace maiz {

namespace {

// the join handshake, before any Palabra frame: 4-byte tags
const char kJoin[] = "VMJ1";   // joiner -> host, then u32 LE length, then JSON
const char kAllow[] = "VMA1";  // host -> joiner: Palabra frames follow
const char kDeny[] = "VMD1";   // host -> joiner: and the host closes
const std::size_t kMaxHello = 4096;

std::string u32le(std::uint32_t n) {
    std::string s(4, '\0');
    for (int i = 0; i < 4; ++i) s[i] = (char)((n >> (8 * i)) & 0xFF);
    return s;
}

std::uint32_t read_u32le(const std::string& s, std::size_t at) {
    std::uint32_t n = 0;
    for (int i = 0; i < 4; ++i) n |= (std::uint32_t)(unsigned char)s[at + i] << (8 * i);
    return n;
}

std::string str_of(const cJSON* o, const char* k) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsString(v) && v->valuestring ? std::string(v->valuestring) : std::string();
}

unsigned rgb_of(const std::string& hex) {
    if (hex.size() != 7 || hex[0] != '#') return 0x4f86d9;
    return (unsigned)std::strtoul(hex.c_str() + 1, nullptr, 16);
}

std::string hex_of(unsigned rgb) {
    char b[8];
    std::snprintf(b, sizeof b, "#%06x", rgb & 0xFFFFFFu);
    return b;
}

} // namespace

bool lan_address_ok(lan::Ipv4 a) {
    lan::Kind k = lan::classify(a);
    return k == lan::Kind::Private || k == lan::Kind::Loopback;
}

/* One TCP connection and where it is in its life. */
struct LanSession::Conn {
    enum class Phase {
        Connecting, // joiner: TCP not up yet
        Hello,      // joiner: introduced itself, waiting for Allow/Deny
        Waiting,    // host: reading the joiner's hello
        Pending,    // host: a person has not answered yet
        Linked,     // Palabra frames flow
    } phase;
    std::unique_ptr<lan::Tcp> tcp = std::make_unique<lan::Tcp>();
    std::string buf;         // handshake bytes not yet consumed
    std::string peer_id, peer_name;
    int token = 0;
    bool joiner = false;
    long long last_rx = 0; // when bytes last arrived: silence is how a dead link shows
    voidpalabra::sync::StreamReader reader;
    std::string link() const { return "lan:" + peer_id; }
};

LanSession::LanSession() = default;
LanSession::~LanSession() { stop(); }

bool LanSession::start(const LanOptions& options, std::string* error) {
    stop();
    opt_ = options;
    if (!udp_.open(opt_.beacon_port, error)) return false;
    if (opt_.host) {
        if (!listener_.open(opt_.tcp_port, nullptr) && !listener_.open(0, error)) {
            udp_.close();
            return false; // neither the usual port nor any port
        }
    }
    running_ = true;
    last_beacon_ = -1;
    return true;
}

void LanSession::stop() {
    for (auto& c : conns_) c->tcp->close();
    conns_.clear();
    listener_.close();
    udp_.close();
    peers_.clear();
    replied_.clear();
    running_ = false;
}

std::uint16_t LanSession::tcp_port() const { return listener_.port(); }

std::string LanSession::beacon_json(bool reply) const {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "vl", 1);
    cJSON_AddStringToObject(o, "app", opt_.app.c_str());
    cJSON_AddStringToObject(o, "id", opt_.id.c_str());
    cJSON_AddStringToObject(o, "name", opt_.name.c_str());
    cJSON_AddStringToObject(o, "rgb", hex_of(opt_.rgb).c_str());
    cJSON_AddBoolToObject(o, "host", opt_.host);
    if (opt_.host) cJSON_AddNumberToObject(o, "port", listener_.port());
    if (!opt_.net.empty()) cJSON_AddStringToObject(o, "net", opt_.net.c_str());
    cJSON_AddBoolToObject(o, "reply", reply);
    char* t = cJSON_PrintUnformatted(o);
    std::string s = t ? t : "{}";
    cJSON_free(t);
    cJSON_Delete(o);
    return s;
}

void LanSession::on_beacon(const lan::Udp::Datagram& d, long long now) {
    if (!lan_address_ok(d.from) || d.bytes.size() > 2048) return;
    cJSON* o = cJSON_ParseWithLength(d.bytes.data(), d.bytes.size());
    if (!cJSON_IsObject(o)) {
        cJSON_Delete(o);
        return;
    }
    std::string app = str_of(o, "app"), id = str_of(o, "id");
    bool reply = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "reply"));
    if (app == opt_.app && !id.empty() && id != opt_.id) {
        LanPeer& p = peers_[id];
        p.id = id;
        p.name = str_of(o, "name").substr(0, 64);
        p.net = str_of(o, "net").substr(0, 64);
        p.rgb = rgb_of(str_of(o, "rgb"));
        p.host = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "host"));
        const cJSON* port = cJSON_GetObjectItemCaseSensitive(o, "port");
        p.port = cJSON_IsNumber(port) ? (std::uint16_t)port->valuedouble : 0;
        p.addr = d.from;
        p.seen_ms = now;
        // beacon out, unicast back: a phone never hears our broadcast, but it
        // hears this (rate-limited per address, and never a reply to a reply)
        if (!reply) {
            long long& last = replied_[d.from.host];
            if (now - last > 800) {
                udp_.send_to(d.from, d.port, beacon_json(true));
                last = now;
            }
        }
    }
    cJSON_Delete(o);
}

std::vector<LanPeer> LanSession::peers() const {
    std::vector<LanPeer> out;
    for (const auto& [id, p] : peers_) out.push_back(p);
    std::stable_sort(out.begin(), out.end(), [](const LanPeer& a, const LanPeer& b) { return a.host > b.host; });
    return out;
}

std::vector<LanRequest> LanSession::requests() const {
    std::vector<LanRequest> out;
    for (const auto& c : conns_)
        if (c->phase == Conn::Phase::Pending) out.push_back({c->token, c->peer_id, c->peer_name, c->tcp->peer()});
    return out;
}

bool LanSession::already_allowed(const std::string& peer_id) const {
    return allowed_.count(peer_id) > 0;
}

void LanSession::allow(int token) {
    for (auto& c : conns_)
        if (c->phase == Conn::Phase::Pending && c->token == token) {
            allowed_.insert(c->peer_id); // let this device back in after a nap
            c->tcp->write(std::string(kAllow, 4));
            c->phase = Conn::Phase::Linked;
            if (!c->buf.empty()) { // a joiner may already have sent frames: keep them
                c->reader.feed(c->buf);
                c->buf.clear();
            }
            events_.push_back({LanEvent::Kind::Connected, c->link(), c->peer_name + " joined"});
        }
}

void LanSession::deny(int token) {
    for (auto& c : conns_)
        if (c->phase == Conn::Phase::Pending && c->token == token) {
            c->tcp->write(std::string(kDeny, 4));
            c->tcp->pump();
            c->tcp->close();
        }
}

bool LanSession::join(lan::Ipv4 addr, std::uint16_t port, std::string* error) {
    if (!running_) {
        if (error) *error = "the LAN session is not running";
        return false;
    }
    if (!lan_address_ok(addr)) {
        if (error) *error = addr.text() + " is not on a private network; this build joins only LAN addresses";
        return false;
    }
    auto c = std::make_unique<Conn>();
    c->joiner = true;
    c->phase = Conn::Phase::Connecting;
    if (!c->tcp->connect(addr, port, error)) return false;
    conns_.push_back(std::move(c));
    return true;
}

void LanSession::send(const std::string& link, const std::string& frame) {
    for (auto& c : conns_)
        if (c->phase == Conn::Phase::Linked && c->link() == link) c->tcp->write(voidpalabra::sync::stream_frame(frame));
}

std::vector<LanFrame> LanSession::take_frames() {
    std::vector<LanFrame> f;
    f.swap(frames_);
    return f;
}

std::vector<LanEvent> LanSession::take_events() {
    std::vector<LanEvent> e;
    e.swap(events_);
    return e;
}

int LanSession::connected() const {
    int n = 0;
    for (const auto& c : conns_) n += c->phase == Conn::Phase::Linked ? 1 : 0;
    return n;
}

void LanSession::pump_conn(Conn& c, long long now) {
    bool was_linked = c.phase == Conn::Phase::Linked;
    if (!c.tcp->pump()) {
        /* Bytes that arrived WITH the close still count. A host's Deny is written
         * and the socket closed at once, so the joiner reads the answer and the
         * close in one pump (found by lanlink_smoke: the Deny was reported as
         * "closed before answering"). Last frames before a peer leaves, too. */
        std::string last = c.tcp->take_read();
        if (c.phase == Conn::Phase::Hello) {
            c.buf += last;
            if (c.buf.size() >= 4 && c.buf.compare(0, 4, kDeny) == 0) {
                events_.push_back({LanEvent::Kind::Denied, "", "the host said no"});
                return;
            }
        } else if (was_linked && !last.empty()) {
            c.reader.feed(last);
            std::string frame;
            while (c.reader.next(frame)) frames_.push_back({c.link(), frame});
        }
        if (was_linked) events_.push_back({LanEvent::Kind::Disconnected, c.link(), c.peer_name + " left (" + c.tcp->error() + ")"});
        else if (c.joiner && c.phase != Conn::Phase::Hello)
            events_.push_back({LanEvent::Kind::Error, "", "could not reach the host: " + c.tcp->error()});
        else if (c.joiner)
            events_.push_back({LanEvent::Kind::Error, "", "the host closed the connection before answering"});
        return;
    }
    if (c.phase == Conn::Phase::Connecting && c.tcp->connected()) {
        // introduce ourselves; a person on the other side decides
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "app", opt_.app.c_str());
        cJSON_AddStringToObject(o, "id", opt_.id.c_str());
        cJSON_AddStringToObject(o, "name", opt_.name.c_str());
        char* t = cJSON_PrintUnformatted(o);
        std::string hello = t ? t : "{}";
        cJSON_free(t);
        cJSON_Delete(o);
        c.tcp->write(std::string(kJoin, 4) + u32le((std::uint32_t)hello.size()) + hello);
        c.phase = Conn::Phase::Hello;
    }
    std::string in = c.tcp->take_read();
    if (!in.empty()) c.last_rx = now;
    if (in.empty() && c.phase != Conn::Phase::Linked) return;
    switch (c.phase) {
    case Conn::Phase::Connecting: break;
    case Conn::Phase::Hello: // joiner, waiting for the host's answer
        c.buf += in;
        if (c.buf.size() < 4) return;
        if (c.buf.compare(0, 4, kAllow) == 0) {
            c.phase = Conn::Phase::Linked;
            c.peer_id = "host"; // replaced below by the session's own identity; the link needs a name now
            // the link is named after the host we chose; find it among the peers
            for (const auto& [id, p] : peers_)
                if (p.addr == c.tcp->peer()) c.peer_id = id;
            if (c.peer_id == "host") c.peer_id = "host-" + c.tcp->peer().text();
            c.peer_name = peers_.count(c.peer_id) ? peers_[c.peer_id].name : c.tcp->peer().text();
            c.reader.feed(c.buf.substr(4));
            c.buf.clear();
            events_.push_back({LanEvent::Kind::Connected, c.link(), "joined " + c.peer_name});
        } else if (c.buf.compare(0, 4, kDeny) == 0) {
            events_.push_back({LanEvent::Kind::Denied, "", "the host said no"});
            c.tcp->close();
            return;
        } else {
            events_.push_back({LanEvent::Kind::Error, "", "the host answered with something that is not this app"});
            c.tcp->close();
            return;
        }
        break;
    case Conn::Phase::Waiting: { // host, reading the hello
        c.buf += in;
        if (c.buf.size() < 8) return;
        if (c.buf.compare(0, 4, kJoin) != 0) {
            c.tcp->close(); // not one of ours
            return;
        }
        std::uint32_t n = read_u32le(c.buf, 4);
        if (n > kMaxHello) {
            c.tcp->close();
            return;
        }
        if (c.buf.size() < 8 + n) return;
        cJSON* o = cJSON_ParseWithLength(c.buf.data() + 8, n);
        std::string app = str_of(o, "app");
        c.peer_id = str_of(o, "id");
        c.peer_name = str_of(o, "name").substr(0, 64);
        cJSON_Delete(o);
        c.buf.erase(0, 8 + n);
        if (app != opt_.app || c.peer_id.empty()) {
            c.tcp->write(std::string(kDeny, 4));
            c.tcp->pump();
            c.tcp->close();
            return;
        }
        if (allowed_.count(c.peer_id)) { // already let in once: no second knock
            c.tcp->write(std::string(kAllow, 4));
            c.phase = Conn::Phase::Linked;
            if (!c.buf.empty()) {
                c.reader.feed(c.buf);
                c.buf.clear();
            }
            events_.push_back({LanEvent::Kind::Connected, c.link(), c.peer_name + " is back"});
            break;
        }
        c.token = next_token_++;
        c.phase = Conn::Phase::Pending; // a person answers (allow / deny)
        break;
    }
    case Conn::Phase::Pending: // the joiner waits for Allow; keep anything it sent
        c.buf += in;
        break;
    case Conn::Phase::Linked: {
        if (!in.empty()) c.reader.feed(in);
        std::string frame;
        while (c.reader.next(frame)) frames_.push_back({c.link(), frame});
        if (c.reader.broken()) {
            events_.push_back({LanEvent::Kind::Disconnected, c.link(), "the stream broke: " + c.reader.why()});
            c.tcp->close();
        }
        break;
    }
    }
}

void LanSession::poll(long long now) {
    if (!running_) return;
    // beacons: once a second, and everything heard
    if (last_beacon_ < 0 || now - last_beacon_ >= 1000) {
        udp_.broadcast(opt_.beacon_port, beacon_json(false));
        last_beacon_ = now;
    }
    for (const auto& d : udp_.receive()) on_beacon(d, now);
    for (auto it = peers_.begin(); it != peers_.end();) // forget the quiet
        it = now - it->second.seen_ms > 5000 ? peers_.erase(it) : std::next(it);

    // hosting: new joiners, private addresses only
    if (opt_.host)
        while (auto t = listener_.accept()) {
            if (!lan_address_ok(t->peer())) continue; // closed as it goes out of scope
            auto c = std::make_unique<Conn>();
            c->phase = Conn::Phase::Waiting;
            c->tcp = std::move(t);
            conns_.push_back(std::move(c));
        }
    for (auto& c : conns_) {
        if (c->last_rx == 0) c->last_rx = now; // born now: the clock starts here
        pump_conn(*c, now);
        // a link with nothing on it is dead, whatever the socket says: the sync
        // session speaks every few seconds, so silence means gone
        if (c->phase == Conn::Phase::Linked && opt_.idle_ms > 0 && now - c->last_rx > opt_.idle_ms) {
            events_.push_back({LanEvent::Kind::Disconnected, c->link(),
                               c->peer_name + " went quiet (no data for " +
                                   std::to_string(opt_.idle_ms / 1000) + "s)"});
            c->tcp->close();
        }
    }
    conns_.erase(std::remove_if(conns_.begin(), conns_.end(), [](const std::unique_ptr<Conn>& c) { return c->tcp->closed(); }),
                 conns_.end());
}

} // namespace maiz
