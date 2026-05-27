#include "audio_engine.h"
#include "audio_packet.h"
#include "socket_utils.h"
#include "portaudio_dyn.h"
#include "aec_processor.h"
#include "rnnoise_processor.h"
#include "winsock_init.h"
#include "opus.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr int RATE = 48000;
constexpr int FRAME = 960; 
constexpr int FRAME_BYTES = FRAME * 2;
constexpr int AUDIO_PORT = 50002;
constexpr int DSCP_EF = 46;
constexpr int IP_TOS_EF = DSCP_EF << 2;
constexpr int RX_QUEUE_MAX_FRAMES = 8;
constexpr const char* DEFAULT_PORTAUDIO_DLL = "libportaudio.dll";
static WinSockInit g_wsa;

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string exeDir() {
    char path[MAX_PATH] = {0};
    const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return ".";
    }
    std::string full(path, path + len);
    const size_t slash = full.find_last_of("\\/");
    if (slash == std::string::npos) {
        return ".";
    }
    return full.substr(0, slash);
}

HMODULE SecureLoadPortAudioLibrary(const std::string& full_path) {
    const bool has_dir = (full_path.find('\\') != std::string::npos) || (full_path.find('/') != std::string::npos);
    DWORD flags =
        LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
        LOAD_LIBRARY_SEARCH_SYSTEM32 |
        LOAD_LIBRARY_SEARCH_USER_DIRS;
    if (has_dir) {
        flags |= LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR;
    }

    HMODULE h_module = LoadLibraryExA(full_path.c_str(), nullptr, flags);
    if (!h_module && GetLastError() == ERROR_INVALID_PARAMETER) {
        // Older systems may not support LOAD_LIBRARY_SEARCH_* flags.
        // This still uses an explicit (or app-dir) path for the target DLL.
        if (has_dir) {
            h_module = LoadLibraryA(full_path.c_str());
        }
    }
    return h_module;
}

struct PortAudioApi {
    using Pa_Initialize_Fn = PaError (*)();
    using Pa_Terminate_Fn = PaError (*)();
    using Pa_GetErrorText_Fn = const char* (*)(PaError);
    using Pa_GetDeviceCount_Fn = PaDeviceIndex (*)();
    using Pa_GetDefaultInputDevice_Fn = PaDeviceIndex (*)();
    using Pa_GetDefaultOutputDevice_Fn = PaDeviceIndex (*)();
    using Pa_GetDeviceInfo_Fn = const PaDeviceInfo* (*)(PaDeviceIndex);
    using Pa_GetHostApiInfo_Fn = const PaHostApiInfo* (*)(PaHostApiIndex);
    using Pa_OpenStream_Fn = PaError (*)(PaStream**, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long, PaStreamFlags, PaStreamCallback*, void*);
    using Pa_CloseStream_Fn = PaError (*)(PaStream*);
    using Pa_StartStream_Fn = PaError (*)(PaStream*);
    using Pa_StopStream_Fn = PaError (*)(PaStream*);
    using Pa_ReadStream_Fn = PaError (*)(PaStream*, void*, unsigned long);

    HMODULE module = nullptr;
    Pa_Initialize_Fn Initialize = nullptr;
    Pa_Terminate_Fn Terminate = nullptr;
    Pa_GetErrorText_Fn GetErrorText = nullptr;
    Pa_GetDeviceCount_Fn GetDeviceCount = nullptr;
    Pa_GetDefaultInputDevice_Fn GetDefaultInputDevice = nullptr;
    Pa_GetDefaultOutputDevice_Fn GetDefaultOutputDevice = nullptr;
    Pa_GetDeviceInfo_Fn GetDeviceInfo = nullptr;
    Pa_GetHostApiInfo_Fn GetHostApiInfo = nullptr;
    Pa_OpenStream_Fn OpenStream = nullptr;
    Pa_CloseStream_Fn CloseStream = nullptr;
    Pa_StartStream_Fn StartStream = nullptr;
    Pa_StopStream_Fn StopStream = nullptr;
    Pa_ReadStream_Fn ReadStream = nullptr;

    bool load(std::string& error) {
        if (module) return true;

        auto fail = [&](const std::string& msg) -> bool {
            error = msg;
            unload();
            Initialize = nullptr;
            Terminate = nullptr;
            GetErrorText = nullptr;
            GetDeviceCount = nullptr;
            GetDefaultInputDevice = nullptr;
            GetDefaultOutputDevice = nullptr;
            GetDeviceInfo = nullptr;
            GetHostApiInfo = nullptr;
            OpenStream = nullptr;
            CloseStream = nullptr;
            StartStream = nullptr;
            StopStream = nullptr;
            ReadStream = nullptr;
            return false;
        };

        std::vector<std::string> candidates = {
            exeDir() + "\\" + DEFAULT_PORTAUDIO_DLL,
            exeDir() + "\\_internal\\" + DEFAULT_PORTAUDIO_DLL,
            exeDir() + "\\_internal\\third_party\\libportaudio\\" + DEFAULT_PORTAUDIO_DLL,
            exeDir() + "\\third_party\\libportaudio\\" + DEFAULT_PORTAUDIO_DLL,
        };
        for (const auto& candidate : candidates) {
            module = SecureLoadPortAudioLibrary(candidate);
            if (module) break;
        }
        if (!module) {
            // Safe fallback: restrict search to application dir + system32 (no CWD / PATH probing).
            module = SecureLoadPortAudioLibrary(DEFAULT_PORTAUDIO_DLL);
        }
        if (!module) {
            return fail("Could not securely resolve libportaudio.dll dependencies");
        }
        auto load_symbol = [this, &error](auto& fn, const char* name) -> bool {
            fn = reinterpret_cast<std::decay_t<decltype(fn)>>(GetProcAddress(module, name));
            if (!fn) {
                error = std::string("Missing dynamic entry: ") + name;
                return false;
            }
            return true;
        };
        const bool ok =
            load_symbol(Initialize, "Pa_Initialize") &&
            load_symbol(Terminate, "Pa_Terminate") &&
            load_symbol(GetErrorText, "Pa_GetErrorText") &&
            load_symbol(GetDeviceCount, "Pa_GetDeviceCount") &&
            load_symbol(GetDefaultInputDevice, "Pa_GetDefaultInputDevice") &&
            load_symbol(GetDefaultOutputDevice, "Pa_GetDefaultOutputDevice") &&
            load_symbol(GetDeviceInfo, "Pa_GetDeviceInfo") &&
            load_symbol(GetHostApiInfo, "Pa_GetHostApiInfo") &&
            load_symbol(OpenStream, "Pa_OpenStream") &&
            load_symbol(CloseStream, "Pa_CloseStream") &&
            load_symbol(StartStream, "Pa_StartStream") &&
            load_symbol(StopStream, "Pa_StopStream") &&
            load_symbol(ReadStream, "Pa_ReadStream");
        if (!ok) {
            return fail(error);
        }
        return true;
    }

