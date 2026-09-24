/* rnslink.cpp — a Void Maiz application on Reticulum (voidmaiz/rnslink.hpp). */
#include "voidmaiz/rnslink.hpp"

#include "voidpalabra/reticulum.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace maiz {

namespace {

namespace rns = voidpalabra::reticulum;

// The one-byte tag before every message on a link. Before Allow only J/A/D move;
// after it, only F. A message of any other shape is ignored.
const char kHello = 'J'; // joiner -> host: JSON about the joiner
const char kAllow = 'A'; // host -> joiner: JSON about the host; frames follow
const char kDeny = 'D';  // host -> joiner: and the host closes
const char kFrame = 'F'; // a Palabra frame
const std::size_t kMaxAbout = 1024;
const char kAppName[] = "voidmaiz"; // Reticulum app name; the Maiz app is the aspect

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

/* An aspect is a dotted name part: keep it to what every implementation accepts. */
bool aspect_ok(const std::string& a) {
    if (a.empty() || a.size() > 64) return false;
    for (char c : a)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}

} // namespace

RnsSession::~RnsSession() { stop(); }

std::string RnsSession::identity() const { return rns::Node::instance().identity(); }
std::string RnsSession::destination() const { return rns::Node::instance().destination(); }

std::string RnsSession::about_json() const {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "vr", 1);
    cJSON_AddStringToObject(o, "app", opt_.app.c_str());
    cJSON_AddStringToObject(o, "id", opt_.id.c_str());
    cJSON_AddStringToObject(o, "name", opt_.name.c_str());
    cJSON_AddStringToObject(o, "rgb", hex_of(opt_.rgb).c_str());
    cJSON_AddBoolToObject(o, "host", opt_.host);
    if (!opt_.net.empty()) cJSON_AddStringToObject(o, "net", opt_.net.c_str());
    char* t = cJSON_PrintUnformatted(o);
    std::string s = t ? t : "{}";
    cJSON_free(t);
    cJSON_Delete(o);
    return s;
}

bool RnsSession::start(const RnsOptions& options, std::string* error) {
    stop();
    if (!aspect_ok(options.app)) {
        if (error) *error = "the app name must be 1-64 of a-z, 0-9, '-', '_' (it becomes a Reticulum aspect)";
        return false;
    }
    if (options.id.empty()) {
        if (error) *error = "a session needs this device's id";
        return false;
    }
    opt_ = options;
    rns::Node& node = rns::Node::instance();
    if (node.started()) {
        // one node per process, and its destination names the app it serves
        if (node.destination() != rns::Node::destination_hash(node.public_key(), kAppName, opt_.app)) {
            if (error) *error = "this process already runs Reticulum for another app";
            return false;
        }
    } else {
        if (opt_.storage_dir.empty()) {
            if (error) *error = "a session needs storage_dir: this device's Reticulum identity lives there";
            return false;
        }
        rns::Options o;
        o.storage_dir = opt_.storage_dir;
        o.app_name = kAppName;
        o.aspects = opt_.app;
        o.log_level = opt_.log_level;
        rns::UdpInterface u;
        u.name = "lan";
        u.listen_host = opt_.listen_host;
        u.listen_port = opt_.port;
        u.forward_host = opt_.forward_host;
        u.forward_port = opt_.forward_port ? opt_.forward_port : opt_.port;
        u.broadcast = opt_.forward_host.size() > 4 && opt_.forward_host.compare(opt_.forward_host.size() - 4, 4, ".255") == 0;
        u.learn_peers = true; // beacon out, unicast back (voidmaiz/rnslink.hpp)
        o.udp.push_back(u);
        if (!node.start(o, error)) return false;
    }
    node.set_announce_data(about_json());
    running_ = true;
    last_announce_ = -1;
    return true;
}

void RnsSession::stop() {
    if (!running_) return;
    rns::Node& node = rns::Node::instance();
    for (auto& [link, c] : conns_) node.close(link);
    node.loop(); // let the goodbyes out
    std::vector<rns::Event> ignored;
    node.poll(ignored);
    conns_.clear();
    joining_.clear();
    peers_.clear();
    running_ = false;
}

std::vector<RnsPeer> RnsSession::peers() const {
    std::vector<RnsPeer> out;
    for (const auto& [d, p] : peers_) out.push_back(p);
    std::stable_sort(out.begin(), out.end(), [](const RnsPeer& a, const RnsPeer& b) { return a.host && !b.host; });
    return out;
}

