import os
import sys
from pathlib import Path

def _add_dll_dirs():
    here = Path(__file__).resolve().parent.parent
    candidates = [
        here,                                   # project root
        here / "third_party" / "opus",
        here / "third_party" / "libsodium",
        here / "third_party" / "rnnoise",
        here / "third_party" / "libportaudio",
        here / "third_party" / "win_webrtc" / "bin",
    ]
    for path in candidates:
        if path.exists():
            if hasattr(os, "add_dll_directory"):
                os.add_dll_directory(str(path))
            else:
                os.environ["PATH"] = f"{path};{os.environ.get('PATH','')}"
_add_dll_dirs()

from .audio_wrapper import PyAudioEngine, PyPeerDiscovery
__all__ = ["PyAudioEngine", "PyPeerDiscovery"]