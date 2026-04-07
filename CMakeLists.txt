cmake_minimum_required(VERSION 3.20)
project(voice_two_way LANGUAGES C CXX)

if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(VOICE_BUILD_AUDIO_NATIVE "Build native audio test tools" OFF)

set(OPUS_DIR ${CMAKE_SOURCE_DIR}/third_party/opus)
set(OPUS_INCLUDE_DIR ${OPUS_DIR})
set(OPUS_DLL ${OPUS_DIR}/opus.dll)
set(OPUS_LIB ${OPUS_DIR}/opus.lib)
set(PORTAUDIO_DLL "C:/msys64/ucrt64/bin/libportaudio.dll" CACHE FILEPATH "PortAudio runtime DLL")

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
    Nuummite/common/opus_codec.cpp
    Nuummite/common/socket_utils.cpp
    Nuummite/common/winsock_init.cpp
)
target_include_directories(voice_shared PUBLIC ${CMAKE_SOURCE_DIR}/Nuummite/common ${OPUS_INCLUDE_DIR})
target_link_libraries(voice_shared PUBLIC opus ws2_32)

if (VOICE_BUILD_AUDIO_NATIVE)
    add_subdirectory(audio_native)
endif()
