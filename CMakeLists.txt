cmake_minimum_required(VERSION 3.20)
project(Nuummite LANGUAGES C CXX)

if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(VOICE_BUILD_AUDIO_NATIVE    "Build native audio test tools"        OFF)
option(VOICE_BUILD_AUDIO_FLOW_TEST "Build audio flow diagnostic tool"     ON)

# ── Third-party paths ─────────────────────────────────────────────────────────

set(TP ${CMAKE_SOURCE_DIR}/third_party)

# Opus — built from source (static)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING        OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS       OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE   OFF CACHE BOOL "" FORCE)
add_subdirectory(${TP}/opus/src opus_build)
# The opus CMake defines an 'Opus::opus' target; alias for convenient use
if(NOT TARGET opus)
    add_library(opus ALIAS Opus::opus)
endif()
set(OPUS_INCLUDE_DIR ${TP}/opus/src/include)

# PortAudio — built from source (static, WASAPI + WMME)
add_subdirectory(${TP}/libportaudio portaudio_build)
set(PORTAUDIO_INCLUDE_DIR ${TP}/libportaudio/src/include)

# RNNoise — built from source (static C files)
add_library(rnnoise_static STATIC
    ${TP}/rnnoise/denoise.c
    ${TP}/rnnoise/celt_lpc.c
    ${TP}/rnnoise/pitch.c
    ${TP}/rnnoise/rnn.c
    ${TP}/rnnoise/rnn_data.c
    ${TP}/rnnoise/kiss_fft.c
    ${TP}/rnnoise/rnn_reader.c
)
target_include_directories(rnnoise_static PUBLIC ${TP}/rnnoise)
if(MSVC)
    target_compile_options(rnnoise_static PRIVATE /W0)
else()
    target_compile_options(rnnoise_static PRIVATE -w)
endif()

# WebRTC Audio Processing — pre-built static .lib (MSVC)
set(WEBRTC_DIR         ${TP}/webrtc_audio_processing)
set(WEBRTC_INCLUDE_DIR ${WEBRTC_DIR}/include)
set(WEBRTC_AUDIOPROC_LIB  ${WEBRTC_DIR}/lib/webrtc-audio-processing-1-msvc.lib)
set(WEBRTC_AUDIOCODING_LIB ${WEBRTC_DIR}/lib/webrtc-audio-coding-1.lib)

add_library(webrtc_audio_processing STATIC IMPORTED GLOBAL)
set_target_properties(webrtc_audio_processing PROPERTIES
    IMPORTED_LOCATION ${WEBRTC_AUDIOPROC_LIB}
)
add_library(webrtc_audio_coding STATIC IMPORTED GLOBAL)
set_target_properties(webrtc_audio_coding PROPERTIES
    IMPORTED_LOCATION ${WEBRTC_AUDIOCODING_LIB}
)

# libsodium — pre-built MSVC static .lib
# Define SODIUM_STATIC so sodium.h exposes symbols as plain (non-dllimport) linkage.
set(SODIUM_INCLUDE_DIR ${TP}/libsodium/include)
set(SODIUM_LIB         ${TP}/libsodium/libsodium.lib)

add_library(libsodium_static STATIC IMPORTED GLOBAL)
set_target_properties(libsodium_static PROPERTIES
    IMPORTED_LOCATION ${SODIUM_LIB}
)

# ── Shared internal library ───────────────────────────────────────────────────

add_library(voice_shared STATIC
    Nuummite/common/audio_packet.cpp
    Nuummite/common/libsodium_wrapper.cpp
    Nuummite/common/opus_codec.cpp
    Nuummite/common/socket_utils.cpp
    Nuummite/common/winsock_init.cpp
)
target_include_directories(voice_shared PUBLIC
    ${CMAKE_SOURCE_DIR}/Nuummite/common
    ${OPUS_INCLUDE_DIR}
    ${SODIUM_INCLUDE_DIR}
)
target_link_libraries(voice_shared PUBLIC Opus::opus libsodium_static ws2_32)
target_compile_definitions(voice_shared PUBLIC SODIUM_STATIC)

# ── Optional tools ────────────────────────────────────────────────────────────

if (VOICE_BUILD_AUDIO_NATIVE)
    add_subdirectory(audio_native)
endif()

if (VOICE_BUILD_AUDIO_FLOW_TEST)
    add_executable(audio_flow_test
        tools/audio_flow_test.cpp
        Nuummite/audio/audio_engine.cpp
        Nuummite/audio/aec_processor.cpp
        Nuummite/audio/jitter_buffer.cpp
        Nuummite/audio/rnnoise_processor.cpp
        Nuummite/audio/webrtc_apm.cpp
        Nuummite/p2p/peer_discovery.cpp
        Nuummite/p2p/rtp_transport.cpp
    )

    target_include_directories(audio_flow_test PRIVATE
        ${CMAKE_SOURCE_DIR}/Nuummite
        ${CMAKE_SOURCE_DIR}/Nuummite/common
        ${CMAKE_SOURCE_DIR}/Nuummite/audio
        ${CMAKE_SOURCE_DIR}/Nuummite/p2p
        ${OPUS_INCLUDE_DIR}
        ${PORTAUDIO_INCLUDE_DIR}
        ${TP}/rnnoise
        ${WEBRTC_INCLUDE_DIR}
        ${WEBRTC_INCLUDE_DIR}/webrtc-audio-processing-1
    )

    target_link_libraries(audio_flow_test PRIVATE
        voice_shared
        portaudio_static
        rnnoise_static
        webrtc_audio_processing
        webrtc_audio_coding
        ws2_32 winmm iphlpapi
    )

    target_compile_definitions(audio_flow_test PRIVATE
        NOMINMAX
        SODIUM_STATIC
        PA_USE_STATIC        # signals audio_engine to use direct Pa_ calls
    )

    add_custom_command(TARGET audio_flow_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/bin/webrtc-audio-processing-1-3.dll
            ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/bin/webrtc-audio-coding-1-3.dll
            ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libsodium-26.dll
            ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libgcc_s_seh-1.dll
            ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libwinpthread-1.dll
            $<TARGET_FILE_DIR:audio_flow_test>
    )
endif()

add_subdirectory(Nuummite)