std::vector<RnsRequest> RnsSession::requests() const {
    std::vector<RnsRequest> out;
    for (const auto& [link, c] : conns_)
        if (c.phase == Conn::Phase::Pending) out.push_back({c.token, c.peer_id, c.peer_name, c.identity});
    return out;
}

void RnsSession::send_raw(const std::string& link, char tag, const std::string& body) {
    rns::Node::instance().send(link, std::string(1, tag) + body);
}

void RnsSession::admit(const std::string& link, Conn& c, const std::string& how) {
    c.phase = Conn::Phase::Linked;
    allowed_.insert(c.identity); // by proven identity: a claim of an id is not enough
    send_raw(link, kAllow, about_json());
    events_.push_back({LanEvent::Kind::Connected, c.link(), c.peer_name + how});
}

void RnsSession::allow(int token) {
    for (auto& [link, c] : conns_)
        if (c.phase == Conn::Phase::Pending && c.token == token) {
            admit(link, c, " joined");
            return;
        }
}

void RnsSession::deny(int token) {
    for (auto& [link, c] : conns_)
        if (c.phase == Conn::Phase::Pending && c.token == token) {
            send_raw(link, kDeny, "");
            c.phase = Conn::Phase::Waiting; // closed below, once the answer is out
            rns::Node::instance().loop();
            rns::Node::instance().close(link);
            return;
        }
}

bool RnsSession::join(const std::string& destination, std::string* error) {
    if (!running_) {
        if (error) *error = "the session has not started";
        return false;
    }
    if (joining_.count(destination)) return true; // already asking (see the header)
    for (const auto& [link, c] : conns_)
        if (c.joiner && c.destination == destination) return true; // already linked, or answering
    if (!rns::Node::instance().open(destination, error)) return false;
    joining_[destination] = now_ms_;
    return true;
}

void RnsSession::send(const std::string& link, const std::string& frame) {
    for (auto& [id, c] : conns_)
        if (c.phase == Conn::Phase::Linked && c.link() == link) {
            send_raw(id, kFrame, frame);
            return;
        }
}

std::vector<LanFrame> RnsSession::take_frames() {
    std::vector<LanFrame> f;
    f.swap(frames_);
    return f;
}

std::vector<LanEvent> RnsSession::take_events() {
    std::vector<LanEvent> e;
    e.swap(events_);
    return e;
}

int RnsSession::connected() const {
    int n = 0;
    for (const auto& [link, c] : conns_) n += c.phase == Conn::Phase::Linked;
    return n;
}

/* The host asks a person only when it knows BOTH who the joiner says it is (its
 * hello) and who it provably is (its identity on the link); either may come first. */
void RnsSession::maybe_request(const std::string& link, Conn& c) {
    if (c.joiner || c.phase != Conn::Phase::Waiting || !c.said_hello || c.identity.empty()) return;
    // the same id already linked from ANOTHER identity: two devices cannot share a name on the net
    for (const auto& [other, o] : conns_)
        if (other != link && o.phase == Conn::Phase::Linked && o.peer_id == c.peer_id && o.identity != c.identity) {
            send_raw(link, kDeny, "");
            rns::Node::instance().close(link);
            return;
        }
    if (allowed_.count(c.identity)) {
        admit(link, c, " is back"); // already let in once: no second knock
        return;
    }
    c.phase = Conn::Phase::Pending;
    c.token = next_token_++;
}

void RnsSession::on_message(const std::string& link, Conn& c, const std::string& bytes) {
    if (bytes.empty()) return;
    const char tag = bytes[0];
    const std::string body = bytes.substr(1);
    if (tag == kFrame) {
        if (c.phase == Conn::Phase::Linked) frames_.push_back({c.link(), body});
        return; // a frame before Allow is dropped: nothing moves before a person says yes
    }
    if (tag == kHello && !c.joiner && c.phase == Conn::Phase::Waiting && body.size() <= kMaxAbout) {
        cJSON* o = cJSON_ParseWithLength(body.data(), body.size());
        if (cJSON_IsObject(o) && str_of(o, "app") == opt_.app && !str_of(o, "id").empty()) {
            c.peer_id = str_of(o, "id").substr(0, 128);
            c.peer_name = str_of(o, "name").substr(0, 64);
            c.said_hello = true;
        }
        cJSON_Delete(o);
        maybe_request(link, c);
        return;
    }
    if (c.joiner && c.phase == Conn::Phase::Hello) {
        if (tag == kAllow && body.size() <= kMaxAbout) {
            cJSON* o = cJSON_ParseWithLength(body.data(), body.size());
            if (cJSON_IsObject(o)) {
                if (!str_of(o, "id").empty()) c.peer_id = str_of(o, "id").substr(0, 128);
                if (!str_of(o, "name").empty()) c.peer_name = str_of(o, "name").substr(0, 64);
            }
            cJSON_Delete(o);
            c.phase = Conn::Phase::Linked;
            events_.push_back({LanEvent::Kind::Connected, c.link(), "joined " + c.peer_name});
        } else if (tag == kDeny) {
            events_.push_back({LanEvent::Kind::Denied, "", "the host said no"});
            c.phase = Conn::Phase::Waiting; // so its closing is not reported as an error
            rns::Node::instance().close(link);
        }
    }
}

