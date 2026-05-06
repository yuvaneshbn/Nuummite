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
#include <cmath>
#include <cstdlib>
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
    audio->renderOutput(reinterpret_cast<int16_t*>(output), static_cast<int>(frame_count));
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
    // Debug-safe VOIP settings: one encoder for this client's outgoing stream.
    : encoder_(RATE, 1, FRAME, false, 0, 48000, 10, false, OPUS_APPLICATION_VOIP, true, false) {
    SodiumWrapper::init();

    {
        char* value = nullptr;
        size_t len = 0;
        if (_dupenv_s(&value, &len, "NUUMMITE_AUDIO_DEBUG") == 0 && value) {
            audio_debug_ = (*value != '\0' && *value != '0');
            free(value);
        }
    }

    if (!pure_opus_) {
        rnnoise_ = std::make_unique<RnNoiseProcessor>();
        aec_ = std::make_unique<AecProcessor>(RATE, FRAME);
        if (aec_) {
            aec_->setEchoEnabled(false);   // start disabled - enable only after testing clean path
            aec_->set_stream_delay_ms(180);
        }
    }
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

    echo_enabled_.store(false);   // force off initially

    // Preallocate output/mix scratch buffers once (callback-safe).
    mix_accum_.assign(FRAME, 0.0f);
    mix_frame_.assign(FRAME, 0);
    playback_fifo_.assign(static_cast<size_t>(FRAME) * 8u, 0); // ~160ms FIFO
    fifo_read_ = fifo_write_ = fifo_size_ = 0;

    
 
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
    master_volume_ = std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 2.5f);
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
    noise_suppression_enabled_ = pure_opus_ ? false : enabled;
}

void AudioEngine::setAutoGain(bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    auto_gain_ = pure_opus_ ? false : enabled;

    if (aec_) {
        aec_->setAutoGainEnabled(auto_gain_);
    }
}

void AudioEngine::setEchoEnabled(bool enabled) {
    (void)enabled;
    std::lock_guard<std::mutex> lock(echo_mutex_);
    echo_enabled_.store(false);
    if (aec_) {
        aec_->setEchoEnabled(false);
    }
}

void AudioEngine::setAecStreamDelayMs(int delay_ms) {
    if (pure_opus_) {
        return;
    }
    delay_ms = std::clamp(delay_ms, 0, 500);
    aec_stream_delay_ms_.store(delay_ms);
    if (aec_) {
        aec_->set_stream_delay_ms(delay_ms);
    }
}

void AudioEngine::setHearTargets(const std::unordered_set<std::string>& hear_ids) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    hear_targets_ = hear_ids;
}

std::shared_ptr<AudioEngine::StreamState> AudioEngine::getOrCreateStream(const std::string& id) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto& st = streams_[id];
    if (!st) {
        st = std::make_shared<StreamState>();
    }
    return st;
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
    params.suggestedLatency = info->defaultHighOutputLatency;

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

    auto st = getOrCreateStream(packet->sender_id);
    if (!st) {
        return;
    }

    std::lock_guard<std::mutex> lock(st->mtx);
    if (!st->decoder) {
        st->decoder = std::make_unique<OpusCodec>(
            RATE, 1, FRAME,
            true, 10, 24000, 10,
            false,
            OPUS_APPLICATION_VOIP,
            false, true
        );
    }

    // HANDLE PACKET LOSS (PLC)
    if (st->has_seq) {
        const uint16_t expected = static_cast<uint16_t>(st->last_seq + 1);

        if (packet->seq != expected) {
            const uint16_t delta = static_cast<uint16_t>((packet->seq - expected) & 0xFFFF);
            // Drop obviously out-of-order/duplicate packets rather than generating massive PLC.
            if (delta > 1000) {
                return;
            }
            const uint16_t lost = delta;
            if (lost > 0) {
                static thread_local auto last_loss_log = std::chrono::steady_clock::now();
                const auto now = std::chrono::steady_clock::now();
                if (audio_debug_ && (now - last_loss_log) > std::chrono::seconds(1)) {
                    last_loss_log = now;
                    std::cout << "[AUDIO] plc_lost_frames=" << lost << " sender=" << packet->sender_id << "\n";
                }
            }

            for (int i = 0; i < static_cast<int>(lost); ++i) {
                std::vector<int16_t> plc = st->decoder->decode(nullptr, 0);
                if (plc.size() != static_cast<size_t>(FRAME)) {
                    plc.resize(FRAME, 0);
                }
                st->jitter.push_back(std::move(plc));
            }
        }
    }

    st->has_seq = true;
    st->last_seq = packet->seq;

    // DECODE REAL FRAME
    std::vector<int16_t> pcm = st->decoder->decode(packet->payload);
    if (pcm.size() != static_cast<size_t>(FRAME)) {
        pcm.resize(FRAME, 0);
    }

    // PUSH TO JITTER BUFFER
    st->jitter.push_back(std::move(pcm));

    // limit buffer
    while (st->jitter.size() > 5) {
        st->jitter.pop_front();
    }
}

