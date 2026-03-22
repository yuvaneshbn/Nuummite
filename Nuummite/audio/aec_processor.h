#ifndef CLIENT_AUDIO_AEC_PROCESSOR_H
#define CLIENT_AUDIO_AEC_PROCESSOR_H

#include <vector>
#include <cstdint>

// Stub implementation: echo cancellation disabled; keeps interface intact.
class AecProcessor {
public:
    AecProcessor(int sample_rate_hz, int frame_samples);
    ~AecProcessor();

    bool available() const { return false; }
    void set_delay_ms(int delay_ms);
    bool process_render(const int16_t* frame, int samples);
    bool process_capture(std::vector<int16_t>& frame);
};

#endif
