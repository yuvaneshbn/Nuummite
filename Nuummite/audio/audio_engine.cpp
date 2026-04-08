#include "audio_engine.h"

#include "audio_packet.h"
#include "socket_utils.h"
#include "portaudio_dyn.h"
#include "aec_processor.h"
#include "rnnoise_processor.h"
#include "winsock_init.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {
// Higher sample rate and 20 ms frames for better fidelity
constexpr int RATE = 48000;
constexpr int FRAME = 960; // 20 ms @ 48 kHz
constexpr int FRAME_BYTES = FRAME * 2;
constexpr int AUDIO_PORT = 50002;
constexpr int DSCP_EF = 46;
constexpr int IP_TOS_EF = DSCP_EF << 2;
constexpr int RX_QUEUE_MAX_FRAMES = 8;
constexpr int CAPTURE_QUEUE_MAX = 16;
constexpr const char* DEFAULT_PORTAUDIO_DLL = "libportaudio.dll";

// Global Winsock initializer so sockets work even when used via Cython entrypoint
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

std::string cwd() {
    char buf[MAX_PATH] = {0};
    const DWORD len = GetCurrentDirectoryA(MAX_PATH, buf);
    if (len == 0 || len >= MAX_PATH) {
        return ".";
    }
    return std::string(buf, buf + len);
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
    using Pa_OpenStream_Fn = PaError (*)(PaStream**,
                                         const PaStreamParameters*,
                                         const PaStreamParameters*,
                                         double,
                                         unsigned long,
                                         PaStreamFlags,
                                         PaStreamCallback*,
                                         void*);
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
        if (module) {
            return true;
        }

        std::vector<std::string> candidates = {
            exeDir() + "\\" + DEFAULT_PORTAUDIO_DLL,
            exeDir() + "\\third_party\\libportaudio\\" + DEFAULT_PORTAUDIO_DLL,
            cwd() + "\\third_party\\libportaudio\\" + DEFAULT_PORTAUDIO_DLL,
            DEFAULT_PORTAUDIO_DLL,
        };
#ifdef VOICE_PORTAUDIO_DLL_FALLBACK
        candidates.push_back(VOICE_PORTAUDIO_DLL_FALLBACK);
#endif

        for (const auto& candidate : candidates) {
            module = LoadLibraryA(candidate.c_str());
            if (module) {
                break;
            }
        }
        if (!module) {
            error = "Could not load libportaudio.dll";
            return false;
        }

        auto load_symbol = [this, &error](auto& fn, const char* name) -> bool {
            fn = reinterpret_cast<std::decay_t<decltype(fn)>>(GetProcAddress(module, name));
            if (!fn) {
                error = std::string("Missing PortAudio symbol: ") + name;
                return false;
            }
            return true;
        };

        return load_symbol(Initialize, "Pa_Initialize") &&
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
        if (text) {
            return text;
        }
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
        {"windows wasapi", 0},
        {"windows wdm-ks", 1},
        {"windows directsound", 2},
        {"mme", 3},
    };
    static const std::unordered_set<std::string> generic_names = {
        "microsoft sound mapper - input",
        "microsoft sound mapper - output",
        "primary sound capture driver",
        "primary sound driver",
    };

    std::vector<AudioDeviceInfo> result;
    result.push_back(AudioDeviceInfo{-1, input ? "Default Input" : "Default Output"});

    auto& api = portAudioApi();
    const int count = static_cast<int>(api.GetDeviceCount());
    if (count < 0) {
        return result;
    }

    std::vector<DeviceEntry> entries;
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = api.GetDeviceInfo(i);
        if (!info) {
            continue;
        }
        if (input && info->maxInputChannels <= 0) {
            continue;
        }
        if (!input && info->maxOutputChannels <= 0) {
            continue;
        }

        std::string raw_name = info->name ? info->name : (input ? "Input" : "Output");
        std::string host_name;
        const PaHostApiInfo* host = api.GetHostApiInfo(info->hostApi);
        if (host && host->name) {
            host_name = host->name;
        }

        DeviceEntry entry;
        entry.index = i;
        entry.raw_name = raw_name;
        entry.display_name = host_name.empty() ? raw_name : raw_name + " [" + host_name + "]";
        entry.rank = 99;
        auto it = host_priority.find(toLower(host_name));
        if (it != host_priority.end()) {
            entry.rank = it->second;
        }
        entry.is_generic = generic_names.find(toLower(raw_name)) != generic_names.end();
        entries.push_back(std::move(entry));
    }

    if (entries.empty()) {
        return result;
    }

    std::vector<DeviceEntry> candidates;
    std::copy_if(entries.begin(), entries.end(), std::back_inserter(candidates), [](const DeviceEntry& entry) {
        return !entry.is_generic;
    });
    if (candidates.empty()) {
        candidates = entries;
    }

    int best_rank = 99;
    for (const auto& entry : candidates) {
        best_rank = std::min(best_rank, entry.rank);
    }

    std::unordered_map<std::string, DeviceEntry> deduped;
    for (const auto& entry : candidates) {
        if (entry.rank != best_rank) {
            continue;
        }
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
        if (a.raw_name == b.raw_name) {
            return a.index < b.index;
        }
        return toLower(a.raw_name) < toLower(b.raw_name);
    });

    for (const auto& entry : selected) {
        result.push_back(AudioDeviceInfo{entry.index, entry.display_name});
    }
    return result;
}

