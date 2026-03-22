set(CLIENT_SOURCES
    launcher/main.cpp
    audio/audio_engine.cpp
    p2p/peer_discovery.cpp
    p2p/rtp_transport.cpp
    ui/main_window.cpp
    ui/volume_control_panel.cpp
    ui/components/participant_row.cpp
    ui/components/settings_dialog.cpp
    ui/components/startup_dialog.cpp
    ui/components/ui_helpers.cpp
)


add_executable(voice_client ${CLIENT_SOURCES})

# Disable AUTOMOC for this target to avoid cmake_autogen spawning issues; handle moc manually.
set_target_properties(voice_client PROPERTIES AUTOMOC OFF)

set(VOICE_CLIENT_MOC_HEADERS
    ui/volume_control_panel.h
    ui/main_window.h
    ui/components/participant_row.h
    ui/components/settings_dialog.h
    ui/components/startup_dialog.h
)
qt6_wrap_cpp(VOICE_CLIENT_MOC_SOURCES ${VOICE_CLIENT_MOC_HEADERS})
target_sources(voice_client PRIVATE ${VOICE_CLIENT_MOC_SOURCES})

target_include_directories(voice_client PRIVATE
    ${CMAKE_SOURCE_DIR}/Nuummite/common
    ${CMAKE_SOURCE_DIR}/Nuummite
    ${OPUS_INCLUDE_DIR}
)

target_link_libraries(voice_client PRIVATE
    Qt6::Widgets
    Qt6::UiTools
    voice_shared
    ws2_32
    winmm
)

target_compile_definitions(voice_client PRIVATE
    NOMINMAX
)

set(VOICE_VCPKG_BIN "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin")

set(QT_DEPLOY_TOOL "")
if (WINDEPLOYQT_EXECUTABLE AND EXISTS "${WINDEPLOYQT_EXECUTABLE}")
    set(QT_DEPLOY_TOOL "${WINDEPLOYQT_EXECUTABLE}")
elseif (Qt6_DIR)
    get_filename_component(_qt6_cmake_dir "${Qt6_DIR}" ABSOLUTE)
    get_filename_component(_qt6_cmake_root "${_qt6_cmake_dir}" DIRECTORY)
    get_filename_component(_qt6_lib_dir "${_qt6_cmake_root}" DIRECTORY)
    get_filename_component(_qt6_prefix "${_qt6_lib_dir}" DIRECTORY)
    set(QT_DEPLOY_TOOL "${_qt6_prefix}/bin/windeployqt.exe")
endif()

add_custom_command(TARGET voice_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Nuummite/ui
        $<TARGET_FILE_DIR:voice_client>/ui
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/Nuummite/technical-support.ico
        $<TARGET_FILE_DIR:voice_client>/technical-support.ico
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OPUS_DLL}
        $<TARGET_FILE_DIR:voice_client>/opus.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PORTAUDIO_DLL}
        $<TARGET_FILE_DIR:voice_client>/libportaudio.dll
)

if (WIN32 AND EXISTS "${QT_DEPLOY_TOOL}" AND VOICE_RUN_QT_DEPLOY)
    add_custom_command(TARGET voice_client POST_BUILD
        COMMAND "${QT_DEPLOY_TOOL}"
            --no-compiler-runtime
            --no-translations
            $<$<CONFIG:Debug>:--debug>
            $<$<NOT:$<CONFIG:Debug>>:--release>
            "$<TARGET_FILE:voice_client>"
    )
endif()
