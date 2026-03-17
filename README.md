# Nuummite — LAN P2P Voice Client for Windows

Lightweight Qt 6 voice chat client that discovers peers on the local network, sends/receives Opus audio over UDP, and requires **no central server**. Everything here targets Windows.

## What it does
- Peer discovery by broadcast on UDP `50000` (prefix `VOICE_PEER:`)  
- Voice transport on UDP `50002` with Opus @ 48 kHz, 20 ms mono frames  
- Per‑peer talk/mute selection and transmit‑on‑target behavior  
- Runtime UI for device selection, input/output gain, and simple suppression  
- Runs as a single executable (`voice_client.exe`); no backend process

## Repository layout
```
.
|-- CMakeLists.txt           # Top‑level build (creates voice_client + voice_shared)
|-- CMakePresets.json        # Ready‑to‑use presets for MSVC & Ninja
|-- Nuummite/                # Application sources (audio, p2p, Qt UI)
|-- third_party/opus/        # Prebuilt Opus DLL/import lib + headers
|-- third_party/Qt6/         # Optional Qt plugins stash (not a full SDK)
|-- vcpkg.json               # Manifest: qtbase, qttools, opus
`-- vcpkg_installed/         # Local vcpkg artifacts (generated)
```

## Prerequisites
- Windows 10/11
- CMake ≥ 3.20
- Visual Studio 2022 with **MSVC x64** (Desktop C++ workload)  
  Ninja is optional for single‑config builds.
- Git + vcpkg (manifest mode)  
  Set `VCPKG_ROOT` to your vcpkg clone so the presets can locate the toolchain.
- Qt 6 (Widgets + UiTools). The manifest will fetch Qt through vcpkg; if you have a local Qt install, set `QT6_ROOT`.

## Installing vcpkg on an offline machine

If you need to build on a machine without internet access, follow these steps:

1. On an online machine:
      - Download the vcpkg repository as a ZIP from https://github.com/microsoft/vcpkg (or clone it).
      - Download any required dependencies (CMake, Git, Visual Studio Build Tools, etc.).
      - Download the ports/packages you need (see vcpkg docs for offline builds).
2. Transfer the vcpkg ZIP and all downloaded files to the offline machine (USB drive, etc.).
3. On the offline machine:
      - Extract the vcpkg ZIP to a folder (e.g., `C:\vcpkg`).
      - Open a command prompt in that folder.
      - Run:
         ```
         bootstrap-vcpkg.bat
         ```
      - If you need specific libraries, download their source archives on the online machine and place them in the `downloads` folder inside vcpkg before running `vcpkg install`.
4. Set the environment variable:
      ```
      setx VCPKG_ROOT "C:\vcpkg"
      ```
5. Use vcpkg in manifest mode as usual.

For Qt, Opus, PortAudio, etc., manually download their source or binaries and place them in the correct vcpkg folders. See vcpkg documentation for "offline builds" and "binary caching" for advanced usage.


## Quick start (recommended: MSVC + vcpkg presets)
1) Install vcpkg (once):
   ```powershell
   git clone https://github.com/microsoft/vcpkg $env:USERPROFILE/vcpkg
   $env:USERPROFILE/vcpkg/bootstrap-vcpkg.bat
   setx VCPKG_ROOT "$env:USERPROFILE\\vcpkg"
   ```
2) Clone the repo and open a **x64 Native Tools** VS 2022 prompt (or a PowerShell with the VS dev tools in PATH):
   ```powershell
   cd C:\Users\YUVANESH\Desktop\projects\new\oyx-main
   ```
3) Configure with the Visual Studio preset (downloads Qt/Opus via vcpkg as needed):
   ```powershell
   cmake --preset vs-release
   ```
4) Build:
   ```powershell
   cmake --build --preset vs-release
   ```
5) Run:
   ```powershell
   .\build\bin\Release\voice_client.exe
   ```
   On first launch, allow Windows Firewall for UDP `50000`/`50002`.

### Alternate: Ninja single-config
```powershell
cmake --preset ninja-debug    # or ninja-release
cmake --build --preset ninja-debug
.\build-ninja-debug\voice_client.exe
```

## Key CMake cache variables
- `QT6_ROOT` (PATH) – Qt install root (`C:/Qt/6.7.3` etc.); auto-derives `Qt6_DIR`.
- `PORTAUDIO_DLL` (FILEPATH) – path to `libportaudio.dll` to copy beside the exe.
- `VOICE_BUILD_CLIENT` (BOOL, default ON) – client target.
- `VOICE_RUN_QT_DEPLOY` (BOOL, default ON) – run `windeployqt` after build if available.
- `QT6_ROOT`, `PORTAUDIO_DLL`, `OPUS_DLL`, `OPUS_LIB` can be overridden to use custom binaries.
- Optional compile definition `VOICE_PORTAUDIO_DLL_FALLBACK` provides an extra runtime search path for PortAudio.

The build copies, post‑build:
- `Nuummite/ui` → `<out>/ui`
- `Nuummite/technical-support.ico`
- `third_party/opus/opus.dll`
- `PORTAUDIO_DLL` as `libportaudio.dll`
If `windeployqt.exe` is found (Qt SDK or vcpkg tools), it is invoked automatically.

## Runtime checklist
- `voice_client.exe` plus `opus.dll`, `libportaudio.dll`, Qt DLLs/plugins must live together (handled by the post‑build steps above).
- UDP broadcast (50000) must be allowed on the LAN for peer discovery.
- UDP 50002 must be open for audio. Peers choose their own ID at startup; duplicates are replaced by last‑seen.
- Optional env var: `VOICE_REGISTER_SECRET` (defaults to `mysecret`; currently only stored in the UI).

## Using the app
1) Launch `voice_client.exe`.
2) Enter a unique **Client ID** when prompted.
3) The app advertises itself every second and listens for other `VOICE_PEER` beacons.
4) Select peers in the UI to start transmitting; capture only runs while at least one destination is chosen.
5) Adjust mic/output gain and suppression in Settings; choose input/output devices as needed.

## Troubleshooting
- **No peers found:** ensure all machines are on the same subnet and firewall allows UDP broadcast on `50000`.
- **No audio:** confirm `libportaudio.dll` is beside the exe; verify input/output devices in Settings.
- **Qt DLL errors:** rerun build with `VOICE_RUN_QT_DEPLOY=ON` (default) or run `windeployqt` manually on `voice_client.exe`.
- **Using a different PortAudio path:** reconfigure with `-DPORTAUDIO_DLL=C:/path/to/libportaudio.dll` and rebuild.

## Development notes
- Build outputs: executables/DLLs → `build/bin`, static libs → `build/lib`.
- DSCP for audio is set to EF (46) for low‑latency marking.
- Sample rate is fixed at 48 kHz; frames are 20 ms (960 samples).

## License
MIT license in `LICENSE`. Review third‑party licenses for Qt, Opus, PortAudio, and any vendored code.
