from setuptools import setup, Extension
from Cython.Build import cythonize
from pathlib import Path

third_party = Path("third_party")
opus_dir = third_party / "opus"
rnnoise_dir = third_party / "rnnoise"
webrtc_dir = third_party / "webrtc_audio_processing"

ext = Extension(
    "python.audio_wrapper",
    sources=[
        "Nuummite/audio/audio_engine.cpp",
        "Nuummite/audio/aec_processor.cpp",
        "Nuummite/audio/rnnoise_processor.cpp",
        "Nuummite/audio/webrtc_apm.cpp",
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
        str(opus_dir),
        str(rnnoise_dir),
        str(webrtc_dir / "include"),
        str(webrtc_dir / "include" / "webrtc-audio-processing-1"),
    ],
    library_dirs=[
        str(opus_dir),
        str(rnnoise_dir),
        str(webrtc_dir / "lib"),
    ],
    libraries=["opus", "rnnoise", "webrtc-audio-processing-1-msvc", "ws2_32", "winmm", "iphlpapi"],
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
