# -*- mode: python ; coding: utf-8 -*-
from __future__ import annotations

from pathlib import Path

block_cipher = None
# PyInstaller injects `SPECPATH` into the spec's global namespace.
ROOT = Path(SPECPATH).resolve()


def _bin(src: Path, dest: str = "."):
    return (str(src), dest)


# Keep runtime DLLs in the top of `_internal` so both explicit PATH prepends and
# implicit dependency lookup from extension modules succeed.
binaries = [
    _bin(ROOT / "third_party" / "opus" / "opus.dll"),
    _bin(ROOT / "third_party" / "rnnoise" / "rnnoise.dll"),
    _bin(ROOT / "third_party" / "libportaudio" / "libportaudio.dll"),

    # libsodium: the core DLL + its MinGW runtime dependencies (some environments use these).
    _bin(ROOT / "third_party" / "libsodium" / "libsodium.dll"),
    _bin(ROOT / "third_party" / "libsodium" / "bin" / "libsodium-26.dll"),
    _bin(ROOT / "third_party" / "libsodium" / "bin" / "libgcc_s_seh-1.dll"),
    _bin(ROOT / "third_party" / "libsodium" / "bin" / "libwinpthread-1.dll"),

    # WebRTC APM prebuilt binaries.
    _bin(ROOT / "third_party" / "webrtc_audio_processing" / "bin" / "webrtc-audio-processing-1-3.dll"),
    _bin(ROOT / "third_party" / "webrtc_audio_processing" / "bin" / "webrtc-audio-coding-1-3.dll"),
    _bin(ROOT / "third_party" / "webrtc_audio_processing" / "bin" / "libwebrtc-audio-processing-1-3.dll"),
    _bin(ROOT / "third_party" / "webrtc_audio_processing" / "bin" / "libwebrtc-audio-coding-1-3.dll"),
]

# Qt Designer .ui files loaded at runtime via `resource_path("Nuummite/ui/...")`
ui_dir = ROOT / "Nuummite" / "ui"
datas = [(str(p), "Nuummite/ui") for p in sorted(ui_dir.glob("*.ui"))]

a = Analysis(
    ["python\\main.py"],
    pathex=[str(ROOT)],
    binaries=binaries,
    datas=datas,
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
    optimize=0,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="Nuummite",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=[str(ROOT / "Nuummite" / "technical-support.ico")],
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name="Nuummite",
)
