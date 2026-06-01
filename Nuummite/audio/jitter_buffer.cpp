#include "jitter_buffer.h"
#include <algorithm>

namespace {
constexpr uint16_t SEQ_HALF_RANGE = 0x8000;
}

bool JitterBuffer::push(uint16_t seq, const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return false;
    }
    if (len > MAX_PACKET_BYTES) {
        return false;
    }

    // Reset buffer tracking if speech resumes after an extended silent period
    if (consecutive_missing_ > 10) {
        reset();
        expected_seq_ = seq;
    }

    if (buffered_ == 0 &&!started_) {
        expected_seq_ = seq;
    }

    const uint16_t diff = static_cast<uint16_t>(seq - expected_seq_);
    
    // Evaluate if the sequence offset represents a late arrival
    if (diff >= SEQ_HALF_RANGE) {
        // If consecutive packets have been missing, assume a sequence drift has occurred.
        // Sync the sequence offset with the current packet rather than dropping it.
        if (consecutive_missing_ > 0) {
            reset();
            expected_seq_ = seq;
        } else {
            late_packets_++;
            return false;
        }
    }
    
    // Evaluate if the sequence offset exceeds the buffering window
    if (diff >= WINDOW_SIZE) {
        if (consecutive_missing_ > 0) {
            reset();
            expected_seq_ = seq;
        } else {
            late_packets_++;
            return false;
        }
    }

    Slot& slot = buffer_[seq % WINDOW_SIZE];
    if (slot.valid && slot.seq!= seq) {
        if (buffered_ > 0) {
            buffered_--;
        }
    }

    slot.seq = seq;
    slot.len = static_cast<uint16_t>(len);
    std::copy_n(data, len, slot.bytes.begin());
    if (!slot.valid) {
        slot.valid = true;
        buffered_++;
    }

    return true;
}

bool JitterBuffer::pop(PacketView& out, bool& is_missing) {
    // If consecutive frames are missing, reset the buffer
    // and wait for the next incoming sequence to synchronize.
    if (consecutive_missing_ > 20) {
        reset();
        return false;
    }

    if (!started_) {
        if (buffered_ >= static_cast<size_t>(buffer_target_)) {
            started_ = true;
        } else {
            return false;
        }
    }

    Slot& slot = buffer_[expected_seq_ % WINDOW_SIZE];
    if (slot.valid && slot.seq == expected_seq_) {
        out.data = slot.bytes.data();
        out.len = slot.len;
        slot.valid = false;
        slot.len = 0;
        if (buffered_ > 0) {
            buffered_--;
        }
        is_missing = false;
        good_packets_++;
        consecutive_missing_ = 0;
    } else {
        out.data = nullptr;
        out.len = 0;
        is_missing = true;
        late_packets_++;
        consecutive_missing_++;
    }

    expected_seq_++;

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
    for (auto& slot : buffer_) {
        slot.valid = false;
        slot.len = 0;
        slot.seq = 0;
    }
    started_ = false;
    late_packets_ = 0;
    good_packets_ = 0;
    expected_seq_ = 0;
    consecutive_missing_ = 0;
    buffered_ = 0;
}
