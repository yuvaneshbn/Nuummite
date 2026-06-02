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

option(VOICE_BUILD_AUDIO_NATIVE "Build native audio test tools" OFF)
option(VOICE_BUILD_AUDIO_FLOW_TEST "Build audio flow diagnostic tool" ON)

set(OPUS_DIR ${CMAKE_SOURCE_DIR}/third_party/opus)
set(OPUS_INCLUDE_DIR ${OPUS_DIR})
set(OPUS_DLL ${OPUS_DIR}/opus.dll)
set(OPUS_LIB ${OPUS_DIR}/opus.lib)
set(PORTAUDIO_DLL "C:/msys64/ucrt64/bin/libportaudio.dll" CACHE FILEPATH "PortAudio runtime DLL")
set(RNNOISE_DIR ${CMAKE_SOURCE_DIR}/third_party/rnnoise)
set(RNNOISE_INCLUDE_DIR ${RNNOISE_DIR})
set(RNNOISE_DLL ${RNNOISE_DIR}/rnnoise.dll)
set(RNNOISE_LIB ${RNNOISE_DIR}/rnnoise.lib)

set(WEBRTC_DIR ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing)
set(WEBRTC_INCLUDE_DIR ${WEBRTC_DIR}/include)
set(WEBRTC_AUDIOPROC_DLL ${WEBRTC_DIR}/bin/webrtc-audio-processing-1-3.dll)
set(WEBRTC_AUDIOPROC_LIB ${WEBRTC_DIR}/lib/webrtc-audio-processing-1-msvc.lib)

if (EXISTS ${OPUS_LIB})
    add_library(opus SHARED IMPORTED)
    set_target_properties(opus PROPERTIES
        IMPORTED_IMPLIB ${OPUS_LIB}
        IMPORTED_LOCATION ${OPUS_DLL}
    )
else()
    message(FATAL_ERROR "Missing Opus import library: ${OPUS_LIB}")
endif()

add_library(voice_shared STATIC
    Nuummite/common/audio_packet.cpp
    Nuummite/common/libsodium_wrapper.cpp
    Nuummite/common/opus_codec.cpp
    Nuummite/common/socket_utils.cpp
    Nuummite/common/winsock_init.cpp
)
target_include_directories(voice_shared PUBLIC ${CMAKE_SOURCE_DIR}/Nuummite/common ${OPUS_INCLUDE_DIR})
target_link_libraries(voice_shared PUBLIC opus ws2_32)

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
        ${RNNOISE_INCLUDE_DIR}
        ${WEBRTC_INCLUDE_DIR}
        ${WEBRTC_INCLUDE_DIR}/webrtc-audio-processing-1
    )

    target_link_libraries(audio_flow_test PRIVATE
        voice_shared
        ws2_32
        winmm
        iphlpapi
        ${RNNOISE_LIB}
        ${WEBRTC_AUDIOPROC_LIB}
    )

    target_compile_definitions(audio_flow_test PRIVATE NOMINMAX)

    if (EXISTS "${OPUS_DLL}")
        add_custom_command(TARGET audio_flow_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${OPUS_DLL}
                $<TARGET_FILE_DIR:audio_flow_test>/opus.dll
        )
    endif()
    if (EXISTS "${RNNOISE_DLL}")
        add_custom_command(TARGET audio_flow_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${RNNOISE_DLL}
                $<TARGET_FILE_DIR:audio_flow_test>/rnnoise.dll
        )
    endif()
    if (EXISTS "${WEBRTC_AUDIOPROC_DLL}")
        add_custom_command(TARGET audio_flow_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${WEBRTC_AUDIOPROC_DLL}
                $<TARGET_FILE_DIR:audio_flow_test>/webrtc-audio-processing-1-3.dll
        )
    endif()
    if (EXISTS "${PORTAUDIO_DLL}")
        add_custom_command(TARGET audio_flow_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${PORTAUDIO_DLL}
                $<TARGET_FILE_DIR:audio_flow_test>/libportaudio.dll
        )
    endif()
endif()

add_subdirectory(Nuummite)
