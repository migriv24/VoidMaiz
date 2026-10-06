/* src/lan/radio_loopback.cpp — the desktop stand-in for Bluetooth LE
 * (voidmaiz/radio.hpp, loopback_radio).
 *
 * A "radio" whose air is a local TCP connection, throttled to a chosen speed,
 * so two processes on one computer exercise everything above the radio (the
 * Reticulum pipe, membership, sync, the progress bars) exactly as two phones
 * would, minus the radio. One worker thread owns the sockets; poll() hands its
 * events to the application's thread.
 *
 * On the wire, each side first sends one line, "MAIZRADIO1 <name>\t<tag>\n",
 * so the other can report what it found; everything after it is the stream. */
#include "voidmaiz/radio.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using sock_t = SOCKET;
static const sock_t kBad = INVALID_SOCKET;
static void close_sock(sock_t s) { closesocket(s); }
static void nonblock(sock_t s) {
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
}
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_t = int;
static const sock_t kBad = -1;
static void close_sock(sock_t s) { ::close(s); }
static void nonblock(sock_t s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK); }
#endif

namespace maiz {

namespace {

using Clock = std::chrono::steady_clock;

class LoopbackRadio : public RadioPlatform {
  public:
    explicit LoopbackRadio(const LoopbackRadioOptions& o) : opt_(o) {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }
    ~LoopbackRadio() override { stop(RadioKind::Ble); }

    RadioAccess access(RadioKind kind) override {
        return kind == RadioKind::Ble ? RadioAccess::Ready : RadioAccess::Unavailable;
    }
    bool request(RadioKind kind) override { return kind == RadioKind::Ble; }

    bool start(RadioKind kind, const std::string& tag) override {
        if (kind != RadioKind::Ble) return false;
        if (running_) return true;
        tag_ = tag;
        running_ = true;
        worker_ = std::thread([this] { run(); });
        return true;
    }
    void stop(RadioKind kind) override {
        if (kind != RadioKind::Ble || !running_) return;
        running_ = false;
        if (worker_.joinable()) worker_.join();
    }
    bool running(RadioKind kind) const override { return kind == RadioKind::Ble && running_; }

    bool connect(RadioKind kind, const std::string&) override { return kind == RadioKind::Ble && running_; }
    void disconnect(RadioKind, const std::string&) override { drop_ = true; }

    bool send(const std::string& peer, const std::string& bytes) override {
        std::lock_guard<std::mutex> lk(mu_);
        if (peer != peer_id_ || !linked_) return false;
        if (out_.size() + bytes.size() > kMaxBacklog) return false;
        out_ += bytes;
        return true;
    }
    std::size_t backlog(const std::string& peer) const override {
        std::lock_guard<std::mutex> lk(mu_);
        return peer == peer_id_ ? out_.size() : 0;
    }
    void poll(std::vector<RadioEvent>& out) override {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& e : events_) out.push_back(std::move(e));
        events_.clear();
    }

  private:
    static constexpr std::size_t kMaxBacklog = 512 * 1024;

    void emit(RadioEvent e) {
        std::lock_guard<std::mutex> lk(mu_);
        events_.push_back(std::move(e));
    }

