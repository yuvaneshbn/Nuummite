#include "rnnoise_processor.h"
#include <algorithm>
#include <iostream>
#include <cmath>

RnNoiseProcessor::RnNoiseProcessor() {
    state_ = rnnoise_create(nullptr);
    if (state_) {
        std::cout << "[RNNOISE] Initialized successfully\n";
    } else {
        std::cerr << "[RNNOISE] Failed to create state\n";
    }
}

RnNoiseProcessor::~RnNoiseProcessor() {
    if (state_) rnnoise_destroy(state_);
}

void RnNoiseProcessor::process(std::vector<int16_t>& frame) {
    if (!state_ || frame.size() != 960) {  // Only support 960-sample frames now
        return;
    }

    // Process in two 480-sample chunks (RNNoise requirement)
    for (int chunk = 0; chunk < 2; ++chunk) {
        size_t offset = chunk * 480;
        float float_frame[480];

        // Convert to float
        for (int i = 0; i < 480; ++i) {
            float_frame[i] = static_cast<float>(frame[offset + i]) / 32768.0f;
        }

        rnnoise_process_frame(state_, float_frame, float_frame);

        // Convert back to int16 with clamp
        for (int i = 0; i < 480; ++i) {
            float val = float_frame[i] * 32768.0f;
            frame[offset + i] = static_cast<int16_t>(std::clamp(val, -32768.0f, 32767.0f));
        }
    }
}