    void unload() {
        if (module) {
            FreeLibrary(module);
            module = nullptr;
        }
    }
};

PortAudioApi& portAudioApi() {
    static PortAudioApi api;
    return api;
}

std::string paErrorText(PaError err) {
    auto& api = portAudioApi();
    if (api.GetErrorText) {
        const char* text = api.GetErrorText(err);
        if (text) return text;
    }
    return "PortAudio error";
}

struct DeviceEntry {
    int index = -1;
    std::string raw_name;
    std::string display_name;
    int rank = 99;
    bool is_generic = false;
};

std::vector<AudioDeviceInfo> listPreferredDevices(bool input) {
    static const std::unordered_map<std::string, int> host_priority = {
        {"windows wasapi", 0}, {"windows wdm-ks", 1}, {"windows directsound", 2}, {"mme", 3},
    };
    static const std::unordered_set<std::string> generic_names = {
        "microsoft sound mapper - input", "microsoft sound mapper - output", "primary sound capture driver", "primary sound driver",
    };
    std::vector<AudioDeviceInfo> result;
    result.push_back(AudioDeviceInfo{-1, input? "Default Input" : "Default Output"});

    auto& api = portAudioApi();
    const int count = static_cast<int>(api.GetDeviceCount());
    if (count < 0) return result;

    std::vector<DeviceEntry> entries;
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = api.GetDeviceInfo(i);
        if (!info) continue;
        if (input && info->maxInputChannels <= 0) continue;
        if (!input && info->maxOutputChannels <= 0) continue;

        std::string raw_name = info->name? info->name : (input? "Input" : "Output");
        std::string host_name;
        const PaHostApiInfo* host = api.GetHostApiInfo(info->hostApi);
        if (host && host->name) host_name = host->name;

        DeviceEntry entry;
        entry.index = i;
        entry.raw_name = raw_name;
        entry.display_name = host_name.empty()? raw_name : raw_name + " [" + host_name + "]";
        entry.rank = 99;
        auto it = host_priority.find(toLower(host_name));
        if (it!= host_priority.end()) entry.rank = it->second;
        entry.is_generic = generic_names.find(toLower(raw_name))!= generic_names.end();
        entries.push_back(std::move(entry));
    }

    if (entries.empty()) return result;

    std::vector<DeviceEntry> candidates;
    std::copy_if(entries.begin(), entries.end(), std::back_inserter(candidates), [](const DeviceEntry& entry) {
        return!entry.is_generic;
    });
    if (candidates.empty()) candidates = entries;

    int best_rank = 99;
    for (const auto& entry : candidates) {
        best_rank = std::min(best_rank, entry.rank);
    }

    std::unordered_map<std::string, DeviceEntry> deduped;
    for (const auto& entry : candidates) {
        if (entry.rank!= best_rank) continue;
        const std::string key = toLower(entry.raw_name);
        auto it = deduped.find(key);
        if (it == deduped.end() || entry.index < it->second.index) {
            deduped[key] = entry;
        }
    }

    std::vector<DeviceEntry> selected;
    selected.reserve(deduped.size());
    for (const auto& item : deduped) {
        selected.push_back(item.second);
    }
    std::sort(selected.begin(), selected.end(), [](const DeviceEntry& a, const DeviceEntry& b) {
        if (a.raw_name == b.raw_name) return a.index < b.index;
        return toLower(a.raw_name) < toLower(b.raw_name);
    });
    for (const auto& entry : selected) {
        result.push_back(AudioDeviceInfo{entry.index, entry.display_name});
    }
    return result;
}

PaDeviceIndex resolveInputDevice(int requested_index) {
    auto& api = portAudioApi();
    PaDeviceIndex device = (requested_index < 0)? api.GetDefaultInputDevice() : requested_index;
    if (device == paNoDevice) return paNoDevice;
    const PaDeviceInfo* info = api.GetDeviceInfo(device);
    if (!info || info->maxInputChannels <= 0) return paNoDevice;
    return device;
}

