#include "peer_discovery.h"
#include "socket_utils.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace {
    constexpr int DISCOVERY_PORT = 50000;
    constexpr int DSCP_CS3 = 24;
    constexpr int BROADCAST_INTERVAL_MS = 1000;
    constexpr int SELECT_WAIT_MS = 250;
    constexpr int PEER_STALE_MS = 3500;
    constexpr const char* PEER_PREFIX = "VOICE_PEER:";
    constexpr const char* MULTICAST_IP = "239.255.0.1";

    bool is_local_interface_ipv4(in_addr addr) {
        ULONG buf_len = 0;
        const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
        if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &buf_len)!= ERROR_BUFFER_OVERFLOW || buf_len == 0) {
            return false;
        }

        std::vector<unsigned char> buf(buf_len);
        auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        if (GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &buf_len)!= NO_ERROR) {
            return false;
        }

        for (auto* a = addrs; a; a = a->Next) {
            for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
                if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family!= AF_INET) {
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
    my_room_ = room_name.empty()? "main" : room_name;
    running_.store(true);
    thread_ = std::thread(&PeerDiscovery::loop, this);
}

void PeerDiscovery::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
}

void PeerDiscovery::forceAnnounce() {
    force_broadcast_.store(true);
}

std::vector<PeerInfo> PeerDiscovery::peers() const {
    auto snapshot = std::atomic_load(&snapshot_);
    std::vector<PeerInfo> out;
    if (!snapshot) {
        return out;
    }
    for (const auto& entry : *snapshot) {
        PeerInfo info;
        info.id = entry.id;
        info.ip = entry.ip;
        info.port = entry.port;
        info.room = entry.room;
        info.is_local = entry.is_local;
        out.push_back(std::move(info));
    }
    return out;
}

std::vector<PeerSnapshot> PeerDiscovery::peerSnapshots() const {
    auto snapshot = std::atomic_load(&snapshot_);
    if (!snapshot) {
        return {};
    }
    return *snapshot;
}

std::vector<std::string> PeerDiscovery::peerLines() const {
    auto snapshot = std::atomic_load(&snapshot_);
    if (!snapshot) {
        return {};
    }
    std::vector<std::string> out;
    out.reserve(snapshot->size());
    for (const auto& entry : *snapshot) {
        out.push_back(entry.id + "|" + entry.ip + "|" +
                      std::to_string(entry.port) + "|" + entry.room + "|" +
                      (entry.is_local? "1" : "0"));
    }
    return out;
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
    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        closesocket(sock);
        running_.store(false);
        return;
    }

    ip_mreq mreq{};
    inet_pton(AF_INET, MULTICAST_IP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR) {
        closesocket(sock);
        running_.store(false);
        return;
    }

    const int multicast_loop = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&multicast_loop), sizeof(multicast_loop));

    const std::string announce = std::string(PEER_PREFIX) + my_id_ + ":" + my_room_ + ":" + std::to_string(my_port_);
    std::unordered_map<std::string, PeerInfo> peers;
    auto publish_snapshot = [&]() {
        auto next = std::make_shared<std::vector<PeerSnapshot>>();
        next->reserve(peers.size());
        for (const auto& [peer_id, info] : peers) {
            if (info.room!= my_room_) {
                continue;
            }
            PeerSnapshot p_snap;
            p_snap.id = info.id;
            p_snap.ip = info.ip;
            p_snap.port = info.port;
            p_snap.room = info.room;
            p_snap.is_local = info.is_local;
            next->push_back(std::move(p_snap));
        }
        std::atomic_store(&snapshot_, std::const_pointer_cast<const std::vector<PeerSnapshot>>(next));
    };

    auto last_broadcast = std::chrono::steady_clock::now() - std::chrono::milliseconds(BROADCAST_INTERVAL_MS);
    
    // Fixed Defect 1: Correctly allocate packet read buffers on the stack
    char buffer[512] = {0};

    while (running_.load()) {
        const auto now = std::chrono::steady_clock::now();
        bool need_broadcast = false;
        if (force_broadcast_.exchange(false) ||
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_broadcast).count() >= BROADCAST_INTERVAL_MS) {
            need_broadcast = true;
        }

        if (need_broadcast) {
            sockaddr_in mcast_addr{};
            mcast_addr.sin_family = AF_INET;
            mcast_addr.sin_port = htons(DISCOVERY_PORT);
            inet_pton(AF_INET, MULTICAST_IP, &mcast_addr.sin_addr);

            sendto(sock, announce.c_str(), static_cast<int>(announce.size()), 0,
                   reinterpret_cast<const sockaddr*>(&mcast_addr), sizeof(mcast_addr));
            last_broadcast = now;
        }

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(sock, &read_set);
        timeval tv{0, SELECT_WAIT_MS * 1000};
        if (select(0, &read_set, nullptr, nullptr, &tv) > 0 && FD_ISSET(sock, &read_set)) {
            sockaddr_in src{};
            int src_len = sizeof(src);
            
            // Fixed Defect 1: Pass buffer size safely, preventing data overflows
            int recv_len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&src), &src_len);
            if (recv_len > 0) {
                buffer[recv_len] = '\0';
                std::string payload(buffer, recv_len);
                if (payload.rfind(PEER_PREFIX, 0) == 0) {
                    std::string data = payload.substr(std::strlen(PEER_PREFIX));
                    size_t first = data.find(':');
                    size_t second = data.find(':', first+1);
                    if (first!= std::string::npos) {
                        std::string peer_id = data.substr(0, first);
                        std::string peer_room = (second!= std::string::npos)? data.substr(first+1, second-first-1) : "main";
                        std::string port_str = (second!= std::string::npos)? data.substr(second+1) : "50002";
                        if (peer_id!= my_id_) {
                            // Fixed Defect 2: Safely convert string representations of IPv4 addresses
                            char ip_str[INET_ADDRSTRLEN] = {0};
                            inet_ntop(AF_INET, &src.sin_addr, ip_str, sizeof(ip_str));

                            PeerInfo info;
                            info.id = peer_id;
                            info.ip = ip_str;
                            try {
                                info.port = static_cast<uint16_t>(std::stoi(port_str));
                            } catch (const std::exception&) {
                                info.port = 50002;
                            }
                            info.room = peer_room;
                            info.last_seen = std::chrono::steady_clock::now();

                            const bool is_loopback_src = (std::strcmp(ip_str, "127.0.0.1") == 0) ||
                                                         (src.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
                            const bool is_same_host = is_loopback_src || is_local_interface_ipv4(src.sin_addr);
                            if (is_same_host) {
                                info.ip = "127.0.0.1";
                                info.is_local = true;
                            }

                            peers[peer_id] = info;
                            const auto now_seen = std::chrono::steady_clock::now();
                            for (auto it = peers.begin(); it!= peers.end();) {
                                if (std::chrono::duration_cast<std::chrono::milliseconds>(now_seen - it->second.last_seen).count() > PEER_STALE_MS) {
                                    it = peers.erase(it);
                                } else {
                                    ++it;
                                }
                            }
                            publish_snapshot();
                        }
                    }
                }
            }
        } else {
            const auto now_seen = std::chrono::steady_clock::now();
            for (auto it = peers.begin(); it!= peers.end();) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now_seen - it->second.last_seen).count() > PEER_STALE_MS) {
                    it = peers.erase(it);
                } else {
                    ++it;
                }
            }
            publish_snapshot();
        }
    }
    
    setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));
    closesocket(sock);
}
