cmake_minimum_required(VERSION 3.20)
project(voice_two_way LANGUAGES C CXX)

if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(VOICE_BUILD_AUDIO_NATIVE "Build native audio test tools" OFF)
option(VOICE_BUILD_QT_CLIENT "Build Qt6 Widgets client (voice_client)" ON)

set(OPUS_DIR ${CMAKE_SOURCE_DIR}/third_party/opus)
set(OPUS_INCLUDE_DIR ${OPUS_DIR})
set(OPUS_DLL ${OPUS_DIR}/opus.dll)
set(OPUS_LIB ${OPUS_DIR}/opus.lib)
set(_DEFAULT_PORTAUDIO_DLL "${CMAKE_SOURCE_DIR}/third_party/libportaudio/libportaudio.dll")
if (EXISTS "${_DEFAULT_PORTAUDIO_DLL}")
    set(PORTAUDIO_DLL "${_DEFAULT_PORTAUDIO_DLL}" CACHE FILEPATH "PortAudio runtime DLL")
else()
    set(PORTAUDIO_DLL "C:/msys64/ucrt64/bin/libportaudio.dll" CACHE FILEPATH "PortAudio runtime DLL")
endif()

set(LIBSODIUM_DIR ${CMAKE_SOURCE_DIR}/third_party/libsodium)
set(LIBSODIUM_INCLUDE_DIR ${LIBSODIUM_DIR}/include)
set(LIBSODIUM_DLL ${LIBSODIUM_DIR}/libsodium.dll)
set(LIBSODIUM_LIB ${LIBSODIUM_DIR}/libsodium.lib)

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

if (EXISTS ${LIBSODIUM_LIB})
    add_library(libsodium SHARED IMPORTED)
    set_target_properties(libsodium PROPERTIES
        IMPORTED_IMPLIB ${LIBSODIUM_LIB}
        IMPORTED_LOCATION ${LIBSODIUM_DLL}
    )
else()
    message(FATAL_ERROR "Missing libsodium import library: ${LIBSODIUM_LIB}")
endif()

if (EXISTS ${RNNOISE_LIB})
    add_library(rnnoise SHARED IMPORTED)
    set_target_properties(rnnoise PROPERTIES
        IMPORTED_IMPLIB ${RNNOISE_LIB}
        IMPORTED_LOCATION ${RNNOISE_DLL}
    )
else()
    message(FATAL_ERROR "Missing rnnoise import library: ${RNNOISE_LIB}")
endif()

if (EXISTS ${WEBRTC_AUDIOPROC_LIB})
    add_library(webrtc_audio_processing SHARED IMPORTED)
    set_target_properties(webrtc_audio_processing PROPERTIES
        IMPORTED_IMPLIB ${WEBRTC_AUDIOPROC_LIB}
        IMPORTED_LOCATION ${WEBRTC_AUDIOPROC_DLL}
    )
else()
    message(FATAL_ERROR "Missing webrtc-audio-processing import library: ${WEBRTC_AUDIOPROC_LIB}")
endif()

add_library(voice_shared STATIC
    Nuummite/common/audio_packet.cpp
    Nuummite/common/libsodium_wrapper.cpp
    Nuummite/common/opus_codec.cpp
    Nuummite/common/socket_utils.cpp
    Nuummite/common/winsock_init.cpp
    Nuummite/p2p/rtp_transport.cpp
    Nuummite/p2p/peer_discovery.cpp
    Nuummite/audio/audio_engine.cpp
    Nuummite/audio/aec_processor.cpp
    Nuummite/audio/jitter_buffer.cpp
    Nuummite/audio/rnnoise_processor.cpp
    Nuummite/audio/webrtc_apm.cpp
)
target_include_directories(voice_shared PUBLIC
    ${CMAKE_SOURCE_DIR}/Nuummite
    ${CMAKE_SOURCE_DIR}/Nuummite/audio
    ${CMAKE_SOURCE_DIR}/Nuummite/common
    ${CMAKE_SOURCE_DIR}/Nuummite/p2p
    ${OPUS_INCLUDE_DIR}
    ${LIBSODIUM_INCLUDE_DIR}
    ${RNNOISE_INCLUDE_DIR}
    ${WEBRTC_INCLUDE_DIR}
    ${WEBRTC_INCLUDE_DIR}/webrtc-audio-processing-1
)
target_compile_definitions(voice_shared PUBLIC NOMINMAX)
target_link_libraries(voice_shared PUBLIC opus libsodium rnnoise webrtc_audio_processing ws2_32 winmm iphlpapi)
set_target_properties(voice_shared PROPERTIES AUTOMOC OFF AUTOUIC OFF)

if (VOICE_BUILD_AUDIO_NATIVE)
    add_subdirectory(audio_native)
endif()

if (VOICE_BUILD_QT_CLIENT)
    if (DEFINED ENV{QT6_ROOT})
        list(PREPEND CMAKE_PREFIX_PATH "$ENV{QT6_ROOT}")
    endif()
    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    add_executable(voice_client WIN32
        Nuummite/client/main.cpp
        Nuummite/client/MainWindow.cpp
        Nuummite/client/ParticipantRowWidget.cpp
        Nuummite/client/SettingsDialog.cpp
        Nuummite/client/VolumeControlPanel.cpp
        Nuummite/client/resources.qrc

        Nuummite/ui/Popup_message.ui
        Nuummite/ui/main_window.ui
        Nuummite/ui/participant_item.ui
        Nuummite/ui/settings_dialog.ui
        Nuummite/ui/volume_control.ui
    )
    target_link_libraries(voice_client PRIVATE voice_shared Qt6::Widgets)
    target_include_directories(voice_client PRIVATE ${CMAKE_SOURCE_DIR}/Nuummite/client)
    set_property(TARGET voice_client PROPERTY AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/Nuummite/ui)
    set_target_properties(voice_client PROPERTIES AUTORCC ON)

    if (WIN32)
        # Make the build output runnable without manual PATH edits.
        if (EXISTS "${PORTAUDIO_DLL}")
            add_custom_command(TARGET voice_client POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:opus>
                    $<TARGET_FILE:libsodium>
                    $<TARGET_FILE:rnnoise>
                    $<TARGET_FILE:webrtc_audio_processing>
                    "${PORTAUDIO_DLL}"
                    $<TARGET_FILE_DIR:voice_client>
                VERBATIM
            )
        else()
            add_custom_command(TARGET voice_client POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:opus>
                    $<TARGET_FILE:libsodium>
                    $<TARGET_FILE:rnnoise>
                    $<TARGET_FILE:webrtc_audio_processing>
                    $<TARGET_FILE_DIR:voice_client>
                VERBATIM
            )
        endif()

        # Deploy Qt DLLs + plugins (platforms/qwindows.dll, etc.) next to the exe when available.
        set(_QT_BIN_DIR "${Qt6_DIR}/../../../bin")
        if (EXISTS "${_QT_BIN_DIR}/windeployqt.exe")
            add_custom_command(TARGET voice_client POST_BUILD
                COMMAND "${_QT_BIN_DIR}/windeployqt.exe"
                    --no-translations
                    --no-opengl-sw
                    --compiler-runtime
                    "$<TARGET_FILE:voice_client>"
                VERBATIM
            )
        endif()
    endif()
endif()
