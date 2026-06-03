set(CLIENT_SOURCES
    client/main.cpp
    client/MainWindow.cpp
    client/ParticipantRowWidget.cpp
    client/SettingsDialog.cpp
    client/VolumeControlPanel.cpp
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

if (OPUS_DLL AND EXISTS "${OPUS_DLL}")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPUS_DLL}
            $<TARGET_FILE_DIR:voice_client>/opus.dll
    )
endif()

if (PORTAUDIO_DLL AND EXISTS "${PORTAUDIO_DLL}")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PORTAUDIO_DLL}
            $<TARGET_FILE_DIR:voice_client>/libportaudio.dll
    )
endif()

# Deploy libsodium runtime libraries from third_party
if (EXISTS "${CMAKE_SOURCE_DIR}/third_party/libsodium/libsodium.dll")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/third_party/libsodium/libsodium.dll
            $<TARGET_FILE_DIR:voice_client>/libsodium.dll
    )
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libsodium-26.dll")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libsodium-26.dll
            $<TARGET_FILE_DIR:voice_client>/libsodium-26.dll
    )
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libgcc_s_seh-1.dll")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libgcc_s_seh-1.dll
            $<TARGET_FILE_DIR:voice_client>/libgcc_s_seh-1.dll
    )
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libwinpthread-1.dll")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/third_party/libsodium/bin/libwinpthread-1.dll
            $<TARGET_FILE_DIR:voice_client>/libwinpthread-1.dll
    )
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/third_party/rnnoise/rnnoise.dll")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/third_party/rnnoise/rnnoise.dll
            $<TARGET_FILE_DIR:voice_client>/rnnoise.dll
    )
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/bin/webrtc-audio-processing-1-3.dll")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/bin/webrtc-audio-processing-1-3.dll
            $<TARGET_FILE_DIR:voice_client>/webrtc-audio-processing-1-3.dll
    )
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/bin/webrtc-audio-coding-1-3.dll")
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/third_party/webrtc_audio_processing/bin/webrtc-audio-coding-1-3.dll
            $<TARGET_FILE_DIR:voice_client>/webrtc-audio-coding-1-3.dll
    )
endif()

add_custom_command(TARGET voice_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/Nuummite/technical-support.ico
        $<TARGET_FILE_DIR:voice_client>/technical-support.ico
)