PaDeviceIndex resolveOutputDevice(int requested_index) {
    auto& api = portAudioApi();
    PaDeviceIndex device = (requested_index < 0)? api.GetDefaultOutputDevice() : requested_index;
    if (device == paNoDevice) return paNoDevice;
    const PaDeviceInfo* info = api.GetDeviceInfo(device);
    if (!info || info->maxOutputChannels <= 0) return paNoDevice;
    return device;
}

int paOutputCallback(const void*, void* output, unsigned long frame_count, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* user_data) {
    auto* audio = reinterpret_cast<AudioEngine*>(user_data);
    if (!audio ||!output) return paContinue;
    audio->renderOutput(reinterpret_cast<int16_t*>(output), static_cast<int>(frame_count));
    return paContinue;
}

int paInputCallback(const void* input, void*, unsigned long frame_count, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* user_data) {
    auto* audio = reinterpret_cast<AudioEngine*>(user_data);
    if (!audio) return paContinue;
    if (!input || frame_count == 0) return paContinue;
    audio->pushCaptureFrame(reinterpret_cast<const int16_t*>(input), static_cast<int>(frame_count));
    return paContinue;
}
} // namespace

AudioEngine::AudioEngine()
    : encoder_(RATE, 1, FRAME, false, 0, 48000, 10, false, OPUS_APPLICATION_VOIP, true, false) {
    SodiumWrapper::init();

    // Instantiate Win32 semaphore with anonymous security attributes
    capture_semaphore_ = CreateSemaphore(nullptr, 0, CAPTURE_QUEUE_MAX, nullptr);

    {
        char* value = nullptr;
        size_t len = 0;
        if (_dupenv_s(&value, &len, "NUUMMITE_AUDIO_DEBUG") == 0 && value) {
            audio_debug_ = (*value!= '\0' && *value!= '0');
            free(value);
        }
    }
    {
        char* value = nullptr;
        size_t len = 0;
        if (_dupenv_s(&value, &len, "NUUMMITE_PURE_OPUS") == 0 && value) {
            pure_opus_ = (*value!= '\0' && *value!= '0');
            free(value);
        }
    }

    rnnoise_ = std::make_unique<RnNoiseProcessor>();
    aec_ = std::make_unique<AecProcessor>(RATE, FRAME);
    if (aec_) {
        aec_->setEchoEnabled(false);
        aec_->setAutoGainEnabled(false);
        aec_->set_stream_delay_ms(180);
    }

    // PortAudio initialization may trigger COM/WASAPI calls. Doing it on a worker thread avoids
    // re-entrancy issues when the UI thread is unwinding a modal dialog / input-synchronous call.
    std::string pa_error;
    std::thread init_thread([&]() {
        auto& pa = portAudioApi();
        if (!pa.load(pa_error)) {
            return;
        }
        const PaError pa_init = pa.Initialize();
        if (pa_init != paNoError) {
            pa_error = paErrorText(pa_init);
        }
    });
    init_thread.join();
    if (!pa_error.empty()) {
        throw std::runtime_error(pa_error);
    }

    recv_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recv_sock_ == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    const int exclusive = 1;
    if (setsockopt(recv_sock_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR) {
        closesocket(recv_sock_);
        recv_sock_ = INVALID_SOCKET;
        throw std::runtime_error("Failed to secure exclusive port binding rules");
    }
    const int buf_size = 65536;
    setsockopt(recv_sock_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&buf_size), sizeof(buf_size));
    socket_utils::set_dscp(recv_sock_, IP_TOS_EF);
    socket_utils::set_non_blocking(recv_sock_, true);
    socket_utils::disable_udp_connreset(recv_sock_);

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(0);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(recv_sock_, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        closesocket(recv_sock_);
        throw std::runtime_error("Failed to bind UDP socket");
    }

    sockaddr_in name{};
    int name_len = sizeof(name);
    if (getsockname(recv_sock_, reinterpret_cast<sockaddr*>(&name), &name_len) == 0) {
        port_ = ntohs(name.sin_port);
    }

    echo_enabled_.store(false);

    mix_accum_.assign(FRAME, 0.0f);
    mix_frame_.assign(FRAME, 0);
    playback_fifo_.assign(static_cast<size_t>(FRAME) * 8u, 0); 
    fifo_read_ = fifo_write_ = fifo_size_ = 0;
    for (auto& buf : stream_snapshot_buffers_) {
        buf.reserve(128);
    }

    if (!openOutput()) {
        std::cerr << " Failed to open output device\n";
    }

    listen_thread_ = std::thread(&AudioEngine::listenLoop, this);
}

AudioEngine::~AudioEngine() {
    shutdown();
    if (capture_semaphore_) {
        CloseHandle(capture_semaphore_);
        capture_semaphore_ = nullptr;
    }
}

std::vector<AudioDeviceInfo> AudioEngine::listInputDevices() const {
    return listPreferredDevices(true);
}

std::vector<AudioDeviceInfo> AudioEngine::listOutputDevices() const {
    return listPreferredDevices(false);
}

bool AudioEngine::setInputDevice(int device_index) {
    input_device_index_ = device_index;
    std::vector<std::string> destinations;
    {
        std::lock_guard<std::mutex> lock(routing_mutex_);
        destinations = send_destinations_;
    }
    if (running_.load() &&!destinations.empty()) {
        stop();
        return start(destinations);
    }
    closeInput();
    const bool ok = openInput();
    closeInput();
    return ok;
}

bool AudioEngine::setOutputDevice(int device_index) {
    output_device_index_ = device_index;
    closeOutput();
    return openOutput();
}

void AudioEngine::setMasterVolume(int value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    master_volume_ = std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 2.5f);
}

void AudioEngine::setOutputVolume(int value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    output_volume_ = std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 2.5f);
}