PaDeviceIndex resolveInputDevice(int requested_index) {
    auto& api = portAudioApi();
    PaDeviceIndex device = (requested_index < 0) ? api.GetDefaultInputDevice() : requested_index;
    if (device == paNoDevice) {
        return paNoDevice;
    }
    const PaDeviceInfo* info = api.GetDeviceInfo(device);
    if (!info || info->maxInputChannels <= 0) {
        return paNoDevice;
    }
    return device;
}

PaDeviceIndex resolveOutputDevice(int requested_index) {
    auto& api = portAudioApi();
    PaDeviceIndex device = (requested_index < 0) ? api.GetDefaultOutputDevice() : requested_index;
    if (device == paNoDevice) {
        return paNoDevice;
    }
    const PaDeviceInfo* info = api.GetDeviceInfo(device);
    if (!info || info->maxOutputChannels <= 0) {
        return paNoDevice;
    }
    return device;
}

int paOutputCallback(const void*,
                     void* output,
                     unsigned long frame_count,
                     const PaStreamCallbackTimeInfo*,
                     PaStreamCallbackFlags,
                     void* user_data) {
    auto* audio = reinterpret_cast<AudioEngine*>(user_data);
    if (!audio || !output) {
        return paContinue;
    }

    auto frame = audio->mixFrame();
    if (frame.size() < static_cast<size_t>(frame_count)) {
        frame.resize(static_cast<size_t>(frame_count), 0);
    } else if (frame.size() > static_cast<size_t>(frame_count)) {
        frame.resize(static_cast<size_t>(frame_count));
    }

    std::memcpy(output, frame.data(), frame.size() * sizeof(int16_t));
    return paContinue;
}

int paInputCallback(const void* input,
                    void*,
                    unsigned long frame_count,
                    const PaStreamCallbackTimeInfo*,
                    PaStreamCallbackFlags,
                    void* user_data) {
    auto* audio = reinterpret_cast<AudioEngine*>(user_data);
    if (!audio) {
        return paContinue;
    }
    if (!input || frame_count == 0) {
        return paContinue;
    }
    audio->pushCaptureFrame(reinterpret_cast<const int16_t*>(input), static_cast<int>(frame_count));
    return paContinue;
}

} // namespace

