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
set(PORTAUDIO_DLL ${CMAKE_SOURCE_DIR}/third_party/libportaudio/libportaudio.dll)
set(RNNOISE_DIR ${CMAKE_SOURCE_DIR}/third_party/rnnoise)
set(RNNOISE_INCLUDE_DIR ${RNNOISE_DIR})
set(RNNOISE_DLL ${RNNOISE_DIR}/rnnoise.dll)
set(RNNOISE_LIB ${RNNOISE_DIR}/rnnoise.lib)

set(WEBRTC_DIR ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing)
set(WEBRTC_INCLUDE_DIR ${WEBRTC_DIR}/include)
set(WEBRTC_AUDIOPROC_DLL ${WEBRTC_DIR}/bin/webrtc-audio-processing-1-3.dll)
set(WEBRTC_AUDIOCODING_DLL ${WEBRTC_DIR}/bin/webrtc-audio-coding-1-3.dll)
set(WEBRTC_AUDIOPROC_LIB ${WEBRTC_DIR}/lib/webrtc-audio-processing-1-msvc.lib)
set(WEBRTC_AUDIOCODING_LIB ${WEBRTC_DIR}/lib/webrtc-audio-coding-1.lib)
set(LIBSODIUM_DLL ${CMAKE_SOURCE_DIR}/third_party/libsodium/libsodium.dll)
set(LIBSODIUM_DLL_VERSIONED ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libsodium-26.dll)
set(LIBGCC_SEH_DLL ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libgcc_s_seh-1.dll)
set(LIBWINPTHREAD_DLL ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libwinpthread-1.dll)
set(VOICE_RUNTIME_BUNDLE_DIR ${CMAKE_SOURCE_DIR}/third_party/runtime/bin)
set(VC_REDIST_X64_EXE ${VOICE_RUNTIME_BUNDLE_DIR}/vc_redist.x64.exe)

function(voice_deploy_runtime TARGET_NAME)
    if (EXISTS "${VOICE_RUNTIME_BUNDLE_DIR}")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${VOICE_RUNTIME_BUNDLE_DIR}"
                $<TARGET_FILE_DIR:${TARGET_NAME}>
        )
        if (EXISTS "${VC_REDIST_X64_EXE}")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${VC_REDIST_X64_EXE}"
                    $<TARGET_FILE_DIR:${TARGET_NAME}>/vc_redist.x64.exe
            )
        else()
            message(STATUS "VC++ redistributable not found at ${VC_REDIST_X64_EXE}; place vc_redist.x64.exe there to bundle it into bin/.")
        endif()
    else()
        message(WARNING "Runtime bundle not found at ${VOICE_RUNTIME_BUNDLE_DIR}; build outputs will not be mirrored from third_party.")
    endif()
endfunction()

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

    voice_deploy_runtime(audio_flow_test)
endif()

add_subdirectory(Nuummite)
