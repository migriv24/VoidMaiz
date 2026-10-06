/* src/net/rnsradio.cpp — see voidmaiz/rnsradio.hpp. */
#include "voidmaiz/rnsradio.hpp"

namespace maiz {

namespace {
namespace rns = voidpalabra::reticulum;
std::string pipe_name(const std::string& peer) { return "ble:" + peer; }
} // namespace

void RadioBridge::handle(const std::vector<RadioEvent>& events) {
    rns::Node& node = rns::Node::instance();
    for (const auto& e : events) {
        switch (e.type) {
        case RadioEvent::Type::connected:
            if (e.kind == RadioKind::Ble && !pipes_.count(e.peer)) {
                rns::PipeInterface p;
                p.name = pipe_name(e.peer);
                p.bitrate = opt_.ble_bitrate;
                if (node.add_pipe(p)) pipes_[e.peer];
            }
            break;
        case RadioEvent::Type::disconnected:
            if (pipes_.erase(e.peer)) node.remove_interface(pipe_name(e.peer));
            break;
        case RadioEvent::Type::data: {
            auto it = pipes_.find(e.peer);
            if (it == pipes_.end()) break;
            for (auto& packet : it->second.decoder.feed(e.bytes)) node.pipe_in(pipe_name(e.peer), packet);
            break;
        }
        case RadioEvent::Type::group: {
            // "owner <ip>" or "client <owner ip>": the group is IP now
            const bool owner = e.detail.rfind("owner ", 0) == 0;
            const std::string ip = e.detail.substr(e.detail.find(' ') + 1);
            rns::UdpInterface u;
            u.name = "p2p";
            u.listen_host = "0.0.0.0";
            u.listen_port = opt_.p2p_port;
            u.forward_port = opt_.p2p_port;
            u.learn_peers = true;
            if (owner) {
                // the group's broadcast: the owner's address with the last octet 255
                const auto dot = ip.rfind('.');
                u.forward_host = dot == std::string::npos ? "192.168.49.255" : ip.substr(0, dot) + ".255";
                u.broadcast = true;
            } else {
                u.forward_host = ip;
            }
            if (node.add_udp(u)) group_ = e.detail;
            break;
        }
        case RadioEvent::Type::group_gone:
            node.remove_interface("p2p");
            group_.clear();
            break;
        default: break;
        }
    }
}

void RadioBridge::pump() {
    rns::Node& node = rns::Node::instance();
    for (auto& [peer, p] : pipes_) {
        for (auto& packet : node.pipe_out(pipe_name(peer))) p.out += rns::Hdlc::frame(packet);
        if (p.out.empty()) continue;
        // hand the radio what it will take; it refuses when its queue is full,
        // and the rest waits here (whole, in order)
        const std::size_t chunk = std::min<std::size_t>(p.out.size(), 4096);
        if (radio_.send(peer, p.out.substr(0, chunk))) p.out.erase(0, chunk);
    }
}

void RadioBridge::clear() {
    rns::Node& node = rns::Node::instance();
    for (const auto& [peer, _] : pipes_) node.remove_interface(pipe_name(peer));
    pipes_.clear();
    if (!group_.empty()) node.remove_interface("p2p");
    group_.clear();
}

std::vector<std::string> RadioBridge::pipes() const {
    std::vector<std::string> v;
    for (const auto& [peer, _] : pipes_) v.push_back(peer);
    return v;
}

std::size_t RadioBridge::waiting(const std::string& peer) const {
    auto it = pipes_.find(peer);
    return (it == pipes_.end() ? 0 : it->second.out.size()) + radio_.backlog(peer);
}

} // namespace maiz
