#ifndef PEER_DISCOVERY_H
#define PEER_DISCOVERY_H

#include <atomic>
#include <cstdint>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <winsock2.h>

struct PeerInfo {
    std::string id;
    std::string ip;
    uint16_t port = 50002;
    std::chrono::steady_clock::time_point last_seen;
};

class PeerDiscovery {
public:
    PeerDiscovery() = default;
    ~PeerDiscovery();

    void start(const std::string& my_id, uint16_t audio_port);
    void stop();

    std::vector<PeerInfo> peers() const;

    // Manual campus cross-building support
    void addManualPeer(const std::string& peer_id, const std::string& ip);

    // UDP signaling over discovery socket
    void sendCallInvite(const std::string& peer_id);
    void sendCallAccept(const std::string& peer_id);
    void sendCallDecline(const std::string& peer_id);
    void hintPeerAddress(const std::string& peer_id, const std::string& ip, uint16_t port);

    using SignalingHandler = std::function<void(const std::string& type,
                                                const std::string& from_id,
                                                uint16_t rtp_port,
                                                const std::string& from_ip)>;
    void setIncomingSignalingHandler(SignalingHandler handler);

private:
    void loop();
    void pruneLocked();
    void processPendingSignaling(SOCKET sock);

    std::string my_id_;
    uint16_t my_port_ = 50002;
    std::atomic<bool> running_{false};
    std::thread thread_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, PeerInfo> peers_;
    // Permanent manual peers (never pruned, unicast announced)
    std::unordered_map<std::string, std::string> manual_peers_;

    SignalingHandler signaling_handler_;
    std::deque<std::pair<std::string, std::string>> pending_signaling_; // peer_id -> msg
    std::mutex signaling_mutex_;
};

#endif
