#include "peer_discovery.h"

#include "socket_utils.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int DISCOVERY_PORT = 50000;
constexpr int DSCP_CS3 = 24;
constexpr int IP_TOS_CS3 = DSCP_CS3 << 2;
constexpr int BROADCAST_INTERVAL_MS = 1000;
constexpr int SELECT_WAIT_MS = 250;
constexpr int PEER_STALE_MS = 3500;
constexpr const char* PEER_PREFIX = "VOICE_PEER:";
} // namespace

namespace {
bool is_local_ip(const std::string& ip) {
    in_addr target{};
    if (inet_pton(AF_INET, ip.c_str(), &target) != 1) {
        return false;
    }

    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* res = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &res) != 0) {
        return false;
    }

    bool match = false;
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        if (addr && std::memcmp(&addr->sin_addr, &target, sizeof(in_addr)) == 0) {
            match = true;
            break;
        }
    }
    if (res) {
        freeaddrinfo(res);
    }
    return match;
}

bool is_valid_ipv4(const std::string& ip) {
    in_addr addr{};
    return inet_pton(AF_INET, ip.c_str(), &addr) == 1;
}
} // namespace

PeerDiscovery::~PeerDiscovery() {
    stop();
}

void PeerDiscovery::start(const std::string& my_id, uint16_t audio_port) {
    if (running_.load()) {
        return;
    }
    my_id_ = my_id;
    my_port_ = audio_port;
    running_.store(true);
    thread_ = std::thread(&PeerDiscovery::loop, this);
}

void PeerDiscovery::stop() {
    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::vector<PeerInfo> PeerDiscovery::peers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PeerInfo> out;
    out.reserve(peers_.size());
    for (const auto& entry : peers_) {
        out.push_back(entry.second);
    }
    return out;
}

void PeerDiscovery::addManualPeer(const std::string& peer_id, const std::string& ip) {
    if (ip.empty() || !is_valid_ipv4(ip)) {
        return;
    }
    const std::string id = peer_id.empty() ? ip : peer_id;
    if (id == my_id_ || is_local_ip(ip)) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    manual_peers_[id] = ip;
    PeerInfo info;
    info.id = id;
    info.ip = ip;
    info.port = 0; // unknown until their announce arrives
    info.last_seen = std::chrono::steady_clock::now();
    peers_[id] = info;
    std::cout << "[DISCOVERY] Manual peer added: " << id << " @ " << ip << "\n";
}

void PeerDiscovery::pruneLocked() {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = peers_.begin(); it != peers_.end();) {
        if (manual_peers_.count(it->first)) {
            ++it;
            continue;
        }
        const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_seen).count();
        if (age > PEER_STALE_MS) {
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }
}

void PeerDiscovery::setIncomingSignalingHandler(SignalingHandler handler) {
    signaling_handler_ = std::move(handler);
}

void PeerDiscovery::sendCallInvite(const std::string& peer_id) {
    if (peer_id.empty() || peer_id == my_id_) {
        return;
    }
    std::string msg = "VOICE_CALL:" + my_id_ + ":" + std::to_string(my_port_);
    std::lock_guard<std::mutex> lock(signaling_mutex_);
    pending_signaling_.emplace_back(peer_id, msg);
    std::cout << "[DISCOVERY] Queued CALL invite to " << peer_id << "\n";
}

void PeerDiscovery::sendCallAccept(const std::string& peer_id) {
    if (peer_id.empty() || peer_id == my_id_) {
        return;
    }
    std::string msg = "VOICE_ACCEPT:" + my_id_ + ":" + std::to_string(my_port_);
    std::lock_guard<std::mutex> lock(signaling_mutex_);
    pending_signaling_.emplace_back(peer_id, msg);
}

void PeerDiscovery::sendCallDecline(const std::string& peer_id) {
    if (peer_id.empty() || peer_id == my_id_) {
        return;
    }
    std::string msg = "VOICE_DECLINE:" + my_id_;
    std::lock_guard<std::mutex> lock(signaling_mutex_);
    pending_signaling_.emplace_back(peer_id, msg);
}

