#ifndef WEBRTC_APM_H
#define WEBRTC_APM_H

#include <vector>
#include <cstdint>
#include <memory>

class WebRtcApm {
public:
    explicit WebRtcApm(int sample_rate_hz = 48000);
    ~WebRtcApm();

    bool available() const;
    bool process_render(const int16_t* frame, int samples);   // far-end (playback)
    bool process_capture(std::vector<int16_t>& frame);        // near-end (mic)
    bool hasVoice() const;                                    // VAD result
    void setEchoEnabled(bool enabled);
    void set_stream_delay_ms(int delay_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