void AudioEngine::renderOutput(int16_t* out, int sample_count) {
    if (!out || sample_count <= 0) {
        return;
    }

    const size_t need = static_cast<size_t>(sample_count);
    if (playback_fifo_.empty() || mix_frame_.size() != static_cast<size_t>(FRAME) ||
        mix_accum_.size() != static_cast<size_t>(FRAME)) {
        std::fill(out, out + need, 0);
        return;
    }

    const size_t cap = playback_fifo_.size();

    auto fifoWrite = [&](const int16_t* data, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (fifo_size_ >= cap) {
                // Drop oldest sample rather than blocking the audio callback.
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

        // Never block the audio callback on locks held by network/UI threads.
        std::unique_lock<std::mutex> streams_lock(streams_mutex_, std::try_to_lock);
        if (!streams_lock.owns_lock()) {
            std::fill(mix_frame_.begin(), mix_frame_.end(), 0);
            updateMixedLevel(mix_frame_);
            return;
        }

        const auto hear_targets = hear_targets_;
        std::vector<std::pair<std::string, std::shared_ptr<StreamState>>> snapshot;
        snapshot.reserve(streams_.size());
        for (const auto& [id, st] : streams_) {
            snapshot.emplace_back(id, st);
        }
        streams_lock.unlock();

        for (auto& [id, st] : snapshot) {
            if (!st) {
                continue;
            }
            if (!hear_targets.count(id)) {
                continue;
            }

            std::unique_lock<std::mutex> st_lock(st->mtx, std::try_to_lock);
            if (!st_lock.owns_lock() || st->jitter.empty()) {
                continue;
            }

            auto frame = std::move(st->jitter.front());
            st->jitter.pop_front();

            active++;
            for (int i = 0; i < FRAME; ++i) {
                mix_accum_[i] += static_cast<float>(frame[i]);
            }
        }

        if (active == 0) {
            std::fill(mix_frame_.begin(), mix_frame_.end(), 0);
            updateMixedLevel(mix_frame_);
            return;
        }

        // LIMITER + SOFT CLIP (float domain)
        for (float v : mix_accum_) {
            peak = std::max(peak, static_cast<float>(std::abs(v)));
        }

        const float pre_scale = (peak > 28000.0f) ? (28000.0f / peak) : 1.0f;
        for (int i = 0; i < FRAME; ++i) {
            mix_accum_[i] = (mix_accum_[i] * pre_scale) / 32768.0f;
        }
        static float softclip_mem[1] = {0.0f};
        opus_pcm_soft_clip(mix_accum_.data(), FRAME, 1, softclip_mem);

        for (int i = 0; i < FRAME; ++i) {
            const float v = mix_accum_[i] * 32767.0f;
            mix_frame_[i] = static_cast<int16_t>(std::clamp(v, -32768.0f, 32767.0f));
        }

        static auto last_debug = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        if (now - last_debug > std::chrono::seconds(1)) {
            last_debug = now;
            if (audio_debug_) {
                std::cout << "[AUDIO] active_clients=" << active << " pre_limiter_peak=" << peak << " queues=[";
                for (const auto& [id, st] : snapshot) {
                    if (!st) {
                        continue;
                    }
                    std::unique_lock<std::mutex> st_lock(st->mtx, std::try_to_lock);
                    if (st_lock.owns_lock()) {
                        std::cout << st->jitter.size() << ",";
                    }
                }
                std::cout << "]\n";
            }
        }

        if (!pure_opus_) {
            // Feed reference to AEC before applying user volume scaling.
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
    while (running_.load()) {
        std::vector<int16_t> frame;
        if (!popCaptureFrame(frame)) continue;

        bool tx_muted_local = false;
        bool autogain_local = false;
        bool rnnoise_local = false;
        float tx_gain_db_local = 0.0f;
        int mic_sens_local = 50;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            tx_muted_local = tx_muted_;
            autogain_local = auto_gain_;
            rnnoise_local = noise_suppression_enabled_;
            tx_gain_db_local = tx_gain_db_;
            mic_sens_local = mic_sensitivity_;
        }

        if (!pure_opus_) {
            // === 1. WebRTC APM - AEC + VAD ===
            bool voice_detected = false;
            const bool want_apm = !tx_muted_local && aec_ && aec_->available() && (echo_enabled_.load() || autogain_local);
            if (want_apm) {
                aec_->set_stream_delay_ms(aec_stream_delay_ms_.load());
                aec_->process_capture(frame);  // runs AEC when enabled, runs AGC when enabled
                voice_detected = aec_->hasVoice();
            } else {
                int peak = 0;
                for (const auto sample : frame) {
                    peak = std::max(peak, std::abs(static_cast<int>(sample)));
                }
                voice_detected = peak > 400;
            }

            // === 2. RNNoise (noise suppression) ===
            if (!tx_muted_local && rnnoise_local && rnnoise_ && rnnoise_->isAvailable()) {
                rnnoise_->process(frame);
            }

            // === 3. Transmit gain ===
            // If Auto-Gain is enabled, avoid applying manual gain on top.
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

            // === 4. VAD with Hangover ===
            static int silence_frames = 0;
            constexpr int VAD_HANGOVER_FRAMES = 14;   // ~280ms

            if (voice_detected) {
                silence_frames = 0;
            } else {
                silence_frames++;
            }

            const bool should_send = !tx_muted_local && (voice_detected || silence_frames <= VAD_HANGOVER_FRAMES);

            // === 5. Update UI levels ===
            int input_peak = 0;
            for (const auto sample : frame) {
                input_peak = std::max(input_peak, std::abs(static_cast<int>(sample)));
            }
            capture_level_.store(std::min(100, (input_peak * 100) / 32767));
            capture_active_.store(should_send);

            if (!should_send) continue;
        } else {
            // Pure Opus debug: no AEC/RNNoise/VAD/AGC/manual gain. Always transmit raw frames.
            if (tx_muted_local) {
                std::fill(frame.begin(), frame.end(), 0);
            }

            int input_peak = 0;
            for (const auto sample : frame) {
                input_peak = std::max(input_peak, std::abs(static_cast<int>(sample)));
            }
            capture_level_.store(std::min(100, (input_peak * 100) / 32767));
            capture_active_.store(!tx_muted_local);

            if (tx_muted_local) continue;
        }

        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            tx_muted_local = tx_muted_;
        }

        // === 6. Opus encode + send ===
        const std::vector<uint8_t> opus_data = encoder_.encode(frame);
        if (opus_data.empty()) continue;

        std::vector<uint8_t> packet = build_client_audio_packet(client_id_, seq_, timestamp_, opus_data);
        if (packet.empty()) continue;

        seq_ = static_cast<uint16_t>((seq_ + 1) & 0xFFFF);
        timestamp_ += FRAME;

        transport_.sendPacket(send_sock_, packet);
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