void AudioEngine::setGainDb(int value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    tx_gain_db_ = std::clamp(static_cast<float>(value), -20.0f, 20.0f);
}

void AudioEngine::setMicSensitivity(int value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    mic_sensitivity_ = std::clamp(value, 0, 100);
}

void AudioEngine::setNoiseSuppression(int value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    noise_suppression_ = std::clamp(value, 0, 100);
}

void AudioEngine::setNoiseSuppressionEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    noise_suppression_enabled_ = enabled;
}

void AudioEngine::setAutoGain(bool enabled) {
    const bool apply_enabled = (!pure_opus_) && enabled;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        auto_gain_ = apply_enabled;
    }
    target_agc_enabled_.store(apply_enabled, std::memory_order_relaxed);
    pending_agc_update_.store(true, std::memory_order_release);
}

void AudioEngine::setEchoEnabled(bool enabled) {
    echo_enabled_.store(enabled, std::memory_order_release);
    target_aec_enabled_.store(enabled, std::memory_order_relaxed);
    pending_aec_update_.store(true, std::memory_order_release);
}

void AudioEngine::setAecStreamDelayMs(int delay_ms) {
    delay_ms = std::clamp(delay_ms, 0, 500);
    aec_stream_delay_ms_.store(delay_ms);
}

void AudioEngine::setHearTargets(const std::unordered_set<std::string>& hear_ids) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    hear_targets_ = hear_ids;
    rebuildStreamSnapshotLocked_();
}

AudioEngine::StreamState* AudioEngine::getOrCreateStream(const std::string& id) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto& st = streams_[id];
    const bool created = (st == nullptr);
    if (!st) {
        st = std::make_unique<StreamState>();
        st->id = id;
    }
    if (streams_.size() > stream_snapshot_buffers_[0].capacity()) {
        const size_t want = std::max(stream_snapshot_buffers_[0].capacity() * 2, streams_.size());
        for (auto& buf : stream_snapshot_buffers_) {
            buf.reserve(want);
        }
    }
    if (created) {
        rebuildStreamSnapshotLocked_();
    }
    return st.get();
}

void AudioEngine::rebuildStreamSnapshotLocked_() {
    const uint8_t current = stream_snapshot_active_.load(std::memory_order_relaxed);
    const uint8_t write = static_cast<uint8_t>(current ^ 1u);

    while (stream_snapshot_readers_[write].load(std::memory_order_acquire) != 0) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_pause();
#endif
        std::this_thread::yield();
    }

    auto& buf = stream_snapshot_buffers_[write];
    buf.clear();
    buf.reserve(streams_.size());

    for (const auto& [id, st] : streams_) {
        if (!st) continue;
        if (!hear_targets_.empty() && !hear_targets_.count(id)) continue;
        buf.push_back(st.get());
    }

    stream_snapshot_active_.store(write, std::memory_order_release);
}

void AudioEngine::setTxMuted(bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    tx_muted_ = enabled;
}

bool AudioEngine::isTxMuted() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return tx_muted_;
}

