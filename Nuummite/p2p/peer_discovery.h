#ifndef PEER_DISCOVERY_H
#define PEER_DISCOVERY_H
#include <atomic>
#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

struct PeerInfo {
    std::string id;
    std::string ip;
    uint16_t port = 50002;
    std::string room;
    bool is_local = false;
    std::chrono::steady_clock::time_point last_seen;
};

struct PeerSnapshot {
    std::string id;
    std::string ip;
    uint16_t port = 50002;
    std::string room;
    bool is_local = false;
};

class PeerDiscovery {
public:
    PeerDiscovery() = default;
    ~PeerDiscovery();

    void start(const std::string& my_id, uint16_t audio_port, const std::string& room_name);
    void stop();
    std::vector<PeerInfo> peers() const;
    std::vector<PeerSnapshot> peerSnapshots() const;
    std::vector<std::string> peerLines() const;
    std::string currentRoom() const { return my_room_; }

private:
    void loop();

    std::string my_id_;
    uint16_t my_port_ = 50002;
    std::string my_room_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::shared_ptr<const std::vector<PeerSnapshot>> snapshot_;
};

#endif // PEER_DISCOVERY_H
