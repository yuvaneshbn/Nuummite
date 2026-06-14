set(CLIENT_SOURCES
    client/main.cpp
    client/MainWindow.cpp
    client/ParticipantRowWidget.cpp
    client/SettingsDialog.cpp
    client/VolumeControlPanel.cpp
    client/EncryptionDialog.cpp
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
    ui/Popup_message.ui
    ui/settings_dialog.ui
    ui/volume_control.ui
    ui/encryption_dialog.ui 
)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/Nuummite/ui)

find_package(Qt6 6.5 COMPONENTS Core Gui Widgets Network UiTools REQUIRED)

add_executable(voice_client ${CLIENT_SOURCES})

target_include_directories(voice_client PRIVATE
    ${CMAKE_SOURCE_DIR}/Nuummite
    ${CMAKE_SOURCE_DIR}/Nuummite/common
    ${CMAKE_SOURCE_DIR}/Nuummite/audio
    ${CMAKE_SOURCE_DIR}/Nuummite/p2p
    ${OPUS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/rnnoise
    ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/include
    ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/include/webrtc-audio-processing-1
)

target_link_libraries(voice_client PRIVATE
    voice_shared
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::UiTools
    ws2_32
    winmm
    iphlpapi
    ${CMAKE_SOURCE_DIR}/third_party/rnnoise/rnnoise.lib
    ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/lib/webrtc-audio-processing-1-msvc.lib
    ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/lib/webrtc-audio-coding-1.lib
)

target_compile_definitions(voice_client PRIVATE
    NOMINMAX
)

add_custom_command(TARGET voice_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Nuummite/ui
        $<TARGET_FILE_DIR:voice_client>/ui
)

voice_deploy_runtime(voice_client)

add_custom_command(TARGET voice_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/Nuummite/technical-support.ico
        $<TARGET_FILE_DIR:voice_client>/technical-support.ico
)