AudioEngine::AudioEngine()
    // VOIP tuned: enable DTX, lower bitrate, VOIP app for cleaner speech
    : encoder_(RATE, 1, FRAME, true, 10, 32000, 10, true, OPUS_APPLICATION_VOIP, true, false),
      decoder_(RATE, 1, FRAME, true, 10, 64000, 10, false, OPUS_APPLICATION_AUDIO, false, true) {
    SodiumWrapper::init();
    rnnoise_ = std::make_unique<RnNoiseProcessor>();
        aec_ = std::make_unique<AecProcessor>(RATE, FRAME);
    std::string pa_error;
    auto& pa = portAudioApi();
    if (!pa.load(pa_error)) {
        throw std::runtime_error(pa_error);
    }
    const PaError pa_init = pa.Initialize();
    if (pa_init != paNoError) {
        throw std::runtime_error(paErrorText(pa_init));
    }

    recv_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recv_sock_ == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    const int reuse = 1;
    setsockopt(recv_sock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    const int buf_size = 65536;
    setsockopt(recv_sock_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&buf_size), sizeof(buf_size));
    socket_utils::set_dscp(recv_sock_, IP_TOS_EF);

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    // Bind to an ephemeral port; we advertise the chosen port to peers.
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

   

    if (!openOutput()) {
        std::cerr << "[AUDIO] Failed to open output device\n";
    }

    listen_thread_ = std::thread(&AudioEngine::listenLoop, this);
}

AudioEngine::~AudioEngine() {
    shutdown();
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
    if (running_.load() && !destinations.empty()) {
        stop();
        return start(destinations);
    }
    return true;
}

bool AudioEngine::setOutputDevice(int device_index) {
    output_device_index_ = device_index;
    closeOutput();
    return openOutput();
}

void AudioEngine::setMasterVolume(int value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    master_volume_ = std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 2.0f);
}

void AudioEngine::setOutputVolume(int value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    output_volume_ = std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 2.0f);
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
    std::lock_guard<std::mutex> lock(config_mutex_);
    auto_gain_ = enabled;
}

void AudioEngine::setEchoEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(echo_mutex_);
    echo_enabled_.store(enabled && aec_ != nullptr);
}

void AudioEngine::setHearTargets(const std::unordered_set<std::string>& hear_ids) {
    std::lock_guard<std::mutex> lock(rx_mutex_);
    hear_targets_ = hear_ids;
}

void AudioEngine::setTxMuted(bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    tx_muted_ = enabled;
}

bool AudioEngine::isTxMuted() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return tx_muted_;
}

int AudioEngine::testMicrophoneLevel(double duration_sec) {
    duration_sec = std::max(0.2, duration_sec);

    auto& pa = portAudioApi();
    PaDeviceIndex device = resolveInputDevice(input_device_index_);
    if (device == paNoDevice) {
        return 0;
    }

    const PaDeviceInfo* info = pa.GetDeviceInfo(device);
    if (!info) {
        return 0;
    }

    PaStreamParameters params{};
    params.device = device;
    params.channelCount = 1;
    params.sampleFormat = paInt16;
    params.suggestedLatency = info->defaultLowInputLatency;

    PaStream* stream = nullptr;
    PaError err = pa.OpenStream(&stream, &params, nullptr, RATE, FRAME, paNoFlag, nullptr, nullptr);
    if (err != paNoError || !stream) {
        return 0;
    }
    if (pa.StartStream(stream) != paNoError) {
        pa.CloseStream(stream);
        return 0;
    }

    std::vector<int16_t> frame(FRAME, 0);
    int max_peak = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(duration_sec * 1000));
    while (std::chrono::steady_clock::now() < deadline) {
        err = pa.ReadStream(stream, frame.data(), FRAME);
        if (err != paNoError) {
            break;
        }
        for (const auto sample : frame) {
            max_peak = std::max(max_peak, static_cast<int>(std::abs(sample)));
        }
    }

    pa.StopStream(stream);
    pa.CloseStream(stream);

    if (max_peak <= 0) {
        return 0;
    }
    return std::min(100, static_cast<int>((max_peak * 100) / 32767));
}

