#include "rnnoise_processor.h"
#include <algorithm>
#include <iostream>
#include <cmath>

RnNoiseProcessor::RnNoiseProcessor() {
    state_ = rnnoise_create(nullptr);
    if (state_) {
        std::cout << "[RNNOISE] Initialized successfully (linked DLL)\n";
    } else {
        std::cerr << "[RNNOISE] Failed to create state\n";
    }
}

RnNoiseProcessor::~RnNoiseProcessor() {
    if (state_) rnnoise_destroy(state_);
}

void RnNoiseProcessor::process(std::vector<int16_t>& frame) {
    if (!state_ || (frame.size() != 480 && frame.size() != 960)) {
        return;
    }

    auto process_chunk = [&](size_t offset) {
        float float_frame[480];
        for (int i = 0; i < 480; ++i) {
            float_frame[i] = static_cast<float>(frame[offset + i]) / 32768.0f;
        }
        rnnoise_process_frame(state_, float_frame, float_frame);
        for (int i = 0; i < 480; ++i) {
            frame[offset + i] = static_cast<int16_t>(
                std::clamp(float_frame[i] * 32768.0f, -32768.0f, 32767.0f));
        }
    };

    if (frame.size() == 480) {
        process_chunk(0);
    } else {
        process_chunk(0);
        process_chunk(480);
    }
}