void PeerDiscovery::hintPeerAddress(const std::string& peer_id, const std::string& ip, uint16_t port) {
    if (peer_id.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    PeerInfo info;
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) {
        info = it->second;
    }
    info.id = peer_id;
    if (!ip.empty()) {
        info.ip = ip;
        manual_peers_[peer_id] = ip; // keep and announce/unicast; prevents pruning
    }
    if (port > 0) {
        info.port = port;
    }
    info.last_seen = std::chrono::steady_clock::now();
    peers_[peer_id] = info;
}

void PeerDiscovery::processPendingSignaling(SOCKET sock) {
    std::deque<std::pair<std::string, std::string>> to_send;
    {
        std::lock_guard<std::mutex> lock(signaling_mutex_);
        to_send.swap(pending_signaling_);
    }

    for (const auto& entry : to_send) {
        const std::string& peer_id = entry.first;
        const std::string& msg = entry.second;

        std::string ip;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto mit = manual_peers_.find(peer_id);
            if (mit != manual_peers_.end()) {
                ip = mit->second;
            } else {
                auto pit = peers_.find(peer_id);
                if (pit != peers_.end()) {
                    ip = pit->second.ip;
                }
            }
        }
        bool sent = false;
        if (!ip.empty()) {
            sockaddr_in paddr{};
            paddr.sin_family = AF_INET;
            paddr.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
            if (inet_pton(AF_INET, ip.c_str(), &paddr.sin_addr) == 1) {
                sendto(sock,
                       msg.c_str(),
                       static_cast<int>(msg.size()),
                       0,
                       reinterpret_cast<const sockaddr*>(&paddr),
                       sizeof(paddr));
                sent = true;
                std::cout << "[DISCOVERY] Sent " << msg << " to " << peer_id << " @ " << ip << "\n";
            }
        }
        // Fallback: broadcast once if direct IP missing or failed
        if (!sent) {
            sockaddr_in bcast{};
            bcast.sin_family = AF_INET;
            bcast.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
            bcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            sendto(sock,
                   msg.c_str(),
                   static_cast<int>(msg.size()),
                   0,
                   reinterpret_cast<const sockaddr*>(&bcast),
                   sizeof(bcast));
            std::cout << "[DISCOVERY] Broadcast " << msg << " (no direct IP for " << peer_id << ")\n";
        }
    }
}

