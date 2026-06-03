#include "webrtc_apm.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

#include "webrtc-audio-processing-1/modules/audio_processing/include/audio_processing.h"

namespace {

constexpr int kDefaultAnalogLevel = 127;

bool isSpeechLike(const int16_t* data, size_t samples) {
    if (!data || samples == 0) {
        return false;
    }

    int peak = 0;
    double sum = 0.0;
    for (size_t i = 0; i < samples; ++i) {
        const int v = std::abs(static_cast<int>(data[i]));
        peak = std::max(peak, v);
        sum += static_cast<double>(v) * static_cast<double>(v);
    }

    const double rms = std::sqrt(sum / static_cast<double>(samples));
    return peak >= 250 || rms >= 110.0;
}

} // namespace

struct WebRtcApm::Impl {
    explicit Impl(int sample_rate_hz)
        : sample_rate_hz(sample_rate_hz),
          stream_config(sample_rate_hz, 1, false) {}

    void buildConfig() {
        config = webrtc::AudioProcessing::Config{};
        config.pipeline.maximum_internal_processing_rate = sample_rate_hz;
        config.pipeline.multi_channel_render = false;
        config.pipeline.multi_channel_capture = false;
        config.high_pass_filter.enabled = true;
        config.echo_canceller.enabled = echo_enabled;
        config.echo_canceller.enforce_high_pass_filtering = true;
        config.noise_suppression.enabled = true;
        config.noise_suppression.level = noise_suppression_level;
        config.voice_detection.enabled = true;
        config.gain_controller1.enabled = auto_gain_enabled;
        config.gain_controller1.mode = webrtc::AudioProcessing::Config::GainController1::kAdaptiveDigital;
        config.gain_controller1.enable_limiter = true;
        config.gain_controller1.analog_level_minimum = 0;
        config.gain_controller1.analog_level_maximum = 255;
        config.gain_controller1.analog_gain_controller.enabled = auto_gain_enabled;
        config.gain_controller1.analog_gain_controller.startup_min_volume = 85;
        config.gain_controller2.enabled = false;
        config.residual_echo_detector.enabled = echo_enabled;
        config.level_estimation.enabled = true;
    }

    bool initialize() {
        apm.reset(webrtc::AudioProcessingBuilder().Create());
        if (!apm) {
            std::cerr << "[WebRtcApm] AudioProcessingBuilder::Create() failed\n";
            return false;
        }

        webrtc::ProcessingConfig processing_config;
        processing_config.input_stream() = stream_config;
        processing_config.output_stream() = stream_config;
        processing_config.reverse_input_stream() = stream_config;
        processing_config.reverse_output_stream() = stream_config;

        if (apm->Initialize(processing_config) != 0) {
            std::cerr << "[WebRtcApm] Initialize(processing_config) failed\n";
            apm.reset();
            return false;
        }

        buildConfig();
        apm->ApplyConfig(config);
        initialized = true;
        return true;
    }

    void applyConfig() {
        if (!apm) {
            return;
        }
        buildConfig();
        apm->ApplyConfig(config);
    }

    int sample_rate_hz = 48000;
    bool echo_enabled = false;
    bool auto_gain_enabled = false;
    int stream_delay_ms = 0;
    bool voice_detected = false;
    int analog_level = kDefaultAnalogLevel;
    bool initialized = false;
    webrtc::AudioProcessing::Config config;
    webrtc::StreamConfig stream_config;
    std::unique_ptr<webrtc::AudioProcessing> apm;
    webrtc::AudioProcessing::Config::NoiseSuppression::Level noise_suppression_level =
        webrtc::AudioProcessing::Config::NoiseSuppression::kModerate;
};

WebRtcApm::WebRtcApm(int sample_rate_hz)
    : impl_(std::make_unique<Impl>(sample_rate_hz)) {
    if (impl_) {
        impl_->initialize();
    }
}

WebRtcApm::~WebRtcApm() = default;

bool WebRtcApm::available() const {
    return impl_ && impl_->initialized && impl_->apm;
}

bool WebRtcApm::process_render(const int16_t* frame, int samples) {
    if (!available() || !impl_->echo_enabled) {
        return true;
    }
    if (!frame || samples <= 0) {
        return false;
    }

    impl_->stream_config.set_sample_rate_hz(impl_->sample_rate_hz);
    impl_->stream_config.set_num_channels(1);

    const int rc = impl_->apm->ProcessReverseStream(frame,
                                                    impl_->stream_config,
                                                    impl_->stream_config,
                                                    const_cast<int16_t*>(frame));
    return rc == 0;
}

bool WebRtcApm::process_capture(std::vector<int16_t>& frame) {
    if (frame.empty()) {
        impl_->voice_detected = false;
        return true;
    }

    impl_->voice_detected = isSpeechLike(frame.data(), frame.size());

    if (!available()) {
        return true;
    }

    impl_->stream_config.set_sample_rate_hz(impl_->sample_rate_hz);
    impl_->stream_config.set_num_channels(1);

    impl_->apm->set_stream_delay_ms(std::max(0, impl_->stream_delay_ms));

    if (impl_->auto_gain_enabled) {
        impl_->apm->set_stream_analog_level(impl_->analog_level);
    }

    const int rc = impl_->apm->ProcessStream(frame.data(),
                                             impl_->stream_config,
                                             impl_->stream_config,
                                             frame.data());
    if (rc != 0) {
        return false;
    }

    if (impl_->auto_gain_enabled) {
        impl_->analog_level = impl_->apm->recommended_stream_analog_level();
    }

    return true;
}

bool WebRtcApm::hasVoice() const {
    return impl_ ? impl_->voice_detected : false;
}

void WebRtcApm::setEchoEnabled(bool enabled) {
    if (!impl_) {
        return;
    }
    impl_->echo_enabled = enabled;
    impl_->applyConfig();
}

void WebRtcApm::setAutoGainEnabled(bool enabled) {
    if (!impl_) {
        return;
    }
    impl_->auto_gain_enabled = enabled;
    impl_->applyConfig();
}

void WebRtcApm::set_stream_delay_ms(int delay_ms) {
    if (impl_) {
        impl_->stream_delay_ms = delay_ms;
    }
}
