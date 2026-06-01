#pragma once

#include <cstdint>
#include <array>

class JitterBuffer {
public:
    static constexpr size_t WINDOW_SIZE = 64;
    // Opus max packet size is 1275 bytes; we keep some headroom for safety.
    static constexpr size_t MAX_PACKET_BYTES = 1500;

    struct PacketView {
        const uint8_t* data = nullptr;
        size_t len = 0;
    };

    bool push(uint16_t seq, const uint8_t* data, size_t len);
    bool pop(PacketView& out, bool& is_missing);
    void reset();
    size_t buffered() const { return buffered_; }
    int consecutiveMissing() const { return consecutive_missing_; }

private:
    struct Slot {
        uint16_t seq = 0;
        uint16_t len = 0;
        bool valid = false;
        std::array<uint8_t, MAX_PACKET_BYTES> bytes{};
    };

    std::array<Slot, WINDOW_SIZE> buffer_{};
    uint16_t expected_seq_ = 0;
    bool started_ = false;
    int buffer_target_ = 4;

    int late_packets_ = 0;
    int good_packets_ = 0;
    int consecutive_missing_ = 0;
    size_t buffered_ = 0;
}; 
