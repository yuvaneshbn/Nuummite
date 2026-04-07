from setuptools import setup, Extension
from Cython.Build import cythonize
import os
from pathlib import Path

# Third-party DLL + .lib folders (same style as your Opus)
third_party = Path("third_party")
opus_dir = third_party / "opus"
rnnoise_dir = third_party / "rnnoise"

ext = Extension(
    "python.audio_wrapper",   # this is what you import in __init__.py
    sources=[
        "Nuummite/audio/audio_engine.cpp",
        "Nuummite/audio/aec_processor.cpp",
        "Nuummite/audio/rnnoise_processor.cpp",
        "Nuummite/common/audio_packet.cpp",
        "Nuummite/common/libsodium_wrapper.cpp",
        "Nuummite/common/opus_codec.cpp",
        "Nuummite/common/socket_utils.cpp",
        "Nuummite/common/winsock_init.cpp",
        "Nuummite/p2p/rtp_transport.cpp",
        "Nuummite/p2p/peer_discovery.cpp",
        "python/audio_wrapper.pyx",
    ],
    include_dirs=[
        "Nuummite",
        "Nuummite/audio",
        "Nuummite/common",
        "Nuummite/p2p",
        str(opus_dir),              # opus.h
        str(rnnoise_dir),           # rnnoise.h
    ],
    library_dirs=[
        str(opus_dir),
        str(rnnoise_dir),           # ← rnnoise.lib goes here
    ],
    libraries=["opus", "rnnoise", "ws2_32", "winmm"],  # sockets + timeGetTime
    language="c++",
    define_macros=[("NOMINMAX", None)],
    extra_compile_args=["/std:c++17", "/O2"],
)

setup(
    name="nuummite-python",
    version="0.1.0",
    packages=["python"],
    package_dir={"python": "python"},
    ext_modules=cythonize([ext], language_level=3),
    zip_safe=False,
)
