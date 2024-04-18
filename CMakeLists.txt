cmake_minimum_required(VERSION 3.10)

project(audio_player)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable")
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  # to fix sds overflow warning
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -E")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
  src/ap_playlist.c
  thirdparty/wcwidth.c
  thirdparty/sds.c

  src/os/${OS-DIR}/ap_terminal.c
  src/os/${OS-DIR}/ap_utils.c
  src/os/common/ap_terminal_common.c
  src/os/common/ap_os_common.c
  src/os/common/ap_utils_common.c

  src/audio/ap_audio.c
  src/audio/ap_audio_output.c
  src/audio/ap_audio_processing.c

  src/ui/ap_ui.c
  src/ui/tui/ap_draw.c
  src/ui/tui/ap_tui.c

  ${widgets}
)

target_include_directories(aplayer PUBLIC src/include)
target_include_directories(aplayer PUBLIC src/audio/include)
target_include_directories(aplayer PUBLIC src/ui/include)
target_include_directories(aplayer PUBLIC src/ui/tui/include)
target_include_directories(aplayer PUBLIC vcpkg_installed/x64-windows/include)
target_include_directories(aplayer PUBLIC thirdparty)

target_link_directories(aplayer PUBLIC vcpkg_installed/x64-windows/lib)
target_link_libraries(aplayer PUBLIC
  avformat
  avcodec
  avutil
  swresample
  portaudio
)