bool AudioEngine::openOutput() {
    if (wave_out_) {
        return true;
    }

    auto& pa = portAudioApi();
    PaDeviceIndex device = resolveOutputDevice(output_device_index_);
    if (device == paNoDevice) {
        return false;
    }

    const PaDeviceInfo* info = pa.GetDeviceInfo(device);
    if (!info) {
        return false;
    }

    PaStreamParameters params{};
    params.device = device;
    params.channelCount = 1;
    params.sampleFormat = paInt16;
    params.suggestedLatency = info->defaultLowOutputLatency;

    PaStream* stream = nullptr;
    PaError err = pa.OpenStream(&stream, nullptr, &params, RATE, FRAME, paNoFlag, &paOutputCallback, this);
    if (err != paNoError || !stream) {
        std::cerr << "[AUDIO] PortAudio output open failed: " << paErrorText(err) << "\n";
        return false;
    }
    err = pa.StartStream(stream);
    if (err != paNoError) {
        pa.CloseStream(stream);
        std::cerr << "[AUDIO] PortAudio output start failed: " << paErrorText(err) << "\n";
        return false;
    }

    wave_out_ = stream;
    playback_running_.store(true);
    return true;
}

void AudioEngine::closeOutput() {
    playback_running_.store(false);
    if (!wave_out_) {
        return;
    }

    auto& pa = portAudioApi();
    PaStream* stream = reinterpret_cast<PaStream*>(wave_out_);
    pa.StopStream(stream);
    pa.CloseStream(stream);
    wave_out_ = nullptr;
}

bool AudioEngine::openInput() {
    if (wave_in_) {
        return true;
    }

    auto& pa = portAudioApi();
    PaDeviceIndex device = resolveInputDevice(input_device_index_);
    if (device == paNoDevice) {
        return false;
    }

    const PaDeviceInfo* info = pa.GetDeviceInfo(device);
    if (!info) {
        return false;
    }

    PaStreamParameters params{};
    params.device = device;
    params.channelCount = 1;
    params.sampleFormat = paInt16;
    params.suggestedLatency = info->defaultLowInputLatency;

    PaStream* stream = nullptr;
    PaError err = pa.OpenStream(&stream, &params, nullptr, RATE, FRAME, paNoFlag, &paInputCallback, this);
    if (err != paNoError || !stream) {
        std::cerr << "[AUDIO] PortAudio input open failed: " << paErrorText(err) << "\n";
        return false;
    }
    err = pa.StartStream(stream);
    if (err != paNoError) {
        pa.CloseStream(stream);
        std::cerr << "[AUDIO] PortAudio input start failed: " << paErrorText(err) << "\n";
        return false;
    }

    wave_in_ = stream;
    return true;
}

void AudioEngine::closeInput() {
    if (!wave_in_) {
        return;
    }

    auto& pa = portAudioApi();
    PaStream* stream = reinterpret_cast<PaStream*>(wave_in_);
    pa.StopStream(stream);
    pa.CloseStream(stream);
    wave_in_ = nullptr;
}

void AudioEngine::listenLoop() {
    std::cout << "[AUDIO] Listening for audio on port " << port_ << "\n";
    while (listen_running_.load()) {
        std::vector<uint8_t> buffer(4096);
        sockaddr_in src{};
        int src_len = sizeof(src);
        const int recv_len = recvfrom(recv_sock_, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&src), &src_len);
        if (recv_len <= 0) {
            if (!listen_running_.load()) {
                break;
            }
            continue;
        }
        buffer.resize(static_cast<size_t>(recv_len));
        handleIncomingPacket(buffer);
    }
}

void AudioEngine::handleIncomingPacket(const std::vector<uint8_t>& data) {
    const auto packet = parse_voice_packet(data);
    if (!packet.has_value() || packet->kind != VoicePacketKind::ClientAudio ||
        packet->sender_id.empty() || packet->sender_id == client_id_) {
        return;
    }

    std::vector<int16_t> pcm;
    {
        std::lock_guard<std::mutex> lock(decoder_mutex_);
        pcm = decoder_.decode(packet->payload);
    }
    if (pcm.empty()) {
        return;
    }
    if (pcm.size() != FRAME) {
        pcm.resize(FRAME, 0);
    }

    std::lock_guard<std::mutex> lock(rx_mutex_);
    rx_last_pcm_[packet->sender_id] = std::move(pcm);
    rx_last_rx_time_[packet->sender_id] = std::chrono::steady_clock::now();
}

