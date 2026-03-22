from setuptools import Extension, setup
from Cython.Build import cythonize
import os


ROOT = os.path.abspath(os.path.dirname(__file__))


def p(*parts):
    return os.path.join(ROOT, *parts)


include_dirs = [
    p("Nuummite"),
    p("Nuummite", "common"),
    p("third_party", "opus"),
]

library_dirs = [
    p("third_party", "opus"),
]

sources = [
    p("python", "audio_wrapper.pyx"),
    p("Nuummite", "audio", "audio_engine.cpp"),
    p("Nuummite", "audio", "aec_processor.cpp"),
    p("Nuummite", "p2p", "peer_discovery.cpp"),
    p("Nuummite", "p2p", "rtp_transport.cpp"),
    p("Nuummite", "common", "audio_packet.cpp"),
    p("Nuummite", "common", "opus_codec.cpp"),
    p("Nuummite", "common", "socket_utils.cpp"),
    p("Nuummite", "common", "winsock_init.cpp"),
]

libraries = ["opus", "ws2_32", "winmm"]

extra_compile_args = ["/std:c++17"] if os.name == "nt" else ["-std=c++17"]

ext = Extension(
    name="python.audio_wrapper",  # module import path
    sources=sources,
    include_dirs=include_dirs,
    library_dirs=library_dirs,
    libraries=libraries,
    language="c++",
    define_macros=[("NOMINMAX", None)],
    extra_compile_args=extra_compile_args,
)

setup(
    name="nuummite-python",
    version="0.1.0",
    packages=["python"],
    package_dir={"python": "python"},
    ext_modules=cythonize([ext], language_level=3),
    zip_safe=False,
)
