#include "aec_processor.h"
#include <algorithm>
#include <cmath>

AecProcessor::AecProcessor(int sample_rate_hz, int frame_samples)
    : sample_rate_(sample_rate_hz), frame_size_(frame_samples) {}

AecProcessor::~AecProcessor() = default;

void AecProcessor::set_delay_ms(int) { /* not used in simple version */ }

bool AecProcessor::process_render(const int16_t*, int) {
    return true;
}

bool AecProcessor::process_capture(std::vector<int16_t>& frame) {
    if (frame.size() != static_cast<size_t>(frame_size_)) {
        return false;
    }

    // Improved VAD (energy + hangover)
    int peak = 0;
    for (const auto sample : frame) {
        peak = std::max(peak, std::abs(static_cast<int>(sample)));
    }

    bool voice = peak > VAD_THRESHOLD;

    if (voice) {
        silence_frames_ = 0;
        voice_detected_this_frame_ = true;
    } else {
        silence_frames_++;
        if (silence_frames_ > VAD_HANGOVER_FRAMES) {
            voice_detected_this_frame_ = false;
        }
        // else keep voice_detected = true (hangover)
    }

    // TODO: When you compile WebRTC APM as DLL, replace this with real AEC3 here
    // For now we have a usable VAD that removes the crude "any non-zero" logic.
    return true;
}