void RnsSession::poll(long long now_ms) {
    if (!running_) return;
    now_ms_ = now_ms;
    rns::Node& node = rns::Node::instance();
    node.loop();
    if (last_announce_ < 0 || now_ms - last_announce_ >= opt_.announce_ms) {
        node.set_announce_data(about_json()); // the net's name or hosting may have changed
        node.announce();
        last_announce_ = now_ms;
    }
    std::vector<rns::Event> events;
    node.poll(events);
    for (const rns::Event& e : events) {
        switch (e.type) {
        case rns::Event::Type::announce: {
            if (e.bytes.size() > kMaxAbout || e.destination == node.destination()) break;
            cJSON* o = cJSON_ParseWithLength(e.bytes.data(), e.bytes.size());
            if (cJSON_IsObject(o) && str_of(o, "app") == opt_.app && !str_of(o, "id").empty() &&
                str_of(o, "id") != opt_.id) {
                RnsPeer& p = peers_[e.destination];
                p.destination = e.destination;
                p.identity = e.identity;
                p.id = str_of(o, "id").substr(0, 128);
                p.name = str_of(o, "name").substr(0, 64);
                p.net = str_of(o, "net").substr(0, 64);
                p.rgb = rgb_of(str_of(o, "rgb"));
                p.host = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "host"));
                p.seen_ms = now_ms;
            }
            cJSON_Delete(o);
            break;
        }
        case rns::Event::Type::link_established: {
            Conn c;
            if (!e.destination.empty()) { // we opened it: a join
                joining_.erase(e.destination);
                c.joiner = true;
                c.destination = e.destination;
                c.phase = Conn::Phase::Hello;
                auto p = peers_.find(e.destination);
                c.peer_id = p != peers_.end() ? p->second.id : e.destination;
                c.peer_name = p != peers_.end() ? p->second.name : e.destination.substr(0, 8);
                conns_[e.link] = c;
                send_raw(e.link, kHello, about_json());
            } else if (opt_.host) {
                conns_[e.link] = c; // Waiting: for its hello and its proven identity
            } else {
                node.close(e.link); // not hosting: nobody may join us
            }
            break;
        }
        case rns::Event::Type::peer_identified: {
            auto it = conns_.find(e.link);
            if (it == conns_.end() || it->second.joiner) break;
            it->second.identity = e.identity;
            maybe_request(e.link, it->second);
            break;
        }
        case rns::Event::Type::data: {
            auto it = conns_.find(e.link);
            if (it != conns_.end()) on_message(e.link, it->second, e.bytes);
            break;
        }
        case rns::Event::Type::link_closed: {
            if (!e.destination.empty()) joining_.erase(e.destination); // a join that never came up
            auto it = conns_.find(e.link);
            if (it == conns_.end()) break;
            const Conn& c = it->second;
            if (c.phase == Conn::Phase::Linked)
                events_.push_back({LanEvent::Kind::Disconnected, c.link(), c.peer_name + " left (" + e.detail + ")"});
            else if (c.joiner && c.phase == Conn::Phase::Hello)
                events_.push_back({LanEvent::Kind::Error, "", "the host closed the connection before answering (" +
                                                                 e.detail + ")"});
            conns_.erase(it);
            break;
        }
        case rns::Event::Type::error:
            break; // a large message that did not arrive: Palabra re-sends what matters
        }
    }
    // a join with no answer for a minute is over, whatever became of it (a path
    // that never arrived reports nothing): the application may ask again
    for (auto it = joining_.begin(); it != joining_.end();)
        it = now_ms - it->second > 60000 ? joining_.erase(it) : std::next(it);
    for (auto it = peers_.begin(); it != peers_.end();)
        it = now_ms - it->second.seen_ms > opt_.peer_ttl_ms ? peers_.erase(it) : std::next(it);
}

} // namespace maiz