// Fixed GUI Blocking: Spawns an asynchronous background thread for microphone testing.
int AudioEngine::testMicrophoneLevel(double duration_sec) {
    if (running_.load(std::memory_order_acquire)) {
        return capture_level_.load(std::memory_order_relaxed);
    }

    // Ensure only one background testing thread executes at a time
    if (is_mic_testing_.exchange(true, std::memory_order_acq_rel)) {
        return capture_level_.load(std::memory_order_relaxed);
    }

    duration_sec = std::max(0.2, duration_sec);

    std::thread([this, duration_sec]() {
        auto& pa = portAudioApi();
        PaDeviceIndex device = resolveInputDevice(input_device_index_);
        if (device == paNoDevice) {
            is_mic_testing_.store(false, std::memory_order_release);
            return;
        }

        const PaDeviceInfo* info = pa.GetDeviceInfo(device);
        if (!info) {
            is_mic_testing_.store(false, std::memory_order_release);
            return;
        }

        PaStreamParameters params{};
        params.device = device;
        params.channelCount = 1;
        params.sampleFormat = paInt16;
        params.suggestedLatency = info->defaultLowInputLatency;
        PaStream* stream = nullptr;

        PaError err = pa.OpenStream(&stream, &params, nullptr, RATE, FRAME, paNoFlag, nullptr, nullptr);
        if (err!= paNoError ||!stream) {
            is_mic_testing_.store(false, std::memory_order_release);
            return;
        }
        if (pa.StartStream(stream)!= paNoError) {
            pa.CloseStream(stream);
            is_mic_testing_.store(false, std::memory_order_release);
            return;
        }

        std::vector<int16_t> frame(FRAME, 0);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(duration_sec * 1000));

        while (std::chrono::steady_clock::now() < deadline &&!running_.load(std::memory_order_acquire)) {
            err = pa.ReadStream(stream, frame.data(), FRAME);
            if (err!= paNoError) {
                break;
            }
            int max_peak = 0;
            for (const auto sample : frame) {
                max_peak = std::max(max_peak, static_cast<int>(std::abs(sample)));
            }
            if (max_peak > 0) {
                capture_level_.store(std::min(100, static_cast<int>((max_peak * 100) / 32767)), std::memory_order_relaxed);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        pa.StopStream(stream);
        pa.CloseStream(stream);
        capture_level_.store(0, std::memory_order_relaxed);
        is_mic_testing_.store(false, std::memory_order_release);
    }).detach();

    return 0; // Return control immediately to keep the GUI responsive
}

bool AudioEngine::openOutput() {
    if (wave_out_) return true;

    auto& pa = portAudioApi();
    PaDeviceIndex device = resolveOutputDevice(output_device_index_);
    if (device == paNoDevice) return false;

    const PaDeviceInfo* info = pa.GetDeviceInfo(device);
    if (!info) return false;

    PaStreamParameters params{};
    params.device = device;
    params.channelCount = 1;
    params.sampleFormat = paInt16;
    params.suggestedLatency = info->defaultHighOutputLatency;
    PaStream* stream = nullptr;
    PaError err = pa.OpenStream(&stream, nullptr, &params, RATE, FRAME, paNoFlag, &paOutputCallback, this);
    if (err!= paNoError ||!stream) {
        std::cerr << " PortAudio output open failed: " << paErrorText(err) << "\n";
        return false;
    }
    err = pa.StartStream(stream);
    if (err!= paNoError) {
        pa.CloseStream(stream);
        std::cerr << " PortAudio output start failed: " << paErrorText(err) << "\n";
        return false;
    }

    wave_out_ = stream;
    playback_running_.store(true);
    return true;
}

void AudioEngine::closeOutput() {
    playback_running_.store(false);
    if (!wave_out_) return;

    auto& pa = portAudioApi();
    PaStream* stream = reinterpret_cast<PaStream*>(wave_out_);
    pa.StopStream(stream);
    pa.CloseStream(stream);
    wave_out_ = nullptr;
}

bool AudioEngine::openInput() {
    if (wave_in_) return true;

    auto& pa = portAudioApi();
    PaDeviceIndex device = resolveInputDevice(input_device_index_);
    if (device == paNoDevice) return false;

    const PaDeviceInfo* info = pa.GetDeviceInfo(device);
    if (!info) return false;

    PaStreamParameters params{};
    params.device = device;
    params.channelCount = 1;
    params.sampleFormat = paInt16;
    params.suggestedLatency = info->defaultLowInputLatency;
    PaStream* stream = nullptr;
    PaError err = pa.OpenStream(&stream, &params, nullptr, RATE, FRAME, paNoFlag, &paInputCallback, this);
    if (err!= paNoError ||!stream) {
        std::cerr << " PortAudio input open failed: " << paErrorText(err) << "\n";
        return false;
    }
    err = pa.StartStream(stream);
    if (err!= paNoError) {
        pa.CloseStream(stream);
        std::cerr << " PortAudio input start failed: " << paErrorText(err) << "\n";
        return false;
    }

    wave_in_ = stream;
    return true;
}

void AudioEngine::closeInput() {
    if (!wave_in_) return;

    auto& pa = portAudioApi();
    PaStream* stream = reinterpret_cast<PaStream*>(wave_in_);
    pa.StopStream(stream);
    pa.CloseStream(stream);
    wave_in_ = nullptr;
}

// Fixed Socket Polling: Replaced Sleep(1) polling with low-overhead WSAPoll kernel-level blocks.
void AudioEngine::listenLoop() {
    std::cout << " Socket processing loop configured to use kernel descriptor blocks\n";
    
    WSAPOLLFD poll_fd{};
    poll_fd.fd = recv_sock_;
    poll_fd.events = POLLRDNORM;

    while (listen_running_.load()) {
        // Blocks execution until voice packets arrive, checking state every 100ms
        int poll_result = WSAPoll(&poll_fd, 1, 100);
        if (poll_result == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            if (err == WSAENOTSOCK ||!listen_running_.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (poll_result == 0) {
            continue; // WSAPoll timeout; loop to recheck listen_running_ flag
        }

        if (poll_fd.revents & POLLRDNORM) {
            std::vector<uint8_t> buffer(4096);
            sockaddr_in src{};
            int src_len = sizeof(src);
            const int recv_len = recvfrom(recv_sock_, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                                          reinterpret_cast<sockaddr*>(&src), &src_len);
            if (recv_len == SOCKET_ERROR) {
                const int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAECONNRESET) {
                    continue;
                }
                if (!listen_running_.load()) break;
                continue;
            }
            if (recv_len <= 0) continue;

            packets_recv_.fetch_add(1, std::memory_order_relaxed);
            buffer.resize(static_cast<size_t>(recv_len));
            handleIncomingPacket(buffer);
        }
    }
}

void AudioEngine::handleIncomingPacket(const std::vector<uint8_t>& data) {
    const auto packet = parse_voice_packet(data);
    if (!packet.has_value() || packet->kind!= VoicePacketKind::ClientAudio ||
        packet->sender_id.empty() || packet->sender_id == client_id_) {
        return;
    }
    packets_decrypted_.fetch_add(1, std::memory_order_relaxed);

    auto* st = getOrCreateStream(packet->sender_id);
    if (!st) return;

    st->acquireLock();
    if (!st->decoder) {
        st->decoder = std::make_unique<OpusCodec>(
            RATE, 1, FRAME, true, 10, 24000, 10, false, OPUS_APPLICATION_VOIP, false, true
        );
        st->jitter_buffer.reset();
    }

    if (!packet->payload.empty()) {
        st->jitter_buffer.push(packet->seq, packet->payload.data(), packet->payload.size());
    }
    st->releaseLock();
}

int AudioEngine::getPeerPeak(const std::string& peer_id) const {
    if (peer_id.empty()) return 0;

    std::unique_lock<std::mutex> lock(streams_mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        return 0;
    }
    auto it = streams_.find(peer_id);
    if (it == streams_.end() || !it->second) {
        return 0;
    }
    return it->second->peak_pcm.load(std::memory_order_relaxed);
}

void AudioEngine::renderOutput(int16_t* out, int sample_count) {
    if (!out || sample_count <= 0) return;

    struct StreamSnapshotGuard {
        AudioEngine* engine = nullptr;
        uint8_t idx = 0;
        const std::vector<AudioEngine::StreamState*>* snapshot = nullptr;

        explicit StreamSnapshotGuard(AudioEngine* e) : engine(e) {
            for (;;) {
                idx = engine->stream_snapshot_active_.load(std::memory_order_acquire);
                engine->stream_snapshot_readers_[idx].fetch_add(1, std::memory_order_acq_rel);
                if (idx == engine->stream_snapshot_active_.load(std::memory_order_acquire)) break;
                engine->stream_snapshot_readers_[idx].fetch_sub(1, std::memory_order_release);
            }
            snapshot = &engine->stream_snapshot_buffers_[idx];
        }

        ~StreamSnapshotGuard() {
            if (!engine) return;
            engine->stream_snapshot_readers_[idx].fetch_sub(1, std::memory_order_release);
        }

        StreamSnapshotGuard(const StreamSnapshotGuard&) = delete;
        StreamSnapshotGuard& operator=(const StreamSnapshotGuard&) = delete;
    };

    const size_t need = static_cast<size_t>(sample_count);
    if (playback_fifo_.empty() || mix_frame_.size()!= static_cast<size_t>(FRAME) ||
        mix_accum_.size()!= static_cast<size_t>(FRAME)) {
        std::fill(out, out + need, 0);
        return;
    }

    const size_t cap = playback_fifo_.size();
    auto fifoWrite = [&](const int16_t* data, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (fifo_size_ >= cap) {
                fifo_read_ = (fifo_read_ + 1) % cap;
                fifo_size_--;
            }
            playback_fifo_[fifo_write_] = data[i];
            fifo_write_ = (fifo_write_ + 1) % cap;
            fifo_size_++;
        }
    };
    auto fifoRead = [&](int16_t* dst, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (fifo_size_ == 0) {
                dst[i] = 0;
                continue;
            }
            dst[i] = playback_fifo_[fifo_read_];
            fifo_read_ = (fifo_read_ + 1) % cap;
            fifo_size_--;
        }
    };
    auto mixOneFixedFrame = [&]() {
        std::fill(mix_accum_.begin(), mix_accum_.end(), 0.0f);
        int active = 0;
        float peak = 0.0f;

        std::array<int16_t, FRAME> pcm_frame{};
        StreamSnapshotGuard guard(this);
        if (!guard.snapshot || guard.snapshot->empty()) {
            std::fill(mix_frame_.begin(), mix_frame_.end(), 0);
            updateMixedLevel(mix_frame_);
            return;
        }

        for (auto* st : *guard.snapshot) {
            if (!st) continue;

            if (!st->tryAcquireLock()) continue;

            JitterBuffer::PacketView view{};
            bool is_missing = false;
            const bool has_packet = st->jitter_buffer.pop(view, is_missing);
            if (!has_packet ||!st->decoder) {
                st->peak_pcm.store(0, std::memory_order_relaxed);
                st->releaseLock();
                continue;
            }

            int decoded = 0;
            if (is_missing) {
                decoded = st->decoder->decode_into(nullptr, 0, pcm_frame.data(), FRAME);
            } else {
                decoded = st->decoder->decode_into(view.data, static_cast<int>(view.len), pcm_frame.data(), FRAME);
            }
            st->releaseLock();
            if (decoded < 0) {
                st->peak_pcm.store(0, std::memory_order_relaxed);
                continue;
            }
            if (decoded > FRAME) decoded = FRAME;
            if (decoded < FRAME) {
                std::fill(pcm_frame.begin() + decoded, pcm_frame.end(), 0);
            }

            int stream_peak = 0;
            for (int i = 0; i < FRAME; ++i) {
                stream_peak = std::max(stream_peak, std::abs(static_cast<int>(pcm_frame[static_cast<size_t>(i)])));
            }
            st->peak_pcm.store(stream_peak, std::memory_order_relaxed);

            active++;
            for (int i = 0; i < FRAME; ++i) {
                mix_accum_[i] += static_cast<float>(pcm_frame[static_cast<size_t>(i)]);
            }
        }

        if (active == 0) {
            std::fill(mix_frame_.begin(), mix_frame_.end(), 0);
            updateMixedLevel(mix_frame_);
            return;
        }

        for (float v : mix_accum_) {
            peak = std::max(peak, static_cast<float>(std::abs(v)));
        }

        const float pre_scale = (peak > 28000.0f)? (28000.0f / peak) : 1.0f;
        for (int i = 0; i < FRAME; ++i) {
            mix_accum_[i] = (mix_accum_[i] * pre_scale) / 32768.0f;
        }
        static float softclip_mem[1] = {0.0f};
        opus_pcm_soft_clip(mix_accum_.data(), FRAME, 1, softclip_mem);
        for (int i = 0; i < FRAME; ++i) {
            const float v = mix_accum_[i] * 32767.0f;
            mix_frame_[i] = static_cast<int16_t>(std::clamp(v, -32768.0f, 32767.0f));
        }

        if (!pure_opus_) {
            if (aec_ && aec_->available() && echo_enabled_.load()) {
                std::unique_lock<std::mutex> lock(echo_mutex_, std::try_to_lock);
                if (lock.owns_lock()) {
                    aec_->process_render(mix_frame_.data(), FRAME);
                }
            }

            float master = 1.0f, output = 1.0f;
            std::unique_lock<std::mutex> cfg_lock(config_mutex_, std::try_to_lock);
            if (cfg_lock.owns_lock()) {
                master = master_volume_;
                output = output_volume_;
            }
            const float volume_factor = std::clamp(master * output, 0.0f, 2.5f);
            if (std::abs(volume_factor - 1.0f) > 0.0001f) {
                for (auto& sample : mix_frame_) {
                    float scaled = static_cast<float>(sample) * volume_factor;
                    sample = static_cast<int16_t>(std::clamp(scaled, -32768.0f, 32767.0f));
                }
            }
        }

        updateMixedLevel(mix_frame_);
    };

    while (fifo_size_ < need) {
        mixOneFixedFrame();
        fifoWrite(mix_frame_.data(), static_cast<size_t>(FRAME));
    }

    fifoRead(out, need);
}

