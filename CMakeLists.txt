cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

project(audio_player)

file(GLOB SOURCE src/*.c)

add_executable(
  aplayer
  ${SOURCE}
)

set(CMAKE_CXX_FLAGS "-O3")
target_include_directories(aplayer PUBLIC src/include)

target_include_directories(aplayer PUBLIC vcpkg_installed/x64-windows/include)
target_link_directories(aplayer PUBLIC vcpkg_installed/x64-windows/lib)
target_link_libraries(aplayer PUBLIC
  avformat
  avcodec
  avutil
  avutil
  swresample
  portaudio
  sdl2
)