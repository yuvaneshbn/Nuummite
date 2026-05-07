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
    if (!state_) {
        return;
    }

    // RNNoise requires 10ms frames: 480 samples at 48kHz.
    if (frame.size() == 480) {
        processBlock(frame.data(), 480);
        return;
    }

    // 20ms frame: process as two 10ms blocks.
    if (frame.size() == 960) {
        processBlock(frame.data(), 480);
        processBlock(frame.data() + 480, 480);
        return;
    }
}

void RnNoiseProcessor::processBlock(int16_t* samples, int sample_count) {
    if (!state_ || !samples || sample_count != 480) {
        return;
    }

    float float_frame[480];

    // Convert to float
    for (int i = 0; i < 480; ++i) {
        float_frame[i] = static_cast<float>(samples[i]) / 32768.0f;
    }

    rnnoise_process_frame(state_, float_frame, float_frame);

    // Convert back to int16 with clamp
    for (int i = 0; i < 480; ++i) {
        float val = float_frame[i] * 32768.0f;
        samples[i] = static_cast<int16_t>(std::clamp(val, -32768.0f, 32767.0f));
    }
}