std::vector<int16_t> AudioEngine::mixFrame() {
    const auto now = std::chrono::steady_clock::now();

    std::vector<int32_t> sum(FRAME, 0);
    int active_speakers = 0;

    {
        std::lock_guard<std::mutex> lock(rx_mutex_);
        for (auto it = rx_last_pcm_.begin(); it != rx_last_pcm_.end(); ) {
            const std::string& id = it->first;
            const auto& pcm = it->second;

            auto time_it = rx_last_rx_time_.find(id);
            const bool expired = (time_it == rx_last_rx_time_.end()) ||
                (std::chrono::duration_cast<std::chrono::milliseconds>(now - time_it->second).count() > 600);

            if (expired) {
                it = rx_last_pcm_.erase(it);
                rx_last_rx_time_.erase(id);
                continue;
            }

            if (hear_targets_.count(id)) {
                for (int i = 0; i < FRAME && i < static_cast<int>(pcm.size()); ++i) {
                    sum[i] += pcm[i];
                }
                ++active_speakers;
            }
            ++it;
        }
    }

    std::vector<int16_t> mix(FRAME);
    for (int i = 0; i < FRAME; ++i) {
        mix[i] = static_cast<int16_t>(sum[i]);
    }

    // Dynamic gain reduction when multiple people speak (removes distortion)
    float mix_gain = (active_speakers > 1) ? (0.85f / active_speakers) : 1.0f;

    for (auto& sample : mix) {
        const int32_t val = static_cast<int32_t>(sample * mix_gain);
        sample = static_cast<int16_t>(std::clamp(val, -32768, 32767));
    }

    float master = 1.0f;
    float output = 1.0f;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        master = master_volume_;
        output = output_volume_;
    }
    const float volume_factor = std::clamp(master * output, 0.0f, 2.0f);
    if (std::abs(volume_factor - 1.0f) > 0.0001f) {
        for (auto& sample : mix) {
            const int scaled = static_cast<int>(sample * volume_factor);
            sample = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
        }
    }

    // AEC reverse (if enabled)
    if (echo_enabled_.load() && aec_) {
        try {
            std::lock_guard<std::mutex> lock(echo_mutex_);
            if (echo_enabled_.load() && aec_) {
                aec_->process_render(mix.data(), FRAME);
            }
        } catch (...) {
            echo_enabled_.store(false);
        }
    }

    updateMixedLevel(mix);
    return mix;
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
    if (!samples || sample_count <= 0 || !running_.load()) {
        return;
    }

    std::vector<int16_t> frame(samples, samples + sample_count);
    if (frame.size() < FRAME) {
        frame.resize(FRAME, 0);
    } else if (frame.size() > FRAME) {
        frame.resize(FRAME);
    }

    {
        std::lock_guard<std::mutex> lock(capture_mutex_);
        if (capture_frames_.size() >= CAPTURE_QUEUE_MAX) {
            capture_frames_.pop_front();
        }
        capture_frames_.push_back(std::move(frame));
    }
    capture_cv_.notify_one();
}

bool AudioEngine::popCaptureFrame(std::vector<int16_t>& out) {
    std::unique_lock<std::mutex> lock(capture_mutex_);
    capture_cv_.wait_for(lock, std::chrono::milliseconds(50), [this]() {
        return !capture_frames_.empty() || !running_.load();
    });
    if (capture_frames_.empty()) {
        return false;
    }
    out = std::move(capture_frames_.front());
    capture_frames_.pop_front();
    return true;
}

