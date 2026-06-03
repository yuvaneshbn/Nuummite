# Nuummite - Decentralized LAN P2P Voice Communication (Windows)

**High-performance, peer-to-peer voice communication for local networks with zero central server.**

Nuummite is a full-mesh, decentralized P2P voice communication platform built for local area networks (LANs) without central media routing servers. All routing, synchronization, and audio processing tasks run directly on edge clients. The system features a decoupled architecture separating the graphical user interface from real-time processing threads, ensuring low-latency, full-duplex audio with comprehensive acoustic preprocessing, end-to-end encryption, and adaptive network handling.

**Key Features:**
- ✅ Full-mesh peer-to-peer topology with zero central server
- ✅ UDP peer discovery on port 50000 (1000ms broadcast cycle, 3500ms peer pruning)
- ✅ Low-latency audio: 48 kHz @ 20ms frames (960 samples), Opus codec @ 48 kbps
- ✅ End-to-end encryption: libsodium secretbox (XSalsa20-Poly1305) with Argon2id KDF
- ✅ Multi-stage acoustic preprocessing: WebRTC AEC, RNNoise deep denoising, AGC
- ✅ Atomic double-buffering for lock-free UI/real-time thread synchronization
- ✅ Jitter buffer with adaptive target sizing and Opus packet loss concealment (PLC)
- ✅ Dual-driver fallback: WASAPI/PortAudio → WaveIn if primary fails
- ✅ Dynamic CPU protection during playback callbacks (x86 pause instructions)
- ✅ Peak-aware soft-clipping for multi-stream mixing
- ✅ DSCP QoS marking (DSCP 46, TOS 0xB8) for network prioritization
- ✅ Real-time device selection, volume controls, per-participant muting
- ✅ Packagable as standalone `.exe` with PyInstaller
- ✅ Native C++/Qt client available for embedded/advanced deployments

**Target OS:** Windows 10/11 (x64), Python 3.11+  
**Architecture:** C++ real-time audio engine (48 kHz, 20ms frames) + Qt6 UI  
**License:** MIT

---

## Table of Contents