    void run() {
        sock_t listener = kBad, s = kBad;
        if (opt_.listen_port) {
            listener = ::socket(AF_INET, SOCK_STREAM, 0);
            int yes = 1;
            setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            a.sin_port = htons((unsigned short)opt_.listen_port);
            if (::bind(listener, (sockaddr*)&a, sizeof a) != 0) {
                emit({RadioEvent::Type::error, RadioKind::Ble, "", "", "", "", "loopback radio: port in use"});
                close_sock(listener);
                listener = kBad;
            } else {
                ::listen(listener, 1);
                nonblock(listener);
            }
        }
        std::string hello = "MAIZRADIO1 " + opt_.name + "\t" + tag_ + "\n", inbuf;
        bool got_hello = false;
        double allowance = 0;
        auto last = Clock::now(), next_try = Clock::now();
        while (running_) {
            if (s == kBad) {
                if (listener != kBad) {
                    const sock_t c = ::accept(listener, nullptr, nullptr);
                    if (c != kBad) s = c;
                }
                if (s == kBad && opt_.connect_port && Clock::now() >= next_try) {
                    next_try = Clock::now() + std::chrono::seconds(1);
                    const sock_t c = ::socket(AF_INET, SOCK_STREAM, 0);
                    sockaddr_in a{};
                    a.sin_family = AF_INET;
                    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                    a.sin_port = htons((unsigned short)opt_.connect_port);
                    if (::connect(c, (sockaddr*)&a, sizeof a) == 0) s = c;
                    else close_sock(c);
                }
                if (s != kBad) {
                    nonblock(s);
                    ::send(s, hello.data(), (int)hello.size(), 0);
                    got_hello = false;
                    inbuf.clear();
                    allowance = 0;
                    last = Clock::now();
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                }
            }
            bool broken = drop_.exchange(false);
            // read
            char buf[4096];
            for (;;) {
                const int n = (int)::recv(s, buf, sizeof buf, 0);
                if (n == 0) {
                    broken = true;
                    break;
                }
                if (n < 0) break; // would block (or an error the next send will find)
                std::string got(buf, (std::size_t)n);
                if (!got_hello) {
                    inbuf += got;
                    const auto nl = inbuf.find('\n');
                    if (nl == std::string::npos) continue;
                    const std::string line = inbuf.substr(0, nl);
                    got = inbuf.substr(nl + 1);
                    got_hello = true;
                    std::string name = line.size() > 11 ? line.substr(11) : "";
                    std::string tag;
                    if (const auto t = name.find('\t'); t != std::string::npos) {
                        tag = name.substr(t + 1);
                        name.resize(t);
                    }
                    {
                        std::lock_guard<std::mutex> lk(mu_);
                        peer_id_ = "loop:" + name;
                        linked_ = true;
                        out_.clear();
                    }
                    emit({RadioEvent::Type::found, RadioKind::Ble, "loop:" + name, name, tag, "", "", -40});
                    emit({RadioEvent::Type::connected, RadioKind::Ble, "loop:" + name, name, tag, "", "", -40});
                }
                if (!got.empty()) {
                    RadioEvent e{RadioEvent::Type::data, RadioKind::Ble, "", "", "", "", "", 0};
                    {
                        std::lock_guard<std::mutex> lk(mu_);
                        e.peer = peer_id_;
                    }
                    e.bytes = std::move(got);
                    emit(std::move(e));
                }
            }
            // write, at the radio's speed
            const auto now = Clock::now();
            const double dt = std::chrono::duration<double>(now - last).count();
            last = now;
            if (opt_.bytes_per_second > 0)
                allowance = std::min(allowance + dt * opt_.bytes_per_second, opt_.bytes_per_second / 4);
            {
                std::lock_guard<std::mutex> lk(mu_);
                std::size_t can = out_.size();
                if (opt_.bytes_per_second > 0) can = std::min(can, (std::size_t)std::max(0.0, allowance));
                if (can > 0 && got_hello) {
                    const int n = (int)::send(s, out_.data(), (int)can, 0);
                    if (n > 0) {
                        out_.erase(0, (std::size_t)n);
                        allowance -= n;
                    }
                }
            }
            if (broken) {
                close_sock(s);
                s = kBad;
                std::string peer;
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    peer = peer_id_;
                    linked_ = false;
                    out_.clear();
                }
                if (got_hello)
                    emit({RadioEvent::Type::disconnected, RadioKind::Ble, peer, "", "", "", "the other side closed", 0});
                got_hello = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (s != kBad) close_sock(s);
        if (listener != kBad) close_sock(listener);
    }

    LoopbackRadioOptions opt_;
    std::string tag_;
    std::atomic<bool> running_{false}, drop_{false};
    std::thread worker_;
    mutable std::mutex mu_;
    std::string peer_id_, out_;
    bool linked_ = false;
    std::vector<RadioEvent> events_;
};

} // namespace

std::unique_ptr<RadioPlatform> loopback_radio(const LoopbackRadioOptions& options) {
    return std::make_unique<LoopbackRadio>(options);
}

} // namespace maiz
