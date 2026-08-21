# CMakeLists.txt for PortAudio 19.7.0 — static library, Windows only (WASAPI + WMME)
# Placed in third_party/libportaudio/ and included via add_subdirectory() from root.

cmake_minimum_required(VERSION 3.16)
project(portaudio C)

set(PA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(PA_COMMON_SOURCES
    ${PA_ROOT}/src/common/pa_allocation.c
    ${PA_ROOT}/src/common/pa_converters.c
    ${PA_ROOT}/src/common/pa_cpuload.c
    ${PA_ROOT}/src/common/pa_debugprint.c
    ${PA_ROOT}/src/common/pa_dither.c
    ${PA_ROOT}/src/common/pa_front.c
    ${PA_ROOT}/src/common/pa_process.c
    ${PA_ROOT}/src/common/pa_ringbuffer.c
    ${PA_ROOT}/src/common/pa_stream.c
    ${PA_ROOT}/src/common/pa_trace.c
)

set(PA_WIN_SOURCES
    ${PA_ROOT}/src/os/win/pa_win_coinitialize.c
    ${PA_ROOT}/src/os/win/pa_win_hostapis.c
    ${PA_ROOT}/src/os/win/pa_win_util.c
    ${PA_ROOT}/src/os/win/pa_win_waveformat.c
    ${PA_ROOT}/src/os/win/pa_win_wdmks_utils.c
    ${PA_ROOT}/src/os/win/pa_x86_plain_converters.c
)

set(PA_WASAPI_SOURCES
    ${PA_ROOT}/src/hostapi/wasapi/pa_win_wasapi.c
)

set(PA_WMME_SOURCES
    ${PA_ROOT}/src/hostapi/wmme/pa_win_wmme.c
)

add_library(portaudio_static STATIC
    ${PA_COMMON_SOURCES}
    ${PA_WIN_SOURCES}
    ${PA_WASAPI_SOURCES}
    ${PA_WMME_SOURCES}
)

target_include_directories(portaudio_static PUBLIC
    ${PA_ROOT}/include               # portaudio.h (public API)
    ${PA_ROOT}/src/common            # pa_util.h, pa_hostapi.h, etc.
    ${PA_ROOT}/src/os/win            # pa_win_coinitialize.h, pa_win_wdmks_utils.h
)

target_compile_definitions(portaudio_static PUBLIC
    PA_USE_WASAPI=1
    PA_USE_WMME=1
)

if(MSVC)
    target_compile_options(portaudio_static PRIVATE /W0)
else()
    target_compile_options(portaudio_static PRIVATE -w)
endif()

# Windows system libraries needed by portaudio
target_link_libraries(portaudio_static PUBLIC
    ole32
    uuid
    winmm
    ksuser
    mfplat
    mfuuid
    wmcodecdspuuid
)

# Alias so consumers can link portaudio::portaudio
add_library(portaudio ALIAS portaudio_static)