void AudioEngine::updateMixedLevel(const std::vector<int16_t>& frame) {
    int peak = 0;
    for (const auto sample : frame) {
        peak = std::max(peak, static_cast<int>(std::abs(sample)));
    }
    const float prev = mixed_peak_.load();
    mixed_peak_.store(0.9f * prev + 0.1f * static_cast<float>(peak));
}

void AudioEngine::pushCaptureFrame(const int16_t* samples, int sample_count) {
    if (!samples || sample_count <= 0 ||!running_.load()) return;

    AudioEngine::CaptureFrame frame{};
    const int n = std::min(sample_count, FRAME);
    std::copy_n(samples, n, frame.begin());
    if (n < FRAME) {
        std::fill(frame.begin() + n, frame.end(), 0);
    }

    if (capture_frames_.push(frame)) {
        // Release the semaphore to notify the send loop that a new frame is ready
        ReleaseSemaphore(capture_semaphore_, 1, nullptr);
    } else {
        capture_dropped_.fetch_add(1, std::memory_order_relaxed);
    }
}

bool AudioEngine::popCaptureFrame(CaptureFrame& out) noexcept {
    return capture_frames_.pop(out);
}

bool AudioEngine::start(const std::vector<std::string>& destinations) {
    if (client_id_.empty()) return false;
    if (running_.load()) return true;
    if (destinations.empty()) return false;

    {
        std::lock_guard<std::mutex> lock(routing_mutex_);
        send_destinations_ = destinations;
    }
    transport_.setDestinations(destinations);
    if (!transport_.hasDestinations()) return false;

    capture_frames_.reset();
    capture_dropped_.store(0, std::memory_order_relaxed);
    running_.store(true);

    if (!openInput()) {
        running_.store(false);
        std::cerr << " Failed to start capture\n";
        return false;
    }

    send_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (send_sock_ == INVALID_SOCKET) {
        running_.store(false);
        closeInput();
        return false;
    }
    const int buf_size = 65536;
    setsockopt(send_sock_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&buf_size), sizeof(buf_size));
    socket_utils::set_dscp(send_sock_, IP_TOS_EF);
    socket_utils::set_non_blocking(send_sock_, true);
    socket_utils::disable_udp_connreset(send_sock_);
    try {
        send_thread_ = std::thread(&AudioEngine::sendLoop, this);
    } catch (...) {
        closesocket(send_sock_);
        send_sock_ = INVALID_SOCKET;
        running_.store(false);
        closeInput();
        throw;
    }

    auto active_destinations = transport_.destinations();
    std::cout << " Audio capture ACTIVE\n";
    return true;
}

