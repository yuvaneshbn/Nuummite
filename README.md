# Nuummite - LAN P2P Voice Chat (Windows)

Peer-to-peer voice over your local network with zero central server.  
C++ audio/transport core, Python + PySide6 UI. 64-bit **Windows 10/11** target.

---

## Features
- UDP peer discovery on **50000** (`VOICE_PEER:<id>:<port>`).
- Audio transport on **50002** with Opus @ 48 kHz, 20 ms mono, DTX tuned for VOIP.
- Live controls: device selection, master/output volume, mic gain, noise suppression, mute, broadcast, per-participant talk/ignore.
- Friendly name dialog with duplicate retry; optional IP hint.
- PyInstaller one-folder/one-file builds (no Qt SDK on target PCs).

---

## Repository layout
```
Nuummite/
  audio/            # C++ audio engine, RTP transport, AEC stub
  common/           # Opus wrapper, packets, socket utils, winsock init
  p2p/              # Peer discovery + RTP transport helpers
  ui/               # Qt .ui files (PySide6 consumes)
  python/           # Python entry (main.py), Cython wrapper, __init__.py
  third_party/opus/ # prebuilt opus.dll + import lib + headers
third_party/
  libsodium/        # prebuilt libsodium.dll + import lib + headers
  rnnoise/          # rnnoise sources + built DLL
  libportaudio/     # optional helper, PortAudio DLL typically in msys64
CMakeLists.txt      # C++/Qt build (voice_client)
setup.py            # Cython extension (python.audio_wrapper)
CMakePresets.json   # VS/Ninja presets
```

---

## Prerequisites (Python UI path)
- Windows 10/11 x64
- **Python 3.11+** (tested 3.11.9)
- **Microsoft Visual C++ Build Tools 2022 or newer** (MSVC 14.3+) for the Cython extension
- Git
- Audio DLLs:
  - `third_party/opus/opus.dll` (included)
  - PortAudio DLL: default `C:/msys64/ucrt64/bin/libportaudio.dll` (adjust PATH if elsewhere)
  - `third_party/libsodium/libsodium.dll` (included)
  - `third_party/rnnoise/rnnoise.dll` (included, 64-bit)

Python dependencies (installed automatically via `pip install -e .`):
- Cython 3.2.x, PySide6 6.6.x, Pillow 12.x, PyInstaller (if you package), plus transitive deps.

---

## Install & run from source (recommended)
```powershell
git clone https://github.com/yuvaneshbn/Nuummite.git
cd Nuummite
python -m venv .venv
.\.venv\Scripts\activate
pip install --upgrade pip
pip install -e .            # builds python/audio_wrapper + installs deps

# run
python -m python.main
```
First launch: allow Windows Firewall for UDP 50000/50002 on private networks.

---

## After changing C++/Cython code (rebuild then run)
When you edit any C++ under `Nuummite/audio`, `Nuummite/common`, `Nuummite/p2p`, or the Cython file `python/audio_wrapper.pyx`, rebuild the extension before running:
```powershell
.\.venv\Scripts\activate
# Fresh machine? install build prereqs first:
python -m pip install --upgrade pip
python -m pip install "cython==3.2.4" "setuptools>=68" wheel

# Rebuild extension
python -m pip install -e . --no-deps   # re-cythonizes and rebuilds python/audio_wrapper

# Run with the fresh extension
python -m python.main
```
If you only change Python UI code, you can just run `python -m python.main` without rebuilding.

> Tip: On a machine that already has Cython and setuptools in the venv, you can skip the explicit install lines and just run `python -m pip install -e . --no-deps`.

python setup.py build_ext --inplace --force
---

## Package with PyInstaller
With the same venv active:
```powershell
pip install pyinstaller
pyinstaller --clean --onedir --windowed --icon Nuummite/technical-support.ico --name "Nuummite-Voice" python/main.py
# One-file (more AV false-positives possible):
# pyinstaller --clean --onefile --windowed --noupx --icon Nuummite/technical-support.ico --name "Nuummite-Voice" python/main.py
```
Output: `dist/Nuummite-Voice/` (onedir) or `dist/Nuummite-Voice.exe` (onefile).  
Keep `technical-support.ico` beside the exe for correct icon.

---

## Native C++/Qt client build
Requirements: CMake >= 3.20, Visual Studio 2022 (MSVC x64), vcpkg installed and `VCPKG_ROOT` set. Qt6 and Opus come from the vcpkg manifest; CMakePresets use the vcpkg toolchain.

Configure + build (Visual Studio solution):
```powershell
cmake --preset vs-release
cmake --build --preset vs-release
.\build\bin\Release\voice_client.exe
```

Alternative (Ninja, single-config, exports compile_commands.json):
```powershell
cmake --preset ninja-release
cmake --build --preset ninja-release
.\build-ninja\voice_client.exe
```

If Qt is installed locally instead of via vcpkg, set `QT6_ROOT` accordingly before configuring.

---

## Rebuilding the rnnoise DLL (only if you change rnnoise sources)
Symptom: `ImportError: DLL load failed while importing audio_wrapper: %1 is not a valid Win32 application` means bitness mismatch.

Build 64-bit rnnoise with MSVC:
```cmd
cd /d C:\path\to\Nuummite-main\third_party\rnnoise
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 && ^
cl /nologo /O2 /LD denoise.c celt_lpc.c pitch.c rnn.c rnn_data.c kiss_fft.c rnn_reader.c ^
   /Fe:rnnoise.dll /I. /MD /link /DEF:rnnoise.def
```
Verify:
```powershell
python - <<'PY'
import python.audio_wrapper as aw
print("audio_wrapper import ok:", aw)
PY
```

---

## Runtime notes
- Each instance chooses a unique **Client ID**; duplicates are rejected.
- Noise suppression defaults to 70%, DTX enabled; tweak in Settings for noisy/quiet mics.
- Audio flows only when at least one target is active (broadcast or per-participant talk).
- If you hear nothing: confirm device selection, Windows mic permission, and firewall rules.

---

## Default ports
- UDP **50000** : peer discovery (broadcast)
- UDP **50002** : audio (per peer)
Allow both on private networks in Windows Firewall.

---

## Known limitations / TODO
- Echo cancellation stubbed (AEC disabled) — use headphones.
- No encryption yet (planned).
- VAD is basic (Opus DTX + gate); WebRTC VAD is a potential upgrade.

---

## License
MIT. See `LICENSE`. Third-party components follow their respective licenses (Qt, Opus, PortAudio, libsodium, rnnoise).
