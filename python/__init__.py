import os
import sys
from pathlib import Path

_DLL_DIRECTORY_COOKIES: list[object] = []

def _add_dll_dirs():
    here = Path(__file__).resolve().parent.parent
    # In PyInstaller builds, sys._MEIPASS points at the bundled application directory
    # (usually the onedir `_internal` folder). Prefer it when present.
    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        here = Path(meipass)

    candidates = [
        here,                                   # project root
        here / "third_party" / "opus",
        here / "third_party" / "libsodium",
        here / "third_party" / "rnnoise",
        here / "third_party" / "libportaudio",
        # WebRTC APM (repo layout uses webrtc_audio_processing/bin)
        here / "third_party" / "webrtc_audio_processing" / "bin",
    ]
    seen: set[str] = set()
    for path in candidates:
        if not path.exists():
            continue
        key = str(path.resolve())
        if key in seen:
            continue
        seen.add(key)

        if hasattr(os, "add_dll_directory"):
            try:
                # Keep returned cookie alive for the lifetime of the process.
                _DLL_DIRECTORY_COOKIES.append(os.add_dll_directory(str(path)))
            except OSError:
                os.environ["PATH"] = f"{path};{os.environ.get('PATH','')}"
        else:
            os.environ["PATH"] = f"{path};{os.environ.get('PATH','')}"
_add_dll_dirs()

from .audio_wrapper import PyAudioEngine, PyPeerDiscovery
__all__ = ["PyAudioEngine", "PyPeerDiscovery"]
