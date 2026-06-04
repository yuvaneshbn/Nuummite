# Nuummite

Nuummite is a Windows LAN voice chat project built around:

- a Qt 6 desktop client
- a P2P discovery layer over UDP multicast
- an Opus-based audio pipeline
- optional RNNoise and WebRTC audio processing
- libsodium for packet encryption

## Repository Layout

- `Nuummite/` - main application sources
- `Nuummite/audio/` - capture, playback, and DSP code
- `Nuummite/p2p/` - peer discovery and transport code
- `Nuummite/client/` - Qt UI and client entry point
- `third_party/` - vendored runtime and import libraries
- `tools/` - diagnostic utilities

## Requirements

- Windows 10 or 11
- Visual Studio 2022 with MSVC v143
- CMake 3.20 or newer
- Ninja
- Qt 6.11.1 for MSVC x64

The project also expects the bundled third-party binaries and import libraries in `third_party/`.

## Build

From a Visual Studio Build Tools developer command prompt, or by calling the
developer shell batch file directly:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
cmake --build build-msvc
```

## Run

After a successful build, run the client from the build output directory:

```bat
build-msvc\bin\voice_client.exe
```

The diagnostic audio flow tool is built alongside it:

```bat
build-msvc\bin\audio_flow_test.exe
```

## Runtime Notes

- Discovery uses UDP port `50000`
- Audio transport uses UDP port `50002`
- The app expects runtime DLLs such as `opus.dll`, `libportaudio.dll`, `libsodium.dll`, `libsodium-26.dll`, `rnnoise.dll`, `webrtc-audio-processing-1-3.dll`, and `webrtc-audio-coding-1-3.dll` to be available beside the executable
- `build-msvc\bin\` also contains the Qt runtime folders and DLLs copied by CMake during post-build steps

## Troubleshooting

- If CMake cannot find Qt, verify `CMAKE_PREFIX_PATH` points to your Qt MSVC x64 install.
- If the client fails to launch, confirm the third-party DLLs were copied into `build-msvc\bin`.
- If peers do not appear, make sure local firewall rules allow UDP traffic on ports `50000` and `50002`.
