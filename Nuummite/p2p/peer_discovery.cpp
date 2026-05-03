#include "peer_discovery.h"
#include "socket_utils.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

namespace {
    constexpr int DISCOVERY_PORT = 50000;
    constexpr int DSCP_CS3 = 24;
    constexpr int BROADCAST_INTERVAL_MS = 1000;
    constexpr int SELECT_WAIT_MS = 250;
    constexpr int PEER_STALE_MS = 3500;
    constexpr const char* PEER_PREFIX = "VOICE_PEER:";

    bool is_local_interface_ipv4(in_addr addr) {
        // True if addr matches any local (non-loopback) adapter IPv4 address.
        ULONG buf_len = 0;
        const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
        if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &buf_len) != ERROR_BUFFER_OVERFLOW || buf_len == 0) {
            return false;
        }

        std::vector<unsigned char> buf(buf_len);
        auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        if (GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &buf_len) != NO_ERROR) {
            return false;
        }

        for (auto* a = addrs; a; a = a->Next) {
            for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
                if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) {
                    continue;
                }
                auto* sin = reinterpret_cast<const sockaddr_in*>(u->Address.lpSockaddr);
                if (sin->sin_addr.s_addr == addr.s_addr) {
                    return true;
                }
            }
        }
        return false;
    }
} // namespace

PeerDiscovery::~PeerDiscovery() { stop(); }

void PeerDiscovery::start(const std::string& my_id, uint16_t audio_port, const std::string& room_name) {
    if (running_.load()) return;
    my_id_ = my_id;
    my_port_ = audio_port;
    my_room_ = room_name.empty() ? "main" : room_name;
    running_.store(true);
    thread_ = std::thread(&PeerDiscovery::loop, this);
}

void PeerDiscovery::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
}

std::vector<PeerInfo> PeerDiscovery::peers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PeerInfo> out;
    for (const auto& entry : peers_) {
        if (entry.second.room == my_room_) {
            out.push_back(entry.second);
        }
    }
    return out;
}

void PeerDiscovery::pruneLocked() {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = peers_.begin(); it != peers_.end();) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_seen).count() > PEER_STALE_MS) {
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }
}

void PeerDiscovery::loop() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) { running_.store(false); return; }

    const int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    socket_utils::set_dscp(sock, DSCP_CS3);

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(DISCOVERY_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));

    const int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));

    const std::string announce = std::string(PEER_PREFIX) + my_id_ + ":" + my_room_ + ":" + std::to_string(my_port_);

    auto last_broadcast = std::chrono::steady_clock::now() - std::chrono::milliseconds(BROADCAST_INTERVAL_MS);

    while (running_.load()) {
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_broadcast).count() >= BROADCAST_INTERVAL_MS) {
            sockaddr_in bcast{};
            bcast.sin_family = AF_INET;
            bcast.sin_port = htons(DISCOVERY_PORT);
            bcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            sendto(sock, announce.c_str(), static_cast<int>(announce.size()), 0,
                   reinterpret_cast<const sockaddr*>(&bcast), sizeof(bcast));

            // Also unicast to loopback so multiple instances on the same host discover each other
            sockaddr_in loop{};
            loop.sin_family = AF_INET;
            loop.sin_port = htons(DISCOVERY_PORT);
            inet_pton(AF_INET, "127.0.0.1", &loop.sin_addr);
            sendto(sock, announce.c_str(), static_cast<int>(announce.size()), 0,
                   reinterpret_cast<const sockaddr*>(&loop), sizeof(loop));
            last_broadcast = now;
        }

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(sock, &read_set);
        timeval tv{0, SELECT_WAIT_MS * 1000};
        if (select(0, &read_set, nullptr, nullptr, &tv) > 0 && FD_ISSET(sock, &read_set)) {
            char buffer[512] = {0};
            sockaddr_in src{};
            int src_len = sizeof(src);
            int recv_len = recvfrom(sock, buffer, sizeof(buffer)-1, 0, reinterpret_cast<sockaddr*>(&src), &src_len);
            if (recv_len > 0) {
                std::string payload(buffer, buffer + recv_len);
                if (payload.rfind(PEER_PREFIX, 0) == 0) {
                    std::string data = payload.substr(std::strlen(PEER_PREFIX));
                    // format: client_id:room:port
                    size_t first = data.find(':');
                    size_t second = data.find(':', first+1);
                    if (first != std::string::npos) {
                        std::string peer_id = data.substr(0, first);
                        std::string peer_room = (second != std::string::npos) ? data.substr(first+1, second-first-1) : "main";
                        std::string port_str = (second != std::string::npos) ? data.substr(second+1) : "50002";

                         if (peer_id != my_id_) {
                             char ip_str[INET_ADDRSTRLEN] = {0};
                             inet_ntop(AF_INET, &src.sin_addr, ip_str, sizeof(ip_str));

                             PeerInfo info;
                             info.id = peer_id;
                             info.ip = ip_str;
                             info.port = static_cast<uint16_t>(std::stoi(port_str));
                             info.room = peer_room;
                             info.last_seen = std::chrono::steady_clock::now();

                              // Prefer loopback for same-host peers so multiple local instances remain distinct.
                              const bool is_loopback_src = (std::strcmp(ip_str, "127.0.0.1") == 0) ||
                                                           (src.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
                              const bool is_same_host = is_loopback_src || is_local_interface_ipv4(src.sin_addr);
                              if (is_same_host) {
                                  info.ip = "127.0.0.1";
                                  info.is_local = true;
                              }

                             std::lock_guard<std::mutex> lock(mutex_);
                             peers_[peer_id] = info;
                             pruneLocked();
                         }
                    }
                }
            }
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            pruneLocked();
        }
    }
    closesocket(sock);
}
