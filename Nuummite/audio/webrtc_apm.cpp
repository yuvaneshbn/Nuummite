// Refactored Nuummite\audio\webrtc_apm.cpp - Safe APM configuration management

#include "webrtc_apm.h"
#include <webrtc-audio-processing-1/modules/audio_processing/include/audio_processing.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>

struct WebRtcApm::Impl {
    std::unique_ptr<webrtc::AudioProcessing> apm;
    webrtc::StreamConfig stream_config;
    webrtc::StreamConfig reverse_config;
    bool has_voice = false;
    std::vector<int16_t> reverse_output;
    int stream_delay_ms = 180;
    webrtc::AudioProcessing::Config config;

    // Split locks: render and capture can progress independently.
    std::mutex render_mutex;
    std::mutex capture_mutex;
    std::mutex config_mutex;
};

WebRtcApm::WebRtcApm(int sample_rate_hz) {
    impl_ = std::make_unique<Impl>();
    impl_->stream_config = webrtc::StreamConfig(sample_rate_hz, 1);
    impl_->reverse_config = webrtc::StreamConfig(sample_rate_hz, 1);
    impl_->reverse_output.assign(4800, 0); 

    webrtc::AudioProcessing::Config config;
    config.echo_canceller.enabled = false;
    config.echo_canceller.mobile_mode = false;
    config.high_pass_filter.enabled = true;
    config.noise_suppression.enabled = false;
    config.gain_controller1.enabled = false;
    config.gain_controller2.enabled = false;
    config.voice_detection.enabled = true;

    webrtc::AudioProcessingBuilder builder;
    impl_->apm.reset(builder.Create());
    if (impl_->apm) {
        impl_->config = config;
        impl_->apm->ApplyConfig(config);
        std::cout << " Acoustic processing unit configured safely\n";
    }
}

WebRtcApm::~WebRtcApm() = default;

bool WebRtcApm::available() const { 
    return impl_ && impl_->apm!= nullptr;
}

void WebRtcApm::set_stream_delay_ms(int delay_ms) {
    if (!available()) return;
    std::lock_guard<std::mutex> lock(impl_->config_mutex);
    impl_->stream_delay_ms = std::clamp(delay_ms, 0, 500);
}

bool WebRtcApm::process_render(const int16_t* frame, int samples) {
    if (!available() ||!frame || samples <= 0) return false;
    constexpr int kChunk = 480;
    if (samples % kChunk!= 0) return false;
    if (impl_->reverse_output.size() < static_cast<size_t>(samples)) return false;

    std::lock_guard<std::mutex> lock(impl_->render_mutex);
    for (int off = 0; off < samples; off += kChunk) {
        const int err = impl_->apm->ProcessReverseStream(
            frame + off, 
            impl_->reverse_config, 
            impl_->reverse_config, 
            impl_->reverse_output.data() + off
        );
        if (err!= webrtc::AudioProcessing::kNoError) return false;
    }
    return true;
}

bool WebRtcApm::process_capture(std::vector<int16_t>& frame) {
    if (!available() || frame.empty()) return false;
    constexpr int kChunk = 480;
    if (frame.size() % kChunk!= 0) return false;

    std::lock_guard<std::mutex> lock(impl_->capture_mutex);
    impl_->has_voice = false;
    bool ok = true;

    int delay_local = 180;
    {
        std::lock_guard<std::mutex> c_lock(impl_->config_mutex);
        delay_local = impl_->stream_delay_ms;
    }

    for (size_t off = 0; off < frame.size(); off += kChunk) {
        impl_->apm->set_stream_delay_ms(delay_local);
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
        auto stats = impl_->apm->GetStatistics();
        impl_->has_voice = stats.voice_detected.value_or(false);
        return true;
    }
    return false;
}

bool WebRtcApm::hasVoice() const {
    if (!available()) return false;
    std::lock_guard<std::mutex> lock(impl_->capture_mutex);
    return impl_->has_voice;
}

void WebRtcApm::setEchoEnabled(bool enabled) {
    if (!available()) return;
    std::lock_guard<std::mutex> lock(impl_->config_mutex);
    if (impl_->config.echo_canceller.enabled == enabled) return;

    impl_->config.echo_canceller.enabled = enabled;
    std::scoped_lock processing_lock(impl_->render_mutex, impl_->capture_mutex);
    impl_->apm->ApplyConfig(impl_->config);
}

void WebRtcApm::setAutoGainEnabled(bool enabled) {
    if (!available()) return;
    std::lock_guard<std::mutex> lock(impl_->config_mutex);
    if (impl_->config.gain_controller2.enabled == enabled) return;

    impl_->config.gain_controller1.enabled = false;
    impl_->config.gain_controller2.enabled = enabled;
    impl_->config.gain_controller2.adaptive_digital.enabled = enabled;
    std::scoped_lock processing_lock(impl_->render_mutex, impl_->capture_mutex);
    impl_->apm->ApplyConfig(impl_->config);
}
