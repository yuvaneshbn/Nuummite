#ifndef CLIENT_AUDIO_AEC_PROCESSOR_H
#define CLIENT_AUDIO_AEC_PROCESSOR_H

#include <vector>
#include <cstdint>
#include <memory>

class WebRtcApm;

class AecProcessor {
public:
    AecProcessor(int sample_rate_hz, int frame_samples);
    ~AecProcessor();

    void set_delay_ms(int delay_ms);
    void set_stream_delay_ms(int delay_ms);
    void setEchoEnabled(bool enabled);
    void setAutoGainEnabled(bool enabled);
    bool process_render(const int16_t* frame, int samples);
    bool process_capture(std::vector<int16_t>& frame);

    bool hasVoice() const;
    bool available() const;

private:
    std::unique_ptr<WebRtcApm> apm_;
};

#endif
