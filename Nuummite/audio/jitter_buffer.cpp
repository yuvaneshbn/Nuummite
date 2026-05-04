#include "jitter_buffer.h"

namespace {
constexpr size_t MAX_BUFFERED_FRAMES = 64;
}

void JitterBuffer::push(uint16_t seq, const std::vector<int16_t>& frame) {
    buffer_[seq] = frame;
    while (buffer_.size() > MAX_BUFFERED_FRAMES) {
        buffer_.erase(buffer_.begin());
    }
}

bool JitterBuffer::pop(std::vector<int16_t>& out, bool& is_missing) {
    if (!started_) {
        if (buffer_.size() >= static_cast<size_t>(buffer_target_)) {
            expected_seq_ = buffer_.begin()->first;
            started_ = true;
        } else {
            return false;
        }
    }

    auto it = buffer_.find(expected_seq_);

    if (it != buffer_.end()) {
        out = std::move(it->second);
        buffer_.erase(it);
        is_missing = false;
        good_packets_++;
        consecutive_missing_ = 0;
    } else {
        out.clear();
        is_missing = true;
        late_packets_++;
        consecutive_missing_++;
    }

    expected_seq_++;

    // Adaptive buffer target.
    if ((good_packets_ + late_packets_) > 50) {
        const float loss = static_cast<float>(late_packets_) / static_cast<float>(good_packets_ + late_packets_);

        if (loss > 0.1f && buffer_target_ < 6) {
            buffer_target_++;
        } else if (loss < 0.02f && buffer_target_ > 2) {
            buffer_target_--;
        }

        good_packets_ = 0;
        late_packets_ = 0;
    }

    return true;
}

void JitterBuffer::reset() {
    buffer_.clear();
    started_ = false;
    late_packets_ = 0;
    good_packets_ = 0;
    buffer_target_ = 4;
    expected_seq_ = 0;
    consecutive_missing_ = 0;
}
