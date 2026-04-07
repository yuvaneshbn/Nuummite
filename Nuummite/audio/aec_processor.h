#ifndef CLIENT_AUDIO_AEC_PROCESSOR_H
#define CLIENT_AUDIO_AEC_PROCESSOR_H

#include <vector>
#include <cstdint>

class AecProcessor {
public:
    AecProcessor(int sample_rate_hz, int frame_samples);
    ~AecProcessor();

    bool available() const { return true; }
    void set_delay_ms(int delay_ms);
    bool process_render(const int16_t* frame, int samples);
    bool process_capture(std::vector<int16_t>& frame);

    bool hasVoice() const { return voice_detected_this_frame_; }

private:
    int sample_rate_;
    int frame_size_;
    bool voice_detected_this_frame_ = false;

    int silence_frames_ = 0;
    static constexpr int VAD_THRESHOLD = 1800;   // tuned for voice
    static constexpr int VAD_HANGOVER_FRAMES = 15;
};

#endif
