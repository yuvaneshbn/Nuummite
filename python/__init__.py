import os
import sys
from pathlib import Path


def _add_dll_dirs():
    """Ensure native DLLs (opus, portaudio) are discoverable on Windows."""
    here = Path(__file__).resolve().parent.parent  # repo root
    candidates = [
        here / "third_party" / "opus",
        Path(r"C:/msys64/ucrt64/bin"),
    ]
    for path in candidates:
        if path.exists():
            if hasattr(os, "add_dll_directory"):
                os.add_dll_directory(str(path))
            else:  # Python <3.8 fallback
                os.environ["PATH"] = f"{path};{os.environ.get('PATH','')}"


_add_dll_dirs()

from .audio_wrapper import PyAudioEngine, PyPeerDiscovery

__all__ = ["PyAudioEngine", "PyPeerDiscovery"]
