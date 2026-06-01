#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "opus_codec.h"
#include "p2p/rtp_transport.h"
#include "libsodium_wrapper.h"
#include "jitter_buffer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <memory>
#include <array>

#include <winsock2.h>
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <immintrin.h>
#endif

struct AudioDeviceInfo { 
    int index;
    std::string name; 
};

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
    int getPeerPeak(const std::string& peer_id) const;
    bool isRunning() const { return running_.load(); }
    bool echoAvailable() const { return aec_!= nullptr; }
    bool echoEnabled() const { return echo_enabled_.load(); }
    bool isTxMuted() const;

    uint64_t debugPacketsSent() const { return packets_sent_.load(std::memory_order_relaxed); }
    uint64_t debugPacketsRecv() const { return packets_recv_.load(std::memory_order_relaxed); }
    uint64_t debugPacketsDecrypted() const { return packets_decrypted_.load(std::memory_order_relaxed); }

    void setHearTargets(const std::unordered_set<std::string>& hear_ids);

private:
    static constexpr int RATE = 48000;
    static constexpr int FRAME = 960;
    static constexpr size_t CAPTURE_QUEUE_MAX = 16;

    void listenLoop();
    void handleIncomingPacket(const std::vector<uint8_t>& data);
    void sendLoop();
    bool openOutput(); 
    void closeOutput();
    bool openInput(); 
    void closeInput();
    void updateMixedLevel(const std::vector<int16_t>& frame);

    template <typename T, size_t Capacity>
    class LockFreeSpscRingBuffer {
    public:
        static_assert(Capacity >= 2, "Capacity must be at least 2");
        bool push(const T& data) noexcept {
            const size_t head = head_.load(std::memory_order_relaxed);
            const size_t next = (head + 1) % Capacity;
            if (next == tail_.load(std::memory_order_acquire)) {
                return false;
            }
            buffer_[head] = data;
            head_.store(next, std::memory_order_release);
            return true;
        }

        bool pop(T& out) noexcept {
            const size_t tail = tail_.load(std::memory_order_relaxed);
            if (tail == head_.load(std::memory_order_acquire)) {
                return false;
            }
            out = buffer_[tail];
            tail_.store((tail + 1) % Capacity, std::memory_order_release);
            return true;
        }

        void reset() noexcept {
            head_.store(0, std::memory_order_relaxed);
            tail_.store(0, std::memory_order_relaxed);
        }

        bool empty() const noexcept {
            return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
        }

    private:
        static constexpr std::size_t kCacheLine = 64;

        std::array<T, Capacity> buffer_{};
        alignas(kCacheLine) std::atomic<size_t> head_{0};
        alignas(kCacheLine) std::atomic<size_t> tail_{0};
    };

    using CaptureFrame = std::array<int16_t, FRAME>;
    static constexpr size_t CAPTURE_RING_CAPACITY = CAPTURE_QUEUE_MAX + 1;

    bool popCaptureFrame(CaptureFrame& out) noexcept;

    struct StreamState {
        std::string id;
        std::unique_ptr<OpusCodec> decoder;
        JitterBuffer jitter_buffer;
        std::mutex mutex;
        std::atomic<int> peak_pcm{0};
    };

    StreamState* getOrCreateStream(const std::string& id);
    void rebuildStreamSnapshotLocked_();

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

    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> packets_recv_{0};
    std::atomic<uint64_t> packets_decrypted_{0};

    bool pure_opus_ = false;
    std::atomic<float> master_volume_{1.0f};
    std::atomic<float> output_volume_{1.0f};
    std::atomic<float> tx_gain_db_{0.0f};
    std::atomic<int> mic_sensitivity_{45};
    std::atomic<int> noise_suppression_{0};
    std::atomic<bool> noise_suppression_enabled_{false};
    std::atomic<bool> auto_gain_{false};
    std::atomic<bool> echo_enabled_{false};

    std::atomic<bool> pending_aec_update_{false};
    std::atomic<bool> pending_agc_update_{false};
    std::atomic<bool> target_aec_enabled_{false};
    std::atomic<bool> target_agc_enabled_{false};
    std::atomic<bool> tx_muted_{false};

    std::unique_ptr<AecProcessor> aec_;
    std::mutex echo_mutex_;
    std::atomic<int> aec_stream_delay_ms_{180};

    OpusCodec encoder_;
    mutable std::mutex streams_mutex_;
    std::unordered_map<std::string, std::unique_ptr<StreamState>> streams_;
    std::unordered_set<std::string> hear_targets_;

    std::atomic<uint8_t> stream_snapshot_active_{0};
    std::array<std::atomic<uint32_t>, 2> stream_snapshot_readers_{{0u, 0u}};
    std::array<std::vector<StreamState*>, 2> stream_snapshot_buffers_{};

    LockFreeSpscRingBuffer<CaptureFrame, CAPTURE_RING_CAPACITY> capture_frames_;
    std::atomic<uint32_t> capture_dropped_{0};
    uint16_t seq_ = 0;
    uint32_t timestamp_ = 0;

    SOCKET recv_sock_ = INVALID_SOCKET;
    SOCKET send_sock_ = INVALID_SOCKET;
    RTPTransport transport_;
    void* wave_out_ = nullptr;
    void* wave_in_ = nullptr;

    std::vector<float> mix_accum_;
    std::vector<int16_t> mix_frame_;
    std::vector<int16_t> playback_fifo_;
    size_t fifo_read_ = 0;
    size_t fifo_write_ = 0;
    size_t fifo_size_ = 0;
    bool audio_debug_ = false;

    // Kernel notification handles to replace thread-sleep polling
    HANDLE capture_semaphore_ = nullptr;
    std::atomic<bool> is_mic_testing_{false};
};

#endif // AUDIO_ENGINE_H
