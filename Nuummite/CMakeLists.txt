set(CLIENT_SOURCES
    client/main.cpp
    client/MainWindow.cpp
    client/ParticipantRowWidget.cpp
    client/SettingsDialog.cpp
    client/VolumeControlPanel.cpp
    client/EncryptionDialog.cpp
    client/LoadingSpinner.cpp
    client/CrystalRoomSelectionDialog.cpp
    audio/audio_engine.cpp
    audio/aec_processor.cpp
    audio/jitter_buffer.cpp
    audio/rnnoise_processor.cpp
    audio/webrtc_apm.cpp
    p2p/peer_discovery.cpp
    p2p/rtp_transport.cpp
    client/resources.qrc
    ui/main_window.ui
    ui/participant_item.ui
    ui/settings_dialog.ui
    ui/volume_control.ui
    ui/encryption_dialog.ui
)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}/Nuummite/ui
    ${CMAKE_SOURCE_DIR}/Nuummite/client
)

find_package(Qt6 6.5 COMPONENTS Core Gui Widgets Network UiTools REQUIRED)

add_executable(voice_client ${CLIENT_SOURCES})

target_include_directories(voice_client PRIVATE
    ${CMAKE_SOURCE_DIR}/Nuummite
    ${CMAKE_SOURCE_DIR}/Nuummite/common
    ${CMAKE_SOURCE_DIR}/Nuummite/audio
    ${CMAKE_SOURCE_DIR}/Nuummite/p2p
    ${OPUS_INCLUDE_DIR}
    ${PORTAUDIO_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/rnnoise
    ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/include
    ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/include/webrtc-audio-processing-1
    ${SODIUM_INCLUDE_DIR}
)

target_compile_definitions(voice_client PRIVATE
    NOMINMAX
    SODIUM_STATIC          # libsodium: suppress dllimport, use static symbols
    PA_USE_STATIC          # audio_engine.cpp: use direct Pa_ calls instead of LoadLibrary
)

target_link_libraries(voice_client PRIVATE
    voice_shared
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::UiTools
    portaudio_static
    rnnoise_static
    webrtc_audio_processing
    webrtc_audio_coding
    ws2_32
    winmm
    iphlpapi
    ole32
    uuid
    ksuser
    mfplat
    mfuuid
    wmcodecdspuuid
)

# ── Post-build: copy UI files ─────────────────────────────────────────────────
add_custom_command(TARGET voice_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Nuummite/ui
        $<TARGET_FILE_DIR:voice_client>/ui
)

add_custom_command(TARGET voice_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/bin/webrtc-audio-processing-1-3.dll
        ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/bin/webrtc-audio-coding-1-3.dll
        ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libsodium-26.dll
        ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libgcc_s_seh-1.dll
        ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libwinpthread-1.dll
        $<TARGET_FILE_DIR:voice_client>
)

add_custom_command(TARGET voice_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/Nuummite/technical-support.ico
        $<TARGET_FILE_DIR:voice_client>/technical-support.ico
)

set(VC_REDIST_X64_EXE ${CMAKE_SOURCE_DIR}/third_party/runtime/bin/vc_redist.x64.exe)
if (EXISTS "${VC_REDIST_X64_EXE}")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VC_REDIST_X64_EXE}"
            $<TARGET_FILE_DIR:voice_client>/vc_redist.x64.exe
    )
else()
    message(STATUS "VC++ redistributable not found at ${VC_REDIST_X64_EXE}; place vc_redist.x64.exe there to include it in bin/.")
endif()
