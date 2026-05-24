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
- Audio packets are encrypted & authenticated with libsodium secretbox (room name is the shared key by default).

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
  libportaudio/     # PortAudio runtime DLL (libportaudio.dll)
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
  - `third_party/libportaudio/libportaudio.dll` (included)
  - `third_party/libsodium/libsodium.dll` (included)
  - `third_party/rnnoise/rnnoise.dll` (included, 64-bit)

Python dependencies (installed automatically via `pip install -e .`):
- Cython 3.2.x, PySide6 6.6.x, Pillow 12.x, PyInstaller (if you package), plus transitive deps.

---

## Quick start (run from source)
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
python setup.py build_ext --inplace --force
```
If you only change Python UI code, you can just run `python -m python.main` without rebuilding.

> Tip: On a machine that already has Cython and setuptools in the venv, you can skip the explicit install lines and just run `python -m pip install -e . --no-deps`.

---

## Package as an `.exe` (PyInstaller)
You have two supported packaging paths:

### Option A (recommended): build via `Nuummite.spec`
This bundles the required third-party DLLs and the `.ui` files deterministically.

```powershell
.\.venv\Scripts\activate
pip install pyinstaller
pyinstaller --clean -y Nuummite.spec
```

Output: `dist/Nuummite/Nuummite.exe` (one-folder build) and `dist/Nuummite/_internal/` (all runtime files).

To distribute: zip the whole `dist/Nuummite/` folder (the exe needs `_internal`).

### Option B: build directly from `python/main.py`
This is useful for quick local builds, but you may need to copy DLLs manually if your environment differs.

```powershell
.\.venv\Scripts\activate
pip install pyinstaller
pyinstaller --clean --onedir --windowed --icon Nuummite/technical-support.ico --name "Nuummite" python/main.py
# One-file (more AV false-positives possible):
# pyinstaller --clean --onefile --windowed --noupx --icon Nuummite/technical-support.ico --name "Nuummite" python/main.py
```
Output:
- one-folder: `dist/Nuummite/Nuummite.exe` + `dist/Nuummite/_internal/`
- one-file: `dist/Nuummite.exe`

If the packaged exe fails to start due to missing runtime DLLs (notably libsodium, which is loaded dynamically), copy the required DLLs into the onedir `_internal` folder:
```powershell
python scripts/copy_required_dlls.py --target dist/Nuummite/_internal
```

### Troubleshooting packaged builds
- If the window never appears, try running from a console and check the exit code.
- Confirm you ship the whole folder for one-folder builds: `Nuummite.exe` must sit beside `_internal/`.
- If you see missing `VCRUNTIME*.dll` on a target PC, install the **Microsoft Visual C++ Redistributable** (x64).
- If audio devices do not enumerate, ensure your PortAudio DLL is present and 64-bit.

---

## Native C++/Qt client build
Requirements (Windows x64):
- CMake >= 3.20
- Ninja
- Visual Studio / Build Tools (MSVC x64)
- Qt 6 (MSVC kit), e.g. `C:\Qt\6.11.1\msvc2022_64`

Configure + build (recommended: MSVC + Ninja):
```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\msvc2022_64 && cmake --build build-msvc'

# run
.\build-msvc\bin\voice_client.exe
```

Rebuild after code changes (no reconfigure needed):
```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cmake --build build-msvc'
```

Notes:
- The `^` line-continuation character is for `cmd.exe` only. If you paste a `^` command into PowerShell as a single line, it will fail; use the one-liners above.
- If you delete `build-msvc/`, rerun the full configure command.
- `voice_client.exe` is made runnable by CMake post-build steps: it copies the required third-party DLLs next to the exe and runs `windeployqt` to deploy Qt runtime + plugins.
- `windeployqt` is invoked with `--compiler-runtime`, which drops `vc_redist.x64.exe` next to the exe. On a fresh machine you typically must run that installer once (or install “Microsoft Visual C++ Redistributable 2015–2022 (x64)”) to satisfy `MSVCP140.dll` / `VCRUNTIME140*.dll`.
- If your PortAudio DLL lives elsewhere, override it at configure time with `-DPORTAUDIO_DLL=C:\path\to\libportaudio.dll`.

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
- Two PCs on the same LAN should both allow UDP on private networks for discovery/audio.

---

## Default ports
- UDP **50000** : peer discovery (broadcast)
- UDP **50002** : audio (per peer)
Allow both on private networks in Windows Firewall.

---

## License
MIT. See `LICENSE`. Third-party components follow their respective licenses (Qt, Opus, PortAudio, libsodium, rnnoise).
