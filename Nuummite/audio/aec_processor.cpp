#include "aec_processor.h"

#include <modules/audio_processing/include/audio_processing.h>

#include <algorithm>
#include <cstring>
#include <iostream>

AecProcessor::AecProcessor(int sample_rate_hz, int frame_samples)
    : sample_rate_hz_(sample_rate_hz), frame_samples_(frame_samples) {
    webrtc::AudioProcessing::Config cfg;
    cfg.echo_canceller.enabled = true;
    cfg.echo_canceller.mobile_mode = false;
    cfg.echo_canceller.export_linear_aec_output = false;

    ap_ = webrtc::AudioProcessingBuilder().Create();
    if (ap_) {
        ap_->ApplyConfig(cfg);
    } else {
        std::cerr << "[AEC] Failed to create WebRTC AudioProcessing\n";
    }
}

AecProcessor::~AecProcessor() = default;

void AecProcessor::set_delay_ms(int delay_ms) {
    stream_delay_ms_ = std::max(0, delay_ms);
}

bool AecProcessor::process_render(const int16_t* frame, int samples) {
    if (!ap_ || !frame || samples != frame_samples_) {
        return false;
    }
    const webrtc::StreamConfig cfg(sample_rate_hz_, 1);
    std::vector<int16_t> scratch(frame_samples_);
    std::memcpy(scratch.data(), frame, scratch.size() * sizeof(int16_t));
    return ap_->ProcessReverseStream(frame, cfg, cfg, scratch.data()) ==
           webrtc::AudioProcessing::kNoError;
}

bool AecProcessor::process_capture(std::vector<int16_t>& frame) {
    if (!ap_ || frame.size() != static_cast<size_t>(frame_samples_)) {
        return false;
    }
    const webrtc::StreamConfig cfg(sample_rate_hz_, 1);
    std::vector<int16_t> scratch(frame);
    ap_->set_stream_delay_ms(stream_delay_ms_);
    if (ap_->ProcessStream(frame.data(), cfg, cfg, scratch.data()) !=
        webrtc::AudioProcessing::kNoError) {
        return false;
    }
    frame.swap(scratch);
    return true;
}
