#include "audio/audio_engine.h"

#include <algorithm>
#include <chrono>
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

        AudioEngine audio;
        audio.setClientId("audio-flow-test");
        audio.setRoomSecret("audio-flow-test");

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
    } catch (const std::exception& e) {
        std::cerr << "audio flow test failed: " << e.what() << "\n";
        return 1;
    }
}