1. [System Architecture](#system-architecture)
3. [Detailed Installation](#detailed-installation)
4. [Compilation & Build](#compilation--build)
5. [Running the Application](#running-the-application)
6. [Real-Time Audio Pipelines](#real-time-audio-pipelines)
7. [Development Workflow](#development-workflow)
8. [Packaging as .exe](#packaging-as-exe)
9. [Native C++/Qt Client](#native-c-qt-client)
10. [Troubleshooting](#troubleshooting)
11. [Runtime Configuration](#runtime-configuration)
12. [Network & Security](#network--security)

---

## System Architecture

### Architectural Foundation and Component Interconnections

The Nuummite client is built on a full-mesh topology with five primary subsystems operating across decoupled threads:

| Module | Files | Dependencies | Thread Domain | Runtime Resolution |
|--------|-------|--------------|----------------|-------------------|
| **Graphical User Interface** | MainWindow.cpp, SettingsDialog.cpp, VolumeControlPanel.cpp | Qt6 Gui, Widgets, Network | Main GUI Thread (Qt Event Loop) | Static CMake linkage |
| **Peer Discovery Protocol** | peer_discovery.cpp | Winsock2, IPHlpApi (GetAdaptersAddresses) | Dedicated Discovery Thread | Static ws2_32.lib, iphlpapi.lib |
| **Core Audio Engine** | audio_engine.cpp | PortAudio, Windows Multimedia (winmm.lib) | Hardware Callbacks & sendLoop | Dynamic libportaudio.dll loading |
| **Acoustic Preprocessor** | aec_processor.cpp, webrtc_apm.cpp, rnnoise_processor.cpp | WebRTC APM, RNNoise | sendLoop (TX) & Playback (RX) | Static DLL imports |
| **Cryptographic Shield** | libsodium_wrapper.cpp, audio_packet.cpp | Libsodium | Main GUI (KDF) & Real-Time Threads | Dynamic libsodium.dll loading |

### Dynamic System Initialization and Runtime Bootstrap Sequence

**Phase 1: Winsock & COM Initialization**
The application calls WSAStartup (Winsock v2.2) and CoInitializeEx(COINIT_MULTITHREADED) to prepare networking and COM libraries for multi-threaded WASAPI operations.

**Phase 2: Room Setup Dialog**
User enters client nickname, multicast/loopback room identifier, and passphrase.

**Phase 3: Cryptographic Key Derivation**
The passphrase is processed through libsodium's crypto_pwhash (Argon2id) with KDF salt "NuummiteVoiceKDF" to derive a 256-bit symmetric key. Fallback to crypto_generichash (BLAKE2b) if Argon2id unavailable.

**Phase 4: Audio Engine Allocation**
AudioEngine allocates a non-blocking UDP socket (recv_sock_) with SO_EXCLUSIVEADDRUSE and SO_RCVBUF (65,536 bytes). SIO_UDP_CONNRESET is disabled to prevent ICMP errors from crashing the receiver loop.

**Phase 5: Subsystem Thread Spawning**
- AudioEngine::listenLoop begins polling the media receiver socket
- PeerDiscovery::start initializes discovery broadcasts on local subnet

| Phase | API Functions | Security Flags | Created Handles | Target State |
|-------|---------------|----------------|-----------------|--------------|
| Winsock & COM | WSAStartup, CoInitializeEx | COINIT_MULTITHREADED | Static g_wsa handle | Networking & WASAPI initialized |
| Dynamic DLL Load | LoadLibraryExW, GetProcAddress | LOAD_LIBRARY_SEARCH_SYSTEM32 | HMODULE for Libsodium, PortAudio | DLL entry points resolved |
| KDF Pipeline | crypto_pwhash, crypto_generichash | Interactive ops/mem limits | Thread-safe atomic KDF key | 256-bit symmetric key derived |
| Socket Allocation | socket, setsockopt, WSAIoctl | SO_EXCLUSIVEADDRUSE, SIO_UDP_CONNRESET | SOCKET recv_sock_ | Non-blocking receiver active |
| Discovery Spawning | std::thread, bind | SO_REUSEADDR, SO_BROADCAST | SOCKET (Port 50000), loop thread | P2P advertising active |

### Peer Discovery Protocol and Atomics-Based UI Synchronization

The PeerDiscovery module implements zero-coordinator peer detection on UDP port 50000:

**Discovery Broadcasts:** At 1000ms intervals, a structured payload broadcasts to 255.255.255.255 (local subnet) and 127.255.255.255 (loopback):
```
VOICE_PEER:<client_id>:<room_name>:<audio_port>
```

**Peer Registry Management:** Uses synchronous select() with 250ms timeout to poll inbound advertisements. Peers matching the local room are added to the registry. Local IPs are marked is_local=true and remapped to 127.0.0.1 for multi-instance support. Peers inactive for 3500ms are pruned.

**Lock-Free UI Synchronization:** The discovery thread maintains std::shared_ptr<const std::vector<PeerSnapshot>> and uses std::atomic_store for double-buffering peer updates. The UI thread (1500ms auto-refresh timer) reads via std::atomic_load in O(1) time without locks.

| Parameter | Port | Protocol | Cycle / Timeout | Payload / Behavior |
|-----------|------|----------|-----------------|-------------------|
| Discovery Broadcast | 50000 | UDP Broadcast | 1000ms | VOICE_PEER:<id>:<room>:<port> |
| Socket Polling | 50000 | Synchronous Select | 250ms Timeout | Inbound 512-byte buffer |
| Peer Pruning | N/A | Local Timer | 3500ms Inactivity | Removes stale records |
| UI Polling (Mic/Peaks) | N/A | Qt QTimer | 200ms | Live level display |
| UI Auto-Refresh | N/A | Qt QTimer | 1500ms | Rebuilds participant list |

---


## Detailed Installation

### System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| OS | Windows 10 x64 | Windows 11 x64 |
| MSVC | Build Tools 2022 | Visual Studio 2022 (v143) |
| RAM | 4 GB | 8 GB |
| Network | 1 Mbps LAN | Gigabit LAN |


#### 1b. Microsoft Visual C++ Build Tools 2022
Required to compile Cython extensions and C++ code.

**Option A: Standalone Build Tools (minimal, ~5 GB)**
1. Download [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
2. Run installer, select:
   - ✅ "Desktop development with C++"
   - ✅ "MSVC v143 x64/x86 build tools"
   - ✅ "CMake tools for Windows"
3. Install to default location

**Option B: Visual Studio Community 2022 (full IDE, ~10 GB)**
1. Download [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/)
2. Run installer, select:
   - ✅ "Desktop development with C++"
   - ✅ "C++ CMake tools for Windows"
3. Install to default location

**Verify:** Open **Developer Command Prompt for VS 2022** and run:
```cmd
cl /version
```
You should see `Microsoft (R) C/C++ Optimizing Compiler Version 19.3x.xxxxx`.

#### 1c. Git for Windows
Download from [git-scm.com](https://git-scm.com/) and install with default options.

**Verify:**
```powershell
git --version
```

#### 1d. CMake 3.20+ (optional, for native C++ builds)
Download from [cmake.org](https://cmake.org/download/). Only needed if building the C++/Qt client.

### Step 2: Clone the Repository

```powershell
# Choose your working directory
cd C:\Projects  # or wherever you prefer

# Clone
git clone https://github.com/yuvaneshbn/Nuummite.git
cd Nuummite

# Verify structure
dir /b
```

You should see:
```
Nuummite/           (source code)
third_party/        (prebuilt DLLs: opus, libsodium, rnnoise)
tools/              (diagnostic utilities)
CMakeLists.txt
CMakePresets.json
README.md
```

### Step 5: Install Nuummite


**Expected output:**
```
Installing collected packages: ... nuummite
Successfully installed nuummite-0.1.0
```

---

## Compilation & Build

### Build System Overview

- **CMake:** Orchestrates C++ compilation
- **MSVC 2022:** Compiles C++17 code with full optimization


### C++/Qt Native Client Build

**Prerequisites:**
- CMake 3.20+
- Visual Studio 2022 Build Tools or Visual Studio 2022
- Qt 6.11.x for MSVC 2022 x64
- The bundled `third_party` DLLs and `.lib` files in this repo

**Build**

```powershell
cd C:\Users\YUVANESH\Downloads\Nuummite

# Recommended: open "x64 Native Tools Command Prompt for VS 2022"
# If you stay in PowerShell, bootstrap MSVC inside cmd.exe so the environment persists:
cmd /c ""C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cmake -S . -B .\build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64 && cmake --build .\build-msvc"

# If you are already inside the Developer Command Prompt, these two commands also work:
# cmake -S . -B .\build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
# cmake --build .\build-msvc
```

**Run**

```powershell
.\build-msvc\bin\voice_client.exe
```

The executable is staged into `build-msvc\bin\` along with the required Qt plugins and runtime DLLs.

---

## Running the Application


**First Launch Wizard:**
1. Enter **Client Name** (must be unique, e.g., "Alice")
2. Choose **Room** (defaults to "main")
3. (Optional) Enter **IP hint** for multi-interface machines
4. Click **Join**

**Firewall Prompt:** Click **Allow** on both private network prompts.

**Connection Status:**
- 🟢 **Green "Connected"** = At least one peer discovered
- 🔴 **Red "Disconnected"** = No other clients found (normal on first launch)

### Native C++/Qt Client

```powershell
.\build-msvc\bin\voice_client.exe
```

---

## Real-Time Audio Pipelines

### C++ Transmit (TX) Pipeline Workflow

**Phase 1: Hardware Input Capture and Stereo Downmixing (48 kHz, 20ms = 960 samples)**

The audio engine opens a WASAPI stream via PortAudio, or falls back to WaveIn if unavailable. Raw PCM samples enter the capture callback. Stereo input is downmixed to mono:

```cpp
// Stereo to Mono Downmixing
int32_t mixed = (static_cast<int32_t>(samples[2 * i]) + 
                 static_cast<int32_t>(samples[2 * i + 1])) / 2;
frame[i] = static_cast<int16_t>(std::clamp(mixed, -32768, 32767));
```

Mono frames are written to a cache-line-aligned SPSC ring buffer (16-frame capacity) and a semaphore signals the transmission thread.

**Phase 2: Multi-Stage Preprocessing**

The transmission thread pops frames and processes through three modules:
1. **Acoustic Echo Cancellation (AEC):** WebRTC APM high-pass filtering and echo cancellation using playback reference frames
2. **Deep Noise Suppression:** RNNoise processes two 10ms blocks (480 samples each) per 20ms frame
3. **Automatic Gain Control:** If enabled, APM normalizes output. If disabled, manual gain scaling applies:
   ```
   ScaledSample = RawSample × (10^(tx_gain_db/20) × mic_sensitivity/50)
   ```

**Phase 3: Voice Activity Detection (VAD) and Hangover Protection**

WebRTC's VAD runs; fallback to peak amplitude comparison against dynamic threshold:
```
VAD_Threshold = clamp(260 - (mic_sensitivity × 2), 60, 220)
```
If speech detected, hangover counter set to 18 frames (360ms). Prevents audio cutoff during pauses. When hangover reaches zero, transmission pauses.

**Phase 4: Opus Voice Encoding and Authenticated Encryption**

Active frames compressed with Opus (OPUS_APPLICATION_VOIP, 48 kbps, complexity 10, in-band FEC enabled). Plaintext header + Opus payload encrypted with SodiumWrapper:

```
Plaintext = "client_id|seq|timestamp:" + OpusData
Packet = [Nonce (24 bytes)] + [Authenticated Ciphertext (variable)]
```

**Phase 5: Network Serialization and QoS Mapping**

Packet sent to all active peer addresses via non-blocking UDP socket. DSCP EF (value 46) applied to IP header's ToS field (0xB8) for voice prioritization.

### C++ Receive (RX) and Mixing Pipeline Workflow

**Phase 1: Packet Reception and Security Verification**

listenLoop monitors recv_sock_ using WSAPoll (100ms timeout). Received packet's first 24 bytes parsed as nonce; remaining bytes decrypted:

```
Plaintext = crypto_secretbox_open_easy(Ciphertext, Nonce, RoomKey)
```
Decryption failure discards packet. Success extracts sender ID, sequence number, timestamp, and routes to peer's StreamState.

**Phase 2: Jitter Buffer and Packet Loss Concealment (PLC)**

Each peer has 64-slot JitterBuffer. Sequence validation detects missing packets. Target buffer size adapts dynamically:
- If packet loss over 50-packet window > 10%: increase target (up to 6 frames)
- If packet loss < 2%: decrease target (down to 2 frames)

Missing packets trigger Opus decoder PLC, synthesizing audio from prior frames.

**Phase 3: Synchronized Stream Mixing and Soft-Clipping**

Hardware playback thread uses StreamSnapshotGuard lock-free reader pattern:

```cpp
// Lock-Free Reader Registration
for (;;) {
    idx = engine->stream_snapshot_active_.load(std::memory_order_acquire);
    engine->stream_snapshot_readers_[idx].fetch_add(1, std::memory_order_acq_rel);
    if (idx == engine->stream_snapshot_active_.load(std::memory_order_acquire)) break;
    engine->stream_snapshot_readers_[idx].fetch_sub(1, std::memory_order_release);
}
```

Frames from active peers mixed into high-precision accumulator. If peak exceeds 28,000, scaling applied:
```
ScaleFactor = 28000.0 / Peak
```
opus_pcm_soft_clip then compresses peaks, converted back to int16 PCM.

**Phase 4: Reference Echo Loop, Volume Scaling, and Stereo Output**

Mixed frame sent to AecProcessor::process_render for WebRTC reference. Master and output volume applied:
```
ScaledPCM = MixedPCM × (master_volume × output_volume)
```
Mono samples written to playback FIFO. For stereo output, mono duplicated to both channels.

---

## Defensive Engineering and Codebase Optimizations

**Optimization 1: Dynamic CPU Core Protection during Double-Buffered Reads**

High-priority hardware thread uses StreamSnapshotGuard spin-wait with CPU pause instructions to prevent audio dropouts when UI updates participant list:

```cpp
while (stream_snapshot_readers_[write].load(std::memory_order_acquire) != 0) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    _mm_pause(); // Low-power thread spin-wait state
#endif
    std::this_thread::yield();
}
```

**Optimization 2: Dual-Driver Input Fallback Path**

If WASAPI/PortAudio fails, system automatically switches to Windows Multimedia WaveIn (mono, 48 kHz, 4 async buffers).

**Optimization 3: SIO_UDP_CONNRESET Socket Guard**

Disables WSAECONNRESET on UDP sockets, preventing ICMP port unreachable from interrupting receiver loop when peers leave:

```cpp
BOOL new_behavior = FALSE;
WSAIoctl(sock, SIO_UDP_CONNRESET, &new_behavior, sizeof(new_behavior), 
         nullptr, 0, &bytes_returned, nullptr, nullptr);
```

**Optimization 4: Peak-Aware Audio Saturation and Soft-Clipping**

Mixing engine measures accumulated frame peak. If > 28,000, scales down before soft-clipping to prevent digital distortion.

### Engine Parameters and Thresholds

| Domain | Parameter | Value | Purpose |
|--------|-----------|-------|---------|
| Audio Processing | Sampling Rate | 48000 Hz | Core frequency |
| Audio Processing | Frame Size | 960 Samples | 20ms processing block |
| Audio Processing | Frame Bytes | 1920 Bytes | Mono 16-bit PCM |
| Acoustic Preprocessor | RNNoise Block Size | 480 Samples | 10ms neural input |
| Network Protocol | Target UDP Port | 50002 | Voice packets |
| Network Protocol | IP QoS Marking | DSCP 46 (TOS 184) | High priority |
| Network Protocol | Jitter Buffer Size | 64 Slots | Out-of-order window |
| Network Protocol | Max Payload Bytes | 1500 Bytes | Encrypted packet MTU |
| Concurrency | Thread Cache Align | alignas(64) | False sharing prevention |
| Concurrency | SPSC Queue Capacity | 17 Slots | Capture frame queue |



### Troubleshooting Packaged Builds

**Issue: "DLL load failed" or missing libraries**

Solution: Copy required DLLs manually:
```powershell
# Copy DLLs to _internal folder
copy third_party\opus\opus.dll dist\Nuummite\_internal\
copy third_party\libsodium\libsodium.dll dist\Nuummite\_internal\
copy third_party\rnnoise\rnnoise.dll dist\Nuummite\_internal\
```


**Issue: Window never appears**

- Run from command prompt to see errors:
  ```powershell
  cd dist\Nuummite
  .\Nuummite.exe
  ```
- Check Event Viewer (Windows > Event Viewer > Windows Logs > Application) for crash details

**Issue: Missing VCRUNTIME*.dll on target PC**

- Install [Microsoft Visual C++ Redistributable (x64)](https://support.microsoft.com/en-us/help/2977003/the-latest-supported-visual-c-downloads) on the target machine

---

## Native C++/Qt Client

### Build Requirements

| Tool | Version | Purpose |
|------|---------|---------|
| CMake | 3.20+ | Build orchestration |
| Visual Studio | 2022 | C++ compiler (MSVC v143) |
| vcpkg | Latest | Package manager for Qt6, Opus |
| Qt6 | 6.5+ | GUI framework (via vcpkg) |

### Build Steps

**1. Configure (generate build files)**

```powershell
cd C:\Projects\Nuummite

# MSVC + Visual Studio solution
cmake --preset vs-release

# OR: Ninja + single-config (faster iteration)
cmake --preset ninja-release
```

**2. Build**

```powershell
# MSVC
cmake --build --preset vs-release

# OR: Ninja
cmake --build --preset ninja-release
```

**3. Run**

```powershell
# MSVC output
.\build\bin\Release\voice_client.exe

# Ninja output
.\build-ninja\voice_client.exe
```

**Two-instance audio flow test**

```powershell
.\build-msvc\bin\audio_flow_test.exe --pair --seconds 4 --room test-room --sender-id alpha --receiver-id beta
```

This starts a synthetic sender and a receiver over loopback, then prints `AUDIO FLOW VERIFIED` when packets travel successfully.

### Preset Reference

Available in `CMakePresets.json`:

| Preset | Generator | Build Type | Output Dir |
|--------|-----------|-----------|-----------|
| `vs-release` | Visual Studio 17 2022 | Release | `.\build\bin\Release\` |
| `vs-debug` | Visual Studio 17 2022 | Debug | `.\build\bin\Debug\` |
| `ninja-release` | Ninja | Release | `.\build-ninja\` |
| `ninja-debug` | Ninja | Debug | `.\build-ninja-debug\` |

---

## Troubleshooting

### Installation Issues

**"error: Microsoft Visual C++ 14.0 or greater is required"**
- MSVC Build Tools not installed
- Solution: Install from [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/)

**"error: cmake: command not found"**
- CMake not installed or not in PATH
- Solution: Install CMake from [cmake.org](https://cmake.org/download/), check "Add CMake to PATH"

### Runtime Issues

**"No audio devices detected"**
- PortAudio/WaveIn not finding audio hardware
- Solution:
  1. Check Windows Sound Settings (Settings > Sound > Volume)
  2. Ensure default mic/speaker are selected
  3. Try Settings > Privacy & Security > Microphone > Allow apps to access microphone

**"Can't hear other participants"**
- Check if `hear_targets_` is set correctly in UI
- Ensure participants are not muted (gray 🔇 icon)
- Verify Windows volume is not muted
- Firewall may be blocking UDP 50002:
  ```powershell
  # Check firewall rule
  Get-NetFirewallRule -DisplayName "python" | Select-Object DisplayName, Enabled
  ```

**"Microphone input detected but no transmission"**
- Audio captured but not sent (likely voice detection threshold too high)
- Solution:
  1. Settings > Gain: Increase to +6 dB
  2. Settings > Mic Sensitivity: Increase to 80+
  3. Ensure "Noise Suppression" is not 100%

**"Connection Timeout / Peers not discovered"**
- Firewall blocking UDP 50000
- Solution:
  1. Windows Firewall > Advanced Settings > Inbound Rules
  2. New Rule > Port > UDP 50000 > Allow > Private networks only
  3. Repeat for UDP 50002

### Build Issues

**"CMake Error: Opus import library not found"**
- `third_party/opus/opus.lib` missing
- Solution: Ensure you cloned with submodules or downloaded prebuilt libs

**"Cannot open include file: 'cstdint' / 'windows.h' / 'winsock2.h'"**
- The MSVC environment was not initialized before configuring or building.
- Solution: open the `x64 Native Tools Command Prompt for VS 2022`, or run the `cmd /c` build command shown above.

**"LINK: fatal error LNK1104: cannot open file 'opus.lib'"**
- Same as above; also check bitness (must be x64)


---

## Runtime Configuration

### Audio Settings

Accessible from Settings dialog (⚙️ icon):

| Setting | Range | Default | Effect |
|---------|-------|---------|--------|
| Master Volume | 0–250% | 100% | Overall playback level |
| Output Volume | 0–250% | 100% | Per-speaker volume |
| Microphone Gain | -20 to +20 dB | 0 dB | Input amplification |
| Mic Sensitivity | 0–100 | 45 | Voice detection threshold |
| Noise Suppression | 0–100 | 70 | RNNoise denoising strength |
| Echo Cancellation | On/Off | On | WebRTC AEC |
| Auto Gain | On/Off | Off | Automatic level control (AGC) |

### Codec Parameters

Hard-coded in `audio_engine.cpp`:

- **Sample Rate:** 48 kHz
- **Frame Size:** 960 samples (20 ms)
- **Channels:** Mono
- **Bitrate:** 48 kbps (variable, VOIP application)
- **Complexity:** 10 (maximum quality)
- **In-Band FEC:** Enabled (forward error correction)

### Room Encryption

- **Key Derivation:** Argon2id (crypto_pwhash) with salt "NuummiteVoiceKDF", fallback to BLAKE2b
- **Encryption:** libsodium secretbox (XSalsa20-Poly1305)
- **Isolation:** Each room is cryptographically isolated; peers from different rooms cannot decrypt each other's traffic

---

## Network & Security

### Ports & Protocols

| Port | Protocol | Direction | Purpose | Cycle |
|------|----------|-----------|---------|-------|
| 50000 | UDP | Broadcast | Peer discovery | 1000ms broadcast, 250ms poll |
| 50002 | UDP | Unicast | Audio stream | Real-time, per-frame |

### Firewall Configuration (Windows)

```powershell
# Allow inbound on private networks
New-NetFirewallRule -DisplayName "Nuummite Discovery" `
  -Direction Inbound -Action Allow -Protocol UDP -LocalPort 50000 `
  -Profile Private

New-NetFirewallRule -DisplayName "Nuummite Audio" `
  -Direction Inbound -Action Allow -Protocol UDP -LocalPort 50002 `
  -Profile Private
```

### Security Properties

- ✅ **Audio packets encrypted:** libsodium secretbox (XSalsa20-Poly1305)
- ✅ **Key derivation:** Argon2id with unique static salt
- ✅ **Packet authentication:** Poly1305 MAC (authenticated encryption)
- ⚠️ **No peer authentication:** Anyone with room name can join
- ⚠️ **Recommend private network only:** LAN behind NAT/firewall
- ⚠️ **No forward secrecy:** Compromised passphrase reveals historical sessions

### Recommended Deployment

- Use on **trusted LAN only** (office, home, friend's network)
- Pair with **network segmentation** if on corporate network
- For untrusted networks, use **VPN + Nuummite**
- Rotate passphrases periodically for long-running deployments

---

## Repository Structure

```
Nuummite/
├── Nuummite/
│   ├── audio/              # C++ audio engine (PortAudio, WebRTC APM, RNNoise)
│   │   ├── audio_engine.cpp/h        (Core TX/RX pipelines, 48 kHz, 20ms frames)
│   │   ├── aec_processor.cpp/h       (Echo cancellation, WebRTC APM wrapper)
│   │   ├── rnnoise_processor.cpp/h   (Deep noise suppression, 10ms blocks)
│   │   ├── jitter_buffer.cpp/h       (Adaptive jitter handling, 64-slot buffer)
│   │   └── webrtc_apm.cpp/h          (WebRTC audio processing module)
│   ├── common/             # Shared utilities
│   │   ├── opus_codec.cpp/h          (Opus VOIP codec, 48 kbps)
│   │   ├── audio_packet.cpp/h        (Packet format & parsing)
│   │   ├── libsodium_wrapper.cpp/h   (Encryption, Argon2id KDF)
│   │   ├── socket_utils.cpp/h        (UDP socket configuration)
│   │   └── winsock_init.cpp/h        (Winsock & COM initialization)
│   ├── p2p/                # Peer discovery & transport
│   │   ├── peer_discovery.cpp/h      (UDP broadcast, atomic double-buffering)
│   │   └── rtp_transport.cpp/h       (RTP packet delivery, DSCP QoS)
│   ├── ui/                 # Qt Designer files
│   │   └── *.ui            (MainWindow, SettingsDialog, VolumePanel)
│   └── CMakeLists.txt      (Qt client build config)
├── third_party/
│   ├── opus/               (Prebuilt opus.dll, opus.lib, headers)
│   ├── libsodium/          (Prebuilt libsodium.dll, libsodium.lib, headers)
│   ├── rnnoise/            (Prebuilt rnnoise.dll, sources)
│   └── webrtc_audio_processing/ (WebRTC APM static library)
├── tools/
│   └── audio_flow_test.cpp (Diagnostic utility for pipeline verification)
├── CMakeLists.txt          (Root CMake config)
├── CMakePresets.json       (Build presets: vs-release, ninja-release, etc.)
└── README.md               (This file)
```

---

## License

MIT License. See `LICENSE` file.

**Third-party components:**
- **Qt6:** LGPL 3.0 (via vcpkg)
- **Opus:** BSD 3-Clause
- **PortAudio:** MIT
- **libsodium:** ISC
- **RNNoise:** BSD 3-Clause
- **WebRTC APM:** BSD 3-Clause

---

## Getting Help

**Issues or Questions?**

1. Check [Troubleshooting](#troubleshooting) section above
2. Review firewall settings (port 50000, 50002 must be open on private networks)
3. Check Windows Event Viewer for crash details
4. Run diagnostic:
   ```powershell
    tools/audio_flow_test.py
   ```
5. Verify peer discovery: Look for "VOICE_PEER:*" broadcasts on port 50000 with Wireshark or tcpdump

---

**Happy voice chatting! 🎤🎧**
