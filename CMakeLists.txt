cmake_minimum_required(VERSION 3.10)
project(audio_player)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable")
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(VCPKG_TARGET_TRIPLET "x64-windows")
set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

include(${CMAKE_TOOLCHAIN_FILE})

set(OS-DIR NOT_SUPPORTED)

if(WIN32)
  set(OS-DIR windows)
elseif(UNIX AND NOT APPLE)
  set(OS-DIR linux)
elseif(APPLE)
  set(OS-DIR macos)
endif()

if(OS-DIR EQUAL NOT_SUPPORTED)
  message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

file(GLOB widgets src/ui/tui/widgets/*.c)

add_executable(
  aplayer
  src/main.c
  src/ap_array.c
  src/ap_queue.c
  src/ap_playlist.c
  src/ap_dict.c
  thirdparty/wcwidth.c
  thirdparty/sds.c

  src/os/${OS-DIR}/ap_terminal.c
  src/os/${OS-DIR}/ap_utils.c
  src/os/${OS-DIR}/ap_timing.c
  src/os/common/ap_terminal_common.c
  src/os/common/ap_os_common.c
  src/os/common/ap_utils_common.c

  src/audio/ap_audio.c
  src/audio/ap_audio_dec.c
  src/audio/ap_audio_output.c
  src/audio/ap_audio_processing.c

  src/ui/ap_ui.c
  src/ui/ap_widget.c
  src/ui/tui/ap_draw.c
  src/ui/tui/ap_tui.c

  ${widgets}
)

find_package(portaudio REQUIRED)
target_link_libraries(aplayer PRIVATE portaudio)

find_package(ffmpeg REQUIRED)
target_include_directories(aplayer PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(aplayer PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(aplayer PRIVATE avformat avcodec avutil swresample)

target_include_directories(aplayer PUBLIC src/include)
target_include_directories(aplayer PUBLIC src/audio/include)
target_include_directories(aplayer PUBLIC src/ui/include)
target_include_directories(aplayer PUBLIC src/ui/tui/include)
target_include_directories(aplayer PUBLIC thirdparty)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
)
