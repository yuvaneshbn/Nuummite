#include "webrtc_apm.h"
#include <webrtc-audio-processing-1/modules/audio_processing/include/audio_processing.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

namespace {
template <typename EchoCanceller>
auto enableAec3Fields(EchoCanceller& ec, int)
    -> decltype(ec.aec3.enabled = true, ec.delay_agnostic = true, void()) {
    ec.aec3.enabled = true;
    ec.delay_agnostic = true;
    std::cout << "[WEBRTC] AEC3 + delay_agnostic ENABLED for desktop\n";
}
template <typename EchoCanceller>
void enableAec3Fields(EchoCanceller&, ...) {
    std::cout << "[WEBRTC] Using basic AEC\n";
}
} // namespace

struct WebRtcApm::Impl {
    std::unique_ptr<webrtc::AudioProcessing> apm;
    webrtc::StreamConfig stream_config;
    webrtc::StreamConfig reverse_config;
    bool has_voice = false;
    std::vector<int16_t> reverse_output;
    int stream_delay_ms = 180;  // default for Windows desktop (tune 120-250)
    webrtc::AudioProcessing::Config config;
};

WebRtcApm::WebRtcApm(int sample_rate_hz) {
    impl_ = std::make_unique<Impl>();
    impl_->stream_config = webrtc::StreamConfig(sample_rate_hz, 1);
    impl_->reverse_config = webrtc::StreamConfig(sample_rate_hz, 1);

    webrtc::AudioProcessing::Config config;
    config.echo_canceller.enabled = false;
    enableAec3Fields(config.echo_canceller, 0);
    config.echo_canceller.mobile_mode = false;

    config.high_pass_filter.enabled = true;
    config.noise_suppression.enabled = false;   // RNNoise if needed later
    config.gain_controller1.enabled = false;
    config.gain_controller1.mode = webrtc::AudioProcessing::Config::GainController1::kAdaptiveDigital;

    config.gain_controller2.enabled = false;
    config.gain_controller2.adaptive_digital.enabled = false;
    config.gain_controller2.adaptive_digital.level_estimator = webrtc::AudioProcessing::Config::GainController2::kRms;
    config.voice_detection.enabled = true;

    webrtc::AudioProcessingBuilder builder;
    impl_->apm.reset(builder.Create());

    if (impl_->apm) {
        impl_->config = config;
        impl_->apm->ApplyConfig(config);
        std::cout << "[WEBRTC] Desktop AEC3 ready (delay = " << impl_->stream_delay_ms << " ms)\n";
    } else {
        std::cerr << "[WEBRTC] Failed to create AudioProcessing\n";
    }
}

WebRtcApm::~WebRtcApm() = default;

bool WebRtcApm::available() const { return impl_ && impl_->apm != nullptr; }

void WebRtcApm::set_stream_delay_ms(int delay_ms) {
    if (available()) {
        impl_->stream_delay_ms = std::clamp(delay_ms, 0, 500);
        impl_->apm->set_stream_delay_ms(impl_->stream_delay_ms);
    }
}

bool WebRtcApm::process_render(const int16_t* frame, int samples) {
    if (!available()) return false;
    if (impl_->reverse_output.size() < static_cast<size_t>(samples)) {
        impl_->reverse_output.resize(static_cast<size_t>(samples));
    }
    int err = impl_->apm->ProcessReverseStream(frame, impl_->reverse_config, impl_->reverse_config, impl_->reverse_output.data());
    return err == webrtc::AudioProcessing::kNoError;
}

bool WebRtcApm::process_capture(std::vector<int16_t>& frame) {
    if (!available() || frame.size() != 960) return false;
    impl_->apm->set_stream_delay_ms(impl_->stream_delay_ms);  // critical - call every frame
    int err = impl_->apm->ProcessStream(frame.data(), impl_->stream_config, impl_->stream_config, frame.data());
    if (err == webrtc::AudioProcessing::kNoError) {
        const auto stats = impl_->apm->GetStatistics();
        impl_->has_voice = stats.voice_detected.value_or(false);
        return true;
    }
    return false;
}

bool WebRtcApm::hasVoice() const { return impl_ ? impl_->has_voice : false; }
void WebRtcApm::setEchoEnabled(bool enabled) {
    if (!available()) {
        return;
    }
    impl_->config.echo_canceller.enabled = enabled;
    impl_->apm->ApplyConfig(impl_->config);
}

void WebRtcApm::setAutoGainEnabled(bool enabled) {
    if (!available()) {
        return;
    }

    // Use GainController2 only (avoid stacking GC1 + GC2).
    impl_->config.gain_controller1.enabled = false;
    impl_->config.gain_controller2.enabled = enabled;
    impl_->config.gain_controller2.adaptive_digital.enabled = enabled;
    impl_->apm->ApplyConfig(impl_->config);
}