void PeerDiscovery::loop() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        running_.store(false);
        return;
    }

    const int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    socket_utils::set_dscp(sock, IP_TOS_CS3);

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        closesocket(sock);
        running_.store(false);
        return;
    }

    const int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));

    const std::string announce = std::string(PEER_PREFIX) + my_id_ + ":" + std::to_string(my_port_);
    auto last_broadcast = std::chrono::steady_clock::now() - std::chrono::milliseconds(BROADCAST_INTERVAL_MS);

    while (running_.load()) {
        processPendingSignaling(sock);

        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_broadcast).count() >= BROADCAST_INTERVAL_MS) {
            sockaddr_in bcast{};
            bcast.sin_family = AF_INET;
            bcast.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
            bcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            sendto(sock,
                   announce.c_str(),
                   static_cast<int>(announce.size()),
                   0,
                   reinterpret_cast<const sockaddr*>(&bcast),
                   sizeof(bcast));

            std::vector<std::string> gateways = {
                "192.168.1.1",
                "192.168.0.1",
                "10.0.0.1",
                "192.168.1.255",
                "192.168.0.255",
            };
            for (const auto& gw : gateways) {
                sockaddr_in gw_addr{};
                gw_addr.sin_family = AF_INET;
                gw_addr.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
                inet_pton(AF_INET, gw.c_str(), &gw_addr.sin_addr);
                sendto(sock,
                       announce.c_str(),
                       static_cast<int>(announce.size()),
                       0,
                       reinterpret_cast<const sockaddr*>(&gw_addr),
                       sizeof(gw_addr));
            }

            // Unicast to manual peers (cross-building)
            std::unordered_map<std::string, std::string> manual_copy;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                manual_copy = manual_peers_;
            }
            for (const auto& entry : manual_copy) {
                sockaddr_in paddr{};
                paddr.sin_family = AF_INET;
                paddr.sin_port = htons(static_cast<u_short>(DISCOVERY_PORT));
                if (inet_pton(AF_INET, entry.second.c_str(), &paddr.sin_addr) == 1) {
                    sendto(sock,
                           announce.c_str(),
                           static_cast<int>(announce.size()),
                           0,
                           reinterpret_cast<const sockaddr*>(&paddr),
                           sizeof(paddr));
                }
            }
            last_broadcast = now;
        }

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(sock, &read_set);

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = SELECT_WAIT_MS * 1000;

        const int sel = select(0, &read_set, nullptr, nullptr, &tv);
        if (sel > 0 && FD_ISSET(sock, &read_set)) {
            char buffer[512] = {0};
            sockaddr_in src{};
            int src_len = sizeof(src);
            const int recv_len = recvfrom(sock,
                                          buffer,
                                          static_cast<int>(sizeof(buffer) - 1),
                                          0,
                                          reinterpret_cast<sockaddr*>(&src),
                                          &src_len);
            if (recv_len > 0) {
                const std::string payload(buffer, buffer + recv_len);
                if (payload.rfind(PEER_PREFIX, 0) == 0) {
                    std::string peer_blob = payload.substr(std::strlen(PEER_PREFIX));
                    peer_blob.erase(std::remove(peer_blob.begin(), peer_blob.end(), '\r'), peer_blob.end());
                    peer_blob.erase(std::remove(peer_blob.begin(), peer_blob.end(), '\n'), peer_blob.end());

                    std::string peer_id = peer_blob;
                    uint16_t peer_port = 50002;
                    const auto colon = peer_blob.find(':');
                    if (colon != std::string::npos) {
                        peer_id = peer_blob.substr(0, colon);
                        try {
                            int parsed = std::stoi(peer_blob.substr(colon + 1));
                            if (parsed > 0 && parsed <= 65535) {
                                peer_port = static_cast<uint16_t>(parsed);
                            }
                        } catch (...) {
                            peer_port = 50002;
                        }
                    }
                    if (!peer_id.empty() && peer_id != my_id_) {
                        char ip_str[INET_ADDRSTRLEN] = {0};
                        inet_ntop(AF_INET, &src.sin_addr, ip_str, sizeof(ip_str));
                        PeerInfo info;
                        info.id = peer_id;
                        info.ip = ip_str;
                        info.port = peer_port;
                        info.last_seen = std::chrono::steady_clock::now();

                        std::lock_guard<std::mutex> lock(mutex_);
                        peers_[peer_id] = info;
                        pruneLocked();
                    }
                } else if (payload.rfind("VOICE_", 0) == 0) {
                    std::string type;
                    std::string from_id;
                    uint16_t rtp_port = 0;

                    // Format: VOICE_<TYPE>:<id>:<port?> or VOICE_<TYPE>:<id>
                    const size_t colon1 = payload.find(':', 6);
                    if (colon1 != std::string::npos) {
                        type = payload.substr(6, colon1 - 6);
                        std::string rest = payload.substr(colon1 + 1);
                        const size_t colon2 = rest.find(':');
                        if (colon2 != std::string::npos) {
                            from_id = rest.substr(0, colon2);
                            try {
                                rtp_port = static_cast<uint16_t>(std::stoi(rest.substr(colon2 + 1)));
                            } catch (...) {
                                rtp_port = 0;
                            }
                        } else {
                            from_id = rest;
                        }
                    }

                    char ip_str[INET_ADDRSTRLEN] = {0};
                    inet_ntop(AF_INET, &src.sin_addr, ip_str, sizeof(ip_str));

                    hintPeerAddress(from_id, ip_str, rtp_port);

                    if (!type.empty() && !from_id.empty() && signaling_handler_) {
                        signaling_handler_(type, from_id, rtp_port, ip_str);
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
