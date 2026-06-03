# Nuummite - LAN P2P Voice Chat (Windows)

**High-performance, peer-to-peer voice communication for local networks with zero central server.**

Nuummite enables real-time encrypted voice chat across your LAN using a decentralized P2P mesh topology. Audio is processed by a high-performance C++ engine with automatic device selection, noise suppression, echo cancellation, and adaptive bitrate encoding—wrapped in a responsive PySide6 UI.

**Key Features:**
- ✅ UDP peer-to-peer discovery on port 50000
- ✅ Opus codec @ 48 kHz, 20 ms frames, mono audio
- ✅ End-to-end encryption (libsodium secretbox)
- ✅ Real-time device selection, volume controls, per-participant muting
- ✅ Noise suppression, echo cancellation (WebRTC APM), RNNoise denoising
- ✅ Zero-config LAN discovery—just enter a name and room
- ✅ Packagable as standalone `.exe` with PyInstaller
- ✅ Native C++/Qt client available for advanced users

**Target OS:** Windows 10/11 (x64), Python 3.11+  
**Architecture:** C++ audio engine (Cython wrapper) + PySide6 UI  
**License:** MIT

---

## Table of Contents

1. [Quick Start (Python UI)](#quick-start-python-ui)
2. [Detailed Installation](#detailed-installation)
3. [Compilation & Build](#compilation--build)
4. [Running the Application](#running-the-application)
5. [Development Workflow](#development-workflow)
6. [Packaging as .exe](#packaging-as-exe)
7. [Native C++/Qt Client](#native-c-qt-client)
8. [Troubleshooting](#troubleshooting)
9. [Runtime Configuration](#runtime-configuration)
10. [Network & Security](#network--security)

---

## Quick Start (Python UI)

### For Impatient Users

If you have Python 3.11+, Git, and MSVC Build Tools installed, do this:

```powershell
# Clone and enter directory
git clone https://github.com/yuvaneshbn/Nuummite.git
cd Nuummite

# Create virtual environment and activate
python -m venv .venv
.\.venv\Scripts\activate

# Install Nuummite and dependencies
pip install --upgrade pip
pip install -e .

# Run!
python -m python.main
```

First launch will prompt for:
- **Client Name:** e.g., "Alice", "Bob" (must be unique on your LAN)
- **Room Name:** e.g., "office", "gaming" (default: "main")

**Firewall:** Allow private network access for `python.exe` on UDP 50000 & 50002.

---

## Detailed Installation

### System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| OS | Windows 10 x64 | Windows 11 x64 |
| Python | 3.11 | 3.12 |
| MSVC | Build Tools 2022 | Visual Studio 2022 (v143) |
| RAM | 4 GB | 8 GB |
| Network | 1 Mbps LAN | Gigabit LAN |

### Step 1: Install Prerequisites

#### 1a. Python 3.11+
Download from [python.org](https://www.python.org/downloads/):
- Choose Python 3.11.9 or 3.12.x
- ✅ **During installation:** Check **"Add Python to PATH"**
- ✅ Verify: Open PowerShell and run:
  ```powershell
  python --version
  ```

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
setup.py
README.md
```

### Step 3: Create Python Virtual Environment

```powershell
# Create venv in project directory (isolated Python)
python -m venv .venv

# Activate (Windows PowerShell)
.\.venv\Scripts\Activate.ps1

# If you get a "running scripts is disabled" error:
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
# Then run Activate.ps1 again

# You should see (.venv) at start of your PowerShell prompt
```

**For Command Prompt (cmd.exe):**
```cmd
.venv\Scripts\activate.bat
```

### Step 4: Upgrade pip and Install Build Tools

```powershell
# Inside venv (.venv) prompt
pip install --upgrade pip setuptools wheel

# Install Cython (required for C++ extension)
pip install "cython==3.2.4"
```

### Step 5: Install Nuummite

```powershell
# Install in development mode (links to repo, editable)
pip install -e .
```

This will:
1. Run CMake to compile C++ audio engine
2. Cythonize `python/audio_wrapper.pyx` to `audio_wrapper.pyd`
3. Install Python dependencies:
   - `PySide6` (Qt6 bindings)
   - `Pillow` (image processing)
   - `psutil` (system monitoring)
   - Others as needed

**Expected output:**
```
Installing collected packages: ... nuummite
Successfully installed nuummite-0.1.0
```

---

## Compilation & Build

### Build System Overview

- **CMake:** Orchestrates C++ compilation
- **MSVC 2022:** Compiles C++17 code
- **Cython:** Converts `.pyx` to `.pyd` (Python extension)
- **setuptools:** Handles Python packaging

### Python UI Build (Automatic)

When you run `pip install -e .`, the build happens automatically:

```
1. setup.py calls CMake to build C++ static library
2. Cython compiles python/audio_wrapper.pyx → audio_wrapper.pyd
3. setuptools links .pyd with C++ library → audio_wrapper.cp311-win_amd64.pyd
4. Python dependencies installed from PyPI
```

**To rebuild after changing C++ code:**

```powershell
# Option 1: Full rebuild
pip install -e . --force-reinstall --no-cache-dir

# Option 2: Quick rebuild (faster)
pip install -e . --no-deps

# Option 3: Manual Cython rebuild
python setup.py build_ext --inplace --force
```

### C++/Qt Native Client Build

**Prerequisites:**
- CMake 3.20+
- Visual Studio 2022
- vcpkg (optional; CMake can fetch deps)
- Qt6 (via vcpkg or system)

**Step 1: Set up vcpkg (recommended)**

```powershell
# Clone vcpkg (first time only)
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Set environment variable (PowerShell)
$env:VCPKG_ROOT = "C:\vcpkg"
# For permanent: Add to System Environment Variables

# Verify
echo $env:VCPKG_ROOT
```

**Step 2: Configure CMake**

```powershell
cd C:\Projects\Nuummite

# Option A: Visual Studio 2022 (generates .sln file)
cmake --preset vs-release

# Option B: Ninja (single-config, fast)
cmake --preset ninja-release
```

CMake will fetch and build:
- Qt6 Core, Gui, Widgets
- Opus codec
- Other dependencies

**Step 3: Build**

```powershell
# Option A: Visual Studio
cmake --build --preset vs-release

# Option B: Ninja
cmake --build --preset ninja-release
```

**Output:** `.\build\bin\Release\voice_client.exe` or `.\build-ninja\voice_client.exe`

**Step 4: Run native client**

```powershell
.\build\bin\Release\voice_client.exe
```

---

## Running the Application

### Python UI

```powershell
# Make sure venv is activated
.\.venv\Scripts\Activate.ps1

# Run
python -m python.main
```

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
.\build\bin\Release\voice_client.exe
```

Same launch wizard as Python UI.

---

## Development Workflow

### Edit Python UI Only

If you only modify `python/main.py`, `python/*.py`, etc.:

```powershell
# No rebuild needed, just run
python -m python.main
```

Changes appear on next launch.

### Edit C++ Audio Engine

If you modify any files in `Nuummite/audio/`, `Nuummite/common/`, `Nuummite/p2p/`:

```powershell
# Rebuild the Cython extension
pip install -e . --no-deps

# Then run
python -m python.main
```

This rebuilds the `.pyd` in-place.

### Edit `.ui` (Qt Designer files)

If you modify `Nuummite/ui/*.ui`:

```powershell
# Force rebuild (pyside6-uic will regenerate Python bindings)
pip install -e . --force-reinstall --no-cache-dir

# Run
python -m python.main
```

---

## Packaging as .exe

Convert your development installation into a standalone executable that runs on any Windows 10/11 x64 machine without Python installed.

### Option 1: Using `Nuummite.spec` (Recommended)

This spec file bundles all DLLs and assets automatically:

```powershell
# Install PyInstaller
pip install pyinstaller

# Create exe
pyinstaller --clean -y Nuummite.spec
```

**Output:**
- `dist/Nuummite/Nuummite.exe` (one-folder executable)
- `dist/Nuummite/_internal/` (all runtime files: DLLs, Qt plugins, etc.)

**To distribute:**
1. Zip the entire `dist/Nuummite/` folder
2. Share the zip file
3. Users extract and run `Nuummite.exe`

### Option 2: Direct PyInstaller (Quick local test)

```powershell
pip install pyinstaller

# One-folder (recommended)
pyinstaller --clean --onedir --windowed --icon Nuummite/technical-support.ico --name "Nuummite" python/main.py

# One-file (slower startup, slightly more AV false-positives)
# pyinstaller --clean --onefile --windowed --noupx --icon Nuummite/technical-support.ico --name "Nuummite" python/main.py
```

**Output:**
- `dist/Nuummite/Nuummite.exe` + `dist/Nuummite/_internal/`

### Troubleshooting Packaged Builds

**Issue: "DLL load failed" or missing libraries**

Solution: Copy required DLLs manually:
```powershell
# Copy DLLs to _internal folder
copy third_party\opus\opus.dll dist\Nuummite\_internal\
copy third_party\libsodium\libsodium.dll dist\Nuummite\_internal\
copy third_party\rnnoise\rnnoise.dll dist\Nuummite\_internal\
```

Or use the helper script (if available):
```powershell
python scripts/copy_required_dlls.py --target dist/Nuummite/_internal
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

**"pip: command not found"**
- Python is not in PATH
- Solution: Reinstall Python, check "Add Python to PATH" during setup

**"error: Microsoft Visual C++ 14.0 or greater is required"**
- MSVC Build Tools not installed
- Solution: Install from [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/)

**"error: cmake: command not found"**
- CMake not installed or not in PATH
- Solution: Install CMake from [cmake.org](https://cmake.org/download/), check "Add CMake to PATH"

**"venv/Scripts/Activate.ps1 cannot be loaded because running scripts is disabled"**
- PowerShell execution policy is restricted
- Solution:
  ```powershell
  Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
  ```

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

**"LINK: fatal error LNK1104: cannot open file 'opus.lib'"**
- Same as above; also check bitness (must be x64)

**"Cython: module initialization failed"**
- C++ extension failed to compile
- Solution:
  ```powershell
  pip install -e . --force-reinstall --no-cache-dir --verbose
  ```
  Look for actual error in output

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
- **Bitrate:** 48 kbps (variable)
- **Application:** VOIP (low-latency tuned)

### Room Encryption

- Shared secret: **Room name** (user-supplied)
- Encryption: libsodium secretbox (XSalsa20-Poly1305)
- Each room is isolated; peers from different rooms can't hear each other

---

## Network & Security

### Ports & Protocols

| Port | Protocol | Direction | Purpose |
|------|----------|-----------|---------|
| 50000 | UDP | Broadcast | Peer discovery |
| 50002 | UDP | Unicast | Audio stream |

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

### Security Notes

- ✅ Audio packets are **encrypted** (libsodium secretbox)
- ✅ Room name acts as **shared secret**
- ⚠️ **Does not authenticate peers**—anyone who knows the room name can join
- ⚠️ **Recommend private network only** (LAN behind NAT/firewall)
- ⚠️ **No message authentication**—cannot verify sender identity

### Recommended Deployment

- Use on **trusted LAN only** (office, home, friend's network)
- Pair with **network segmentation** if on corporate network
- For untrusted networks, use **VPN + Nuummite**

---

## Repository Structure

```
Nuummite/
├── Nuummite/
│   ├── audio/              # C++ audio engine (PortAudio, WebRTC APM, RNNoise)
│   │   ├── audio_engine.cpp/h
│   │   ├── aec_processor.cpp/h         (Echo cancellation)
│   │   ├── rnnoise_processor.cpp/h     (Noise suppression)
│   │   ├── jitter_buffer.cpp/h         (RTP jitter handling)
│   │   └── webrtc_apm.cpp/h            (WebRTC audio processing)
│   ├── common/             # Shared utilities
│   │   ├── opus_codec.cpp/h            (Opus wrapper)
│   │   ├── audio_packet.cpp/h          (Protocol)
│   │   ├── libsodium_wrapper.cpp/h     (Encryption)
│   │   ├── socket_utils.cpp/h
│   │   └── winsock_init.cpp/h
│   ├── p2p/                # Peer discovery & RTP
│   │   ├── peer_discovery.cpp/h
│   │   └── rtp_transport.cpp/h
│   ├── ui/                 # Qt Designer files
│   │   └── *.ui
│   └── CMakeLists.txt      (Qt client build config)
├── third_party/
│   ├── opus/               (Prebuilt opus.dll, headers)
│   ├── libsodium/          (Prebuilt libsodium.dll, headers)
│   ├── rnnoise/            (Prebuilt rnnoise.dll, sources)
│   └── webrtc_audio_processing/
├── tools/
│   └── audio_flow_test.cpp (Diagnostic utility)
├── CMakeLists.txt          (Root CMake config)
├── CMakePresets.json       (Build presets)
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
2. Review firewall settings (port 50000, 50002 must be open)
3. Check Windows Event Viewer for crash details
4. Run diagnostic:
   ```powershell
   python tools/audio_flow_test.py
   ```
---

**Happy voice chatting! 🎤🎧**
