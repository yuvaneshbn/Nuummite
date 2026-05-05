#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "opus_codec.h"
#include "p2p/rtp_transport.h"
#include "libsodium_wrapper.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <memory>

#include <winsock2.h>

struct AudioDeviceInfo { int index; std::string name; };

class AecProcessor;
class RnNoiseProcessor;

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool start(const std::vector<std::string>& destinations);
    bool updateDestinations(const std::vector<std::string>& destinations);
    void stop();
    void shutdown();

    int port() const { return port_; }
    void setClientId(const std::string& id) { client_id_ = id; }
    void setRoomSecret(const std::string& secret) { SodiumWrapper::setKey(secret); }
    const std::string& clientId() const { return client_id_; }

    std::vector<AudioDeviceInfo> listInputDevices() const;
    std::vector<AudioDeviceInfo> listOutputDevices() const;
    bool setInputDevice(int device_index);
    bool setOutputDevice(int device_index);
    int inputDeviceIndex() const { return input_device_index_; }
    int outputDeviceIndex() const { return output_device_index_; }

    void setMasterVolume(int value);
    void setOutputVolume(int value);
    void setGainDb(int value);
    void setMicSensitivity(int value);
    void setNoiseSuppression(int value);
    void setNoiseSuppressionEnabled(bool enabled);
    void setAutoGain(bool enabled);
    void setEchoEnabled(bool enabled);
    void setAecStreamDelayMs(int delay_ms);
    void setTxMuted(bool enabled);

    int testMicrophoneLevel(double duration_sec = 1.0);

    void pushCaptureFrame(const int16_t* samples, int sample_count);
    void renderOutput(int16_t* out, int sample_count);

    int captureLevel() const { return capture_level_.load(); }
    bool captureActive() const { return capture_active_.load(); }
    float mixedPeak() const { return mixed_peak_.load(); }
    bool isRunning() const { return running_.load(); }
    bool echoAvailable() const { return aec_ != nullptr; }
    bool echoEnabled() const { return echo_enabled_.load(); }
    bool isTxMuted() const;

    // Local hear list for mixing
    void setHearTargets(const std::unordered_set<std::string>& hear_ids);

private:
    static constexpr int RATE = 48000;
    static constexpr int FRAME = 960; // 20 ms @ 48 kHz

    void listenLoop();
    void handleIncomingPacket(const std::vector<uint8_t>& data);
    void sendLoop();
    bool openOutput(); void closeOutput();
    bool openInput(); void closeInput();
    void updateMixedLevel(const std::vector<int16_t>& frame);
    bool popCaptureFrame(std::vector<int16_t>& out);

    struct StreamState {
        std::unique_ptr<OpusCodec> decoder;
        std::deque<std::vector<int16_t>> jitter;
        uint16_t last_seq = 0;
        bool has_seq = false;
    };

    StreamState& getStream(const std::string& id) {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        auto& s = streams_[id];

        if (!s.decoder) {
            s.decoder = std::make_unique<OpusCodec>(
                RATE, 1, FRAME,
                true, 10, 24000, 10,
                false,
                OPUS_APPLICATION_VOIP,
                false, true
            );
        }
        return s;
    }

    int port_ = 0;
    std::string client_id_;
    std::vector<std::string> send_destinations_;
    mutable std::mutex routing_mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> listen_running_{true};
    std::atomic<bool> playback_running_{false};

    std::thread listen_thread_;
    std::thread send_thread_;

    std::unique_ptr<RnNoiseProcessor> rnnoise_;
    
    int input_device_index_ = -1;
    int output_device_index_ = -1;

    std::atomic<int> capture_level_{0};
    std::atomic<bool> capture_active_{false};
    std::atomic<float> mixed_peak_{0.0f};

    // config
    bool pure_opus_ = true;         // debug: bypass all DSP (AEC/AGC/RNNoise/VAD/manual gain)
    float master_volume_ = 1.0f;   // default master volume
    float output_volume_ = 1.0f;
    float tx_gain_db_ = 0.0f;
    int mic_sensitivity_ = 45;     // default input sensitivity
    int noise_suppression_ = 0;
    bool noise_suppression_enabled_ = false;   // testing: keep RNNoise off while tuning clarity
    bool auto_gain_ = false;
    std::atomic<bool> echo_enabled_{false};
    bool tx_muted_ = false;
    mutable std::mutex config_mutex_;

    std::unique_ptr<AecProcessor> aec_;
    std::mutex echo_mutex_;
    std::atomic<int> aec_stream_delay_ms_{180};

    OpusCodec encoder_;
    std::mutex streams_mutex_;
    std::unordered_map<std::string, StreamState> streams_;
    std::unordered_set<std::string> hear_targets_;

    std::deque<std::vector<int16_t>> capture_frames_;
    std::mutex capture_mutex_;
    std::condition_variable capture_cv_;

    uint16_t seq_ = 0;
    uint32_t timestamp_ = 0;

    SOCKET recv_sock_ = INVALID_SOCKET;
    SOCKET send_sock_ = INVALID_SOCKET;
    RTPTransport transport_;

    void* wave_out_ = nullptr;
    void* wave_in_ = nullptr;

    // Output callback must be real-time friendly: avoid allocations and blocking locks.
    // We render fixed 20ms (960-sample) mix frames into a small FIFO and then satisfy
    // whatever frame_count PortAudio asks for.
    std::vector<float> mix_accum_;
    std::vector<int16_t> mix_frame_;
    std::vector<int16_t> playback_fifo_;
    size_t fifo_read_ = 0;
    size_t fifo_write_ = 0;
    size_t fifo_size_ = 0;
};

#endif
