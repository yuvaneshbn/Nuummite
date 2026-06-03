#include "audio/audio_engine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <mmsystem.h>

namespace {

void printDevices(const char* label, const std::vector<AudioDeviceInfo>& devices) {
    std::cout << label << ":\n";
    for (const auto& dev : devices) {
        std::cout << "  [" << dev.index << "] " << dev.name << "\n";
    }
}

int parseArg(int argc, char** argv, const char* key, int fallback) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == key) {
            try {
                return std::stoi(argv[i + 1]);
            } catch (...) {
                return fallback;
            }
        }
    }
    return fallback;
}

std::string parseStringArg(int argc, char** argv, const char* key, const std::string& fallback) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == key) {
            return argv[i + 1];
        }
    }
    return fallback;
}

bool hasArg(int argc, char** argv, const char* key) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return true;
        }
    }
    return false;
}

void printWaveInDevices() {
    const UINT count = waveInGetNumDevs();
    std::cout << "Windows waveIn devices (" << count << "):\n";
    for (UINT i = 0; i < count; ++i) {
        WAVEINCAPSW caps{};
        if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            const int required = WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, nullptr, 0, nullptr, nullptr);
            std::string utf8;
            if (required > 0) {
                utf8.resize(static_cast<size_t>(required - 1));
                WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, utf8.data(), required, nullptr, nullptr);
            }
            std::cout << "  [" << i << "] " << (utf8.empty() ? "<unnamed>" : utf8) << "\n";
        } else {
            std::cout << "  [" << i << "] <unavailable>\n";
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const int seconds = std::max(2, parseArg(argc, argv, "--seconds", 5));
        const int input_index = parseArg(argc, argv, "--input", -1);
        const int output_index = parseArg(argc, argv, "--output", -1);
        const bool no_output = hasArg(argc, argv, "--no-output");
        const bool pair_mode = hasArg(argc, argv, "--pair");
        const std::string room = parseStringArg(argc, argv, "--room", "audio-flow-test");
        const std::string sender_id = parseStringArg(argc, argv, "--sender-id", "sender");
        const std::string receiver_id = parseStringArg(argc, argv, "--receiver-id", "receiver");

        if (!pair_mode) {
            AudioEngine audio;
            audio.setClientId(room);
            audio.setRoomSecret(room);

            printDevices("Input devices", audio.listInputDevices());
            printDevices("Output devices", audio.listOutputDevices());
            printWaveInDevices();

            std::cout << "Requested input index: " << input_index << "\n";
            std::cout << "Requested output index: " << output_index << "\n";
            audio.setInputDevice(input_index);
            if (!no_output) {
                audio.setOutputDevice(output_index);
            }

            std::cout << "Starting audio engine with empty destinations...\n";
            const bool started = audio.start({});
            std::cout << "start() returned: " << (started ? "true" : "false") << "\n";

            bool saw_level = false;
            for (int i = 0; i < seconds * 5; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                const int level = audio.captureLevel();
                const bool active = audio.captureActive();
                const bool running = audio.isRunning();
                std::cout << "t=" << (i + 1) * 0.2 << "s"
                          << " running=" << (running ? "1" : "0")
                          << " active=" << (active ? "1" : "0")
                          << " level=" << level << "\n";
                if (level > 0 || active) {
                    saw_level = true;
                }
            }

            audio.stop();
            audio.shutdown();

            std::cout << "audio flow test result: " << (saw_level ? "MIC CAPTURED" : "NO MIC ACTIVITY") << "\n";
            return saw_level ? 0 : 2;
        }

        AudioEngine receiver;
        receiver.setClientId(receiver_id);
        receiver.setRoomSecret(room);
        if (output_index >= 0) {
            receiver.setOutputDevice(output_index);
        }

        const bool receiver_started = receiver.start({}, false, true);
        std::cout << "receiver start() returned: " << (receiver_started ? "true" : "false") << "\n";

        const std::string receiver_endpoint = "127.0.0.1:" + std::to_string(receiver.port());
        std::cout << "receiver endpoint: " << receiver_endpoint << "\n";

        AudioEngine sender;
        sender.setClientId(sender_id);
        sender.setRoomSecret(room);
        if (input_index >= 0) {
            sender.setInputDevice(input_index);
        }

        const bool sender_started = sender.start({receiver_endpoint}, false, false);
        std::cout << "sender start() returned: " << (sender_started ? "true" : "false") << "\n";

        std::atomic<bool> tone_running{true};
        std::thread tone_thread([&]() {
            constexpr int kRate = 48000;
            constexpr int kFrame = 960;
            constexpr float kFrequency = 440.0f;
            constexpr float kAmplitude = 12000.0f;
            constexpr float kStep = 2.0f * 3.14159265358979323846f * kFrequency / static_cast<float>(kRate);
            float phase = 0.0f;
            std::array<int16_t, kFrame> frame{};
            while (tone_running.load(std::memory_order_relaxed) && sender.isRunning()) {
                for (int i = 0; i < kFrame; ++i) {
                    frame[static_cast<size_t>(i)] = static_cast<int16_t>(std::sin(phase) * kAmplitude);
                    phase += kStep;
                    if (phase > 2.0f * 3.14159265358979323846f) {
                        phase -= 2.0f * 3.14159265358979323846f;
                    }
                }
                sender.pushCaptureFrame(frame.data(), kFrame);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });

        bool saw_tx = false;
        bool saw_rx = false;
        bool saw_peer_peak = false;
        for (int i = 0; i < seconds * 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            const int tx_level = sender.captureLevel();
            const bool tx_active = sender.captureActive();
            const uint64_t tx_packets = sender.debugPacketsSent();
            const uint64_t rx_packets = receiver.debugPacketsRecv();
            const uint64_t rx_decrypted = receiver.debugPacketsDecrypted();
            const int peer_peak = receiver.getPeerPeak(sender_id);
            const float mixed_peak = receiver.mixedPeak();

            std::cout << "t=" << (i + 1) * 0.2 << "s"
                      << " tx_active=" << (tx_active ? "1" : "0")
                      << " tx_level=" << tx_level
                      << " tx_packets=" << tx_packets
                      << " rx_packets=" << rx_packets
                      << " rx_decrypted=" << rx_decrypted
                      << " peer_peak=" << peer_peak
                      << " mixed_peak=" << mixed_peak
                      << "\n";

            saw_tx = saw_tx || tx_active || tx_level > 0 || tx_packets > 0;
            saw_rx = saw_rx || rx_packets > 0 || rx_decrypted > 0;
            saw_peer_peak = saw_peer_peak || peer_peak > 0 || mixed_peak > 0.0f;
            if (saw_tx && saw_rx && saw_peer_peak) {
                break;
            }
        }

        tone_running.store(false, std::memory_order_relaxed);
        if (tone_thread.joinable()) {
            tone_thread.join();
        }

        sender.stop();
        receiver.stop();
        sender.shutdown();
        receiver.shutdown();

        const bool success = saw_tx && saw_rx;
        std::cout << "two-instance audio test result: "
                  << (success ? "AUDIO FLOW VERIFIED" : "AUDIO FLOW NOT VERIFIED")
                  << "\n";
        return success ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "audio flow test failed: " << e.what() << "\n";
        return 1;
    }
}
