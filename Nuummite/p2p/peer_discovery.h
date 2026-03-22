#ifndef PEER_DISCOVERY_H
#define PEER_DISCOVERY_H

#include <atomic>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

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

private:
    void loop();
    void pruneLocked();

    std::string my_id_;
    uint16_t my_port_ = 50002;
    std::atomic<bool> running_{false};
    std::thread thread_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, PeerInfo> peers_;
};

#endif