bool AudioEngine::start(const std::vector<std::string>& destinations) {
    if (client_id_.empty()) {
        return false;
    }
    if (running_.load()) {
        return true;
    }
    if (destinations.empty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(routing_mutex_);
        send_destinations_ = destinations;
    }
    transport_.setDestinations(destinations);
    if (!transport_.hasDestinations()) {
        return false;
    }

    if (!openInput()) {
        std::cerr << "[AUDIO] Failed to start capture\n";
        return false;
    }

    send_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (send_sock_ == INVALID_SOCKET) {
        closeInput();
        return false;
    }
    const int buf_size = 65536;
    setsockopt(send_sock_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&buf_size), sizeof(buf_size));
    socket_utils::set_dscp(send_sock_, IP_TOS_EF);

    running_.store(true);
    send_thread_ = std::thread(&AudioEngine::sendLoop, this);

    auto active_destinations = transport_.destinations();
    std::cout << "[AUDIO] Audio capture ACTIVE for " << client_id_ << " -> ";
    for (size_t i = 0; i < active_destinations.size(); ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << active_destinations[i];
        if (active_destinations[i].find(':') == std::string::npos) {
            std::cout << ":" << AUDIO_PORT;
        }
    }
    std::cout << "\n";
    return true;
}

bool AudioEngine::updateDestinations(const std::vector<std::string>& destinations) {
    if (destinations.empty()) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(routing_mutex_);
        send_destinations_ = destinations;
    }
    transport_.setDestinations(destinations);
    return transport_.hasDestinations();
}

void AudioEngine::sendLoop() {
    int packet_count = 0;
    while (running_.load()) {
        std::vector<int16_t> frame;
        if (!popCaptureFrame(frame)) continue;

        // 1. RNNoise noise suppression (on raw mic)
        if (noise_suppression_enabled_ && rnnoise_ && rnnoise_->isAvailable()) {
            rnnoise_->process(frame);
        }

        bool tx_muted = false;
        float tx_gain_db = 0.0f;
        int mic_sens = 35;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            tx_muted = tx_muted_;
            tx_gain_db = tx_gain_db_;
            mic_sens = mic_sensitivity_;
        }

        bool should_send = false;
        if (tx_muted) {
            std::fill(frame.begin(), frame.end(), 0);
        } else {
            // === Apply TX gain + mic sensitivity ===
            const float gain_linear = std::pow(10.0f, tx_gain_db / 20.0f);
            const float sens_linear = static_cast<float>(mic_sens) / 35.0f;
            const float total_gain = gain_linear * sens_linear;
            for (auto& s : frame) {
                const float amplified = static_cast<float>(s) * total_gain;
                s = static_cast<int16_t>(std::clamp(amplified, -32768.0f, 32767.0f));
            }

            // === VAD - ALWAYS RUN (AEC is still optional) ===
            if (aec_) {
                aec_->process_capture(frame);   // ← This was the main bug
            }
            const bool voice_detected = aec_ ? aec_->hasVoice() : false;

            static int silence_frames = 0;
            constexpr int VAD_HANGOVER_FRAMES = 12;
            if (voice_detected) {
                silence_frames = 0;
                should_send = true;
            } else {
                silence_frames++;
                should_send = (silence_frames <= VAD_HANGOVER_FRAMES);
            }
        }

        // Update UI level from processed frame
        int input_peak = 0;
        for (const auto sample : frame) {
            input_peak = std::max(input_peak, static_cast<int>(std::abs(sample)));
        }
        capture_level_.store(std::min(100, (input_peak * 100) / 32767));
        capture_active_.store(should_send && !tx_muted);

        if (!should_send) continue;

        const std::vector<uint8_t> opus = encoder_.encode(frame);
        if (opus.empty()) continue;

        std::vector<uint8_t> packet = build_client_audio_packet(client_id_, seq_, timestamp_, opus);
        if (packet.empty()) {
            continue; // encryption failed; drop frame rather than leak plaintext
        }

        seq_ = static_cast<uint16_t>((seq_ + 1) & 0xFFFF);
        timestamp_ += FRAME;
        const int sent = transport_.sendPacket(send_sock_, packet);
        if (sent > 0) ++packet_count;
    }
}


void AudioEngine::stop() {
    running_.store(false);
    capture_cv_.notify_all();
    if (send_thread_.joinable()) {
        send_thread_.join();
    }

    capture_active_.store(false);
    capture_level_.store(0);

    closeInput();

    if (send_sock_ != INVALID_SOCKET) {
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

    if (recv_sock_ != INVALID_SOCKET) {
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
