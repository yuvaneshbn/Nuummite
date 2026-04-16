#include "aec_processor.h"
#include "webrtc_apm.h"

#include <memory>

AecProcessor::AecProcessor(int sample_rate_hz, int /*frame_samples*/)
    : apm_(std::make_unique<WebRtcApm>(sample_rate_hz)) {}

AecProcessor::~AecProcessor() = default;

void AecProcessor::set_delay_ms(int /*delay_ms*/) {
    // WebRTC APM handles delay internally; no-op here.
}

void AecProcessor::set_stream_delay_ms(int delay_ms) {
    if (apm_) {
        apm_->set_stream_delay_ms(delay_ms);
    }
}

void AecProcessor::setEchoEnabled(bool enabled) {
    if (apm_) {
        apm_->setEchoEnabled(enabled);
    }
}

bool AecProcessor::process_render(const int16_t* frame, int samples) {
    return apm_ ? apm_->process_render(frame, samples) : true;
}

bool AecProcessor::process_capture(std::vector<int16_t>& frame) {
    return apm_ ? apm_->process_capture(frame) : false;
}

bool AecProcessor::hasVoice() const {
    return apm_ ? apm_->hasVoice() : false;
}

bool AecProcessor::available() const {
    return apm_ && apm_->available();
}
