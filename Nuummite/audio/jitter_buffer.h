#pragma once

#include <cstdint>
#include <map>
#include <vector>

class JitterBuffer {
public:
    void push(uint16_t seq, const std::vector<int16_t>& frame);
    bool pop(std::vector<int16_t>& out, bool& is_missing);
    void reset();
    int consecutiveMissing() const { return consecutive_missing_; }

private:
    std::map<uint16_t, std::vector<int16_t>> buffer_;
    uint16_t expected_seq_ = 0;
    bool started_ = false;
    int buffer_target_ = 4;

    int late_packets_ = 0;
    int good_packets_ = 0;
    int consecutive_missing_ = 0;
};