bool AudioEngine::updateDestinations(const std::vector<std::string>& destinations) {
    if (destinations.empty()) return false;
    {
        std::lock_guard<std::mutex> lock(routing_mutex_);
        send_destinations_ = destinations;
    }
    transport_.setDestinations(destinations);
    return transport_.hasDestinations();
}

// Fixed Queue Polling: Used native Win32 Semaphore blocking to eliminate the capture thread Sleep(1) loop.
void AudioEngine::sendLoop() {
    std::vector<int16_t> frame;
    frame.resize(FRAME);

    while (running_.load()) {
        AudioEngine::CaptureFrame raw{};

        // Sleep until a new frame is pushed by the capture callback, checking state every 100ms
        DWORD wait_result = WaitForSingleObject(capture_semaphore_, 100);
        if (wait_result!= WAIT_OBJECT_0) {
            continue; // Semaphore timeout; loop to recheck running_ flag
        }

        if (!popCaptureFrame(raw)) {
            continue;
        }

        std::copy(raw.begin(), raw.end(), frame.begin());

        bool tx_muted_local = false;
        bool autogain_local = false;
        bool rnnoise_local = false;
        float tx_gain_db_local = 0.0f;
        int mic_sens_local = 50;
        int ns_amount_local = 0;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            tx_muted_local = tx_muted_;
            autogain_local = auto_gain_;
            rnnoise_local = noise_suppression_enabled_;
            tx_gain_db_local = tx_gain_db_;
            mic_sens_local = mic_sensitivity_;
            ns_amount_local = noise_suppression_;
        }

        if (!pure_opus_) {
            if (aec_ && aec_->available()) {
                const bool need_update =
                    pending_aec_update_.load(std::memory_order_acquire) ||
                    pending_agc_update_.load(std::memory_order_acquire);
                if (need_update) {
                    std::lock_guard<std::mutex> lock(echo_mutex_);
                    if (aec_) {
                        if (pending_aec_update_.exchange(false, std::memory_order_acq_rel)) {
                            aec_->setEchoEnabled(target_aec_enabled_.load(std::memory_order_relaxed));
                        }
                        if (pending_agc_update_.exchange(false, std::memory_order_acq_rel)) {
                            aec_->setAutoGainEnabled(target_agc_enabled_.load(std::memory_order_relaxed));
                        }
                    } else {
                        pending_aec_update_.store(false, std::memory_order_release);
                        pending_agc_update_.store(false, std::memory_order_release);
                    }
                }
            } else {
                pending_aec_update_.store(false, std::memory_order_release);
                pending_agc_update_.store(false, std::memory_order_release);
            }

            const bool want_apm =!tx_muted_local && aec_ && aec_->available() && (echo_enabled_.load() || autogain_local);
            if (want_apm) {
                if (echo_enabled_.load()) {
                    std::lock_guard<std::mutex> lock(echo_mutex_);
                    aec_->set_stream_delay_ms(aec_stream_delay_ms_.load());
                    aec_->process_capture(frame); 
                } else {
                    std::unique_lock<std::mutex> lock(echo_mutex_, std::try_to_lock);
                    if (lock.owns_lock()) {
                        aec_->set_stream_delay_ms(aec_stream_delay_ms_.load());
                        aec_->process_capture(frame);
                    }
                }
            }

            if (!tx_muted_local && rnnoise_local && rnnoise_ && rnnoise_->isAvailable() && frame.size() == FRAME) {
                const float amount = std::clamp(static_cast<float>(ns_amount_local) / 100.0f, 0.0f, 1.0f);
                if (amount > 0.0001f) {
                    constexpr int blockSize = FRAME / 2; 
                    for (int off = 0; off < FRAME; off += blockSize) {
                        std::array<int16_t, blockSize> orig{};
                        std::array<int16_t, blockSize> denoised{};
                        std::copy_n(frame.data() + off, blockSize, orig.begin());
                        denoised = orig;
                        rnnoise_->processBlock(denoised.data(), blockSize);
                        if (amount >= 0.999f) {
                            std::copy_n(denoised.begin(), blockSize, frame.begin() + off);
                        } else {
                            for (int i = 0; i < blockSize; ++i) {
                                const float mixed = (1.0f - amount) * static_cast<float>(orig[i]) +
                                                    amount * static_cast<float>(denoised[i]);
                                frame[static_cast<size_t>(off + i)] =
                                    static_cast<int16_t>(std::clamp(mixed, -32768.0f, 32767.0f));
                            }
                        }
                    }
                }
            }

            if (tx_muted_local) {
                std::fill(frame.begin(), frame.end(), 0);
            } else if (!autogain_local) {
                const float gain_linear = std::pow(10.0f, tx_gain_db_local / 20.0f);
                const float sens_linear = static_cast<float>(mic_sens_local) / 50.0f;
                const float total_gain = std::clamp(gain_linear * sens_linear, 0.0f, 6.0f);
                if (std::abs(total_gain - 1.0f) > 0.0001f) {
                    for (auto& sample : frame) {
                        float scaled = static_cast<float>(sample) * total_gain;
                        sample = static_cast<int16_t>(std::clamp(scaled, -32768.0f, 32767.0f));
                    }
                }
            }

            int input_peak = 0;
            for (const auto sample : frame) {
                input_peak = std::max(input_peak, std::abs(static_cast<int>(sample)));
            }
            capture_level_.store(std::min(100, (input_peak * 100) / 32767));
            capture_active_.store(!tx_muted_local);
        } else {
            if (tx_muted_local) {
                std::fill(frame.begin(), frame.end(), 0);
            }

            int input_peak = 0;
            for (const auto sample : frame) {
                input_peak = std::max(input_peak, std::abs(static_cast<int>(sample)));
            }
            capture_level_.store(std::min(100, (input_peak * 100) / 32767));
            capture_active_.store(!tx_muted_local);
        }

        const std::vector<uint8_t> opus_data = encoder_.encode(frame);
        if (opus_data.empty()) continue;

        std::vector<uint8_t> packet = build_client_audio_packet(client_id_, seq_, timestamp_, opus_data);
        if (packet.empty()) continue;

        seq_ = static_cast<uint16_t>((seq_ + 1) & 0xFFFF);
        timestamp_ += FRAME;

        const int sent = transport_.sendPacket(send_sock_, packet);
        if (sent > 0) {
            packets_sent_.fetch_add(static_cast<uint64_t>(sent), std::memory_order_relaxed);
        }
    }
}

void AudioEngine::stop() {
    running_.store(false);

    // Release the Win32 semaphore to instantly wake up and stop the send loop thread
    if (capture_semaphore_) {
        ReleaseSemaphore(capture_semaphore_, 1, nullptr);
    }

    if (send_thread_.joinable()) {
        send_thread_.join();
    }
    capture_frames_.reset();

    capture_active_.store(false);
    capture_level_.store(0);

    closeInput();
    if (send_sock_!= INVALID_SOCKET) {
        closesocket(send_sock_);
        send_sock_ = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(routing_mutex_);
        send_destinations_.clear();
    }
    transport_.setDestinations({});
}

void AudioEngine::shutdown() {
    stop();
    listen_running_.store(false);

    if (recv_sock_!= INVALID_SOCKET) {
        closesocket(recv_sock_);
        recv_sock_ = INVALID_SOCKET;
    }
    if (listen_thread_.joinable()) {
        listen_thread_.join();
    }

    closeOutput();

    {
        std::lock_guard<std::mutex> lock(echo_mutex_);
        echo_enabled_.store(false);
    }

    auto& pa = portAudioApi();
    if (pa.Terminate) {
        pa.Terminate();
    }

    SodiumWrapper::shutdown();
}
