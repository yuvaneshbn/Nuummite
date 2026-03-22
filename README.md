# Nuummite – LAN P2P Voice Chat (Windows)

Peer‑to‑peer voice over your local network with zero central server.  
Written in C++ for the audio/transport core and driven by a Python + PySide6 UI.  
Targets **Windows 10/11**, 64‑bit.

---

## Features
- **P2P discovery** via UDP broadcast on **50000** (`VOICE_PEER:<id>:<port>`).
- **Audio transport** UDP on **50002** (per peer may advertise a different port).
- **Opus @ 48 kHz, 20 ms mono**; tuned for VOIP + DTX .
- **Device selection & live controls**: master/output volume, mic gain, noise suppression, mute, broadcast, per‑participant talk/ignore.
- **Name popup with duplicate retry**; optional IP hint field.
- **Single EXE option** (PyInstaller) – no Qt SDK needed on target PCs.

---

## Repository layout (key bits)
```
Nuummite/
  audio/            # C++ audio engine, RTP transport, AEC stub
  common/           # Opus wrapper, packets, socket utils, winsock init
  p2p/              # Peer discovery + RTP transport helpers
  ui/               # Qt .ui files (used by PySide6)
  python/           # Python entry (main.py), Cython wrapper, __init__.py
  third_party/opus/ # prebuilt opus.dll + import lib + headers
third_party/
  # portaudio DLL expected at C:/msys64/ucrt64/bin/libportaudio.dll (default)
CMakeLists.txt      # C++/Qt build (voice_client)
setup.py            # Cython extension (python.audio_wrapper)
```

---

## Prerequisites (for the Python build)
- Windows 10/11 x64
- **Python 3.11+** (tested on 3.11.9)
- **Microsoft Visual C++ Build Tools 2022** (for Cython/C++)
- Git
- PortAudio & Opus DLLs (already provided: `third_party/opus/opus.dll`; PortAudio default path `C:/msys64/ucrt64/bin/libportaudio.dll`)

Python deps (installed automatically via `pip install -e .`):
- `Cython 3.2.4`
- `PySide6 6.6.3.1` (Qt 6.6 runtime bundled in wheel)
- `Pillow 12.1.1`
- `pyinstaller 6.19.0` (only needed if you build an exe)
Other transitive packages come with these wheels.

---

## Quick start: run from source (recommended)
```powershell
git clone https://github.com/yuvaneshbn/Nuummite.git
cd Nuummite
python -m venv .venv
.\.venv\Scripts\activate
pip install --upgrade pip
pip install -e .            # builds the Cython extension + installs PySide6

# run
python -m python.main
```
At first launch, allow Windows Firewall for UDP 50000/50002.

---

## Build a one-folder or one-file app (PyInstaller)
With the venv active in repo root:
```powershell
pip install pyinstaller
pyinstaller --clean --onedir --windowed --icon Nuummite/technical-support.ico --name "Nuummite-Voice" python/main.py
# One-file (may be flagged by AV more often):
# pyinstaller --clean --onefile --windowed --noupx --icon Nuummite/technical-support.ico --name "Nuummite-Voice" python/main.py
```
Result:
- `dist/Nuummite-Voice/` (onedir) **or** `dist/Nuummite-Voice.exe` (onefile).
Place `technical-support.ico` in the same folder for the correct icon.

---

## C++/Qt native build (optional)
If you prefer the original C++ Qt client:
```powershell
cmake --preset vs-release
cmake --build --preset vs-release
.\build\bin\Release\voice_client.exe
```
Requirements: CMake ≥ 3.20, VS 2022, vcpkg manifest (Qt6, opus). Set `QT6_ROOT` if you have a local Qt install. Ports are the same (50000/50002).

---

## Runtime tips
- Each instance chooses a unique **Client ID**; duplicates are rejected at the popup.
- Noise suppression defaults to 70%, DTX enabled; adjust in Settings if your mic is very quiet/noisy.
- Audio runs only while you have at least one target selected (broadcast button or per‑participant talk toggles).
- If you hear nothing: check device selection in Settings and make sure Windows privacy settings allow mic access.

---

## Default ports
- UDP **50000** : peer discovery (broadcast)
- UDP **50002** : audio (per peer)
Allow these in Windows Firewall for private networks.

---

## Known limitations / TODO
- Echo cancellation is stubbed (AEC disabled); use headphones to avoid feedback.
- No encryption yet (planned).
- Voice Activity Detection is basic (Opus DTX + gate); can be extended with WebRTC VAD if needed.

---

## License
MIT. See `LICENSE`. Third‑party components (Qt, Opus, PortAudio) follow their respective licenses.
