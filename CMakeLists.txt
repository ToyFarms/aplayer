cmake_minimum_required(VERSION 3.10)

project(audio_player)

if(WIN32)
  set(OS-DIR windows)
elseif(UNIX AND NOT APPLE)
  set(OS-DIR linux)
elseif(APPLE)
  set(OS-DIR macos)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-unused-function -Wno-unused-const-variable")

add_executable(
  aplayer
  src/main.c
  src/libaudio.c
  src/libdecoder.c
  src/libplayer.c
  src/libstream.c
  src/libtime.c
  src/sb.c
  src/libplaylist.c
  src/ini.c
  src/libsession.c
  src/cJSON.c
  src/libprocessing.c
  src/${OS-DIR}/libcli.c
  src/${OS-DIR}/libfile.c
  src/${OS-DIR}/libhelper.c
  src/${OS-DIR}/libhook.c
  src/common/libhelperc.c
  src/common/libclic.c
  src/common/libfilec.c
)

target_include_directories(aplayer PUBLIC src/include)

target_include_directories(aplayer PUBLIC vcpkg_installed/x64-windows/include)
target_link_directories(aplayer PUBLIC vcpkg_installed/x64-windows/lib)
target_link_libraries(aplayer PUBLIC
  avformat
  avcodec
  avutil
  swresample
  portaudio
)