#ifndef CLIENT_AUDIO_AEC_PROCESSOR_H
#define CLIENT_AUDIO_AEC_PROCESSOR_H

#include <vector>
#include <cstdint>

#if VOICE_WITH_WEBRTC
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc { class AudioProcessing; }

class AecProcessor {
public:
    AecProcessor(int sample_rate_hz, int frame_samples);
    ~AecProcessor();

    bool available() const { return ap_ != nullptr; }
    void set_delay_ms(int delay_ms) { stream_delay_ms_ = std::max(0, delay_ms); }
    bool process_render(const int16_t* frame, int samples);
    bool process_capture(std::vector<int16_t>& frame);

private:
    int sample_rate_hz_;
    int frame_samples_;
    int stream_delay_ms_ = 40;       // Fixed for local mixing
    rtc::scoped_refptr<webrtc::AudioProcessing> ap_;
};
#else
class AecProcessor {
public:
    AecProcessor(int, int) {}
    ~AecProcessor() = default;
    bool available() const { return false; }
    void set_delay_ms(int) {}
    bool process_render(const int16_t*, int) { return false; }
    bool process_capture(std::vector<int16_t>&) { return false; }
};
#endif

#endif
