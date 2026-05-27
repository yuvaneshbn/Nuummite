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
    std::cout << " AEC3 and delay-agnostic routing enabled\n";
}
template <typename EchoCanceller>
void enableAec3Fields(EchoCanceller&,...) {
    std::cout << " System falling back to basic AEC\n";
}
} // namespace

struct WebRtcApm::Impl {
    std::unique_ptr<webrtc::AudioProcessing> apm;
    webrtc::StreamConfig stream_config;
    webrtc::StreamConfig reverse_config;
    bool has_voice = false;
    std::vector<int16_t> reverse_output; // Preallocated real-time safe buffer
    int stream_delay_ms = 180;
    webrtc::AudioProcessing::Config config;
};

WebRtcApm::WebRtcApm(int sample_rate_hz) {
    impl_ = std::make_unique<Impl>();
    impl_->stream_config = webrtc::StreamConfig(sample_rate_hz, 1);
    impl_->reverse_config = webrtc::StreamConfig(sample_rate_hz, 1);

    // Preallocate vector memory to prevent dynamic allocations on the real-time thread.
    // Assigning 4800 samples provides ample headroom for processing up to 100ms frames at 48kHz.
    impl_->reverse_output.assign(4800, 0); 

    webrtc::AudioProcessing::Config config;
    config.echo_canceller.enabled = false;
    enableAec3Fields(config.echo_canceller, 0);
    config.echo_canceller.mobile_mode = false;

    config.high_pass_filter.enabled = true;
    config.noise_suppression.enabled = false;
    
    config.gain_controller1.enabled = false;
    config.gain_controller1.mode = webrtc::AudioProcessing::Config::GainController1::kAdaptiveDigital;

    config.gain_controller2.enabled = false;
    config.gain_controller2.adaptive_digital.enabled = false;
    config.gain_controller2.adaptive_digital.level_estimator = webrtc::AudioProcessing::Config::GainController2::kRms;

    webrtc::AudioProcessingBuilder builder;
    impl_->apm.reset(builder.Create());

    if (impl_->apm) {
        impl_->config = config;
        impl_->apm->ApplyConfig(config);
        std::cout << " Acoustic processing unit configured safely\n";
    } else {
        std::cerr << " Failed to instantiate AudioProcessing module\n";
    }
}

WebRtcApm::~WebRtcApm() = default;

bool WebRtcApm::available() const { 
    return impl_ && impl_->apm!= nullptr;
}

void WebRtcApm::set_stream_delay_ms(int delay_ms) {
    if (available()) {
        impl_->stream_delay_ms = std::clamp(delay_ms, 0, 500);
        impl_->apm->set_stream_delay_ms(impl_->stream_delay_ms);
    }
}

bool WebRtcApm::process_render(const int16_t* frame, int samples) {
    if (!available()) return false;
    if (!frame || samples <= 0) {
        return false;
    }

    constexpr int kChunk = 480;
    if (samples % kChunk!= 0) {
        return false;
    }

    // Safety assert to ensure we do not overrun the preallocated rendering buffer.
    if (impl_->reverse_output.size() < static_cast<size_t>(samples)) {
        return false; 
    }

    for (int off = 0; off < samples; off += kChunk) {
        // Run processing using the preallocated vector to avoid allocations on the render thread.
        const int err = impl_->apm->ProcessReverseStream(
            frame + off, 
            impl_->reverse_config, 
            impl_->reverse_config, 
            impl_->reverse_output.data() + off
        );
        if (err!= webrtc::AudioProcessing::kNoError) {
            return false;
        }
    }

    return true;
}

bool WebRtcApm::process_capture(std::vector<int16_t>& frame) {
    if (!available() || frame.empty()) {
        return false;
    }
    constexpr int kChunk = 480;
    if (frame.size() % kChunk!= 0) {
        return false;
    }

    bool ok = true;
    for (size_t off = 0; off < frame.size(); off += kChunk) {
        impl_->apm->set_stream_delay_ms(impl_->stream_delay_ms);
        const int err = impl_->apm->ProcessStream(
            frame.data() + off, 
            impl_->stream_config, 
            impl_->stream_config, 
            frame.data() + off
        );
        if (err!= webrtc::AudioProcessing::kNoError) {
            ok = false;
            break;
        }
    }

    if (ok) {
        impl_->has_voice = true;
        return true;
    }
    return false;
}

bool WebRtcApm::hasVoice() const { 
    return impl_? impl_->has_voice : false; 
}

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
    impl_->config.gain_controller1.enabled = false;
    impl_->config.gain_controller2.enabled = enabled;
    impl_->config.gain_controller2.adaptive_digital.enabled = enabled;
    impl_->apm->ApplyConfig(impl_->config);
}
