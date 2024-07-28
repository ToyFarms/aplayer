cmake_minimum_required(VERSION 3.10)
project(audio_player)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FindPkgConfig)
find_package(PkgConfig REQUIRED)

add_executable(aplayer
    ./src/main.c

    ./src/struct/array.c
    ./src/struct/dict.c
    ./src/struct/queue.c
    ./src/struct/ring_buf.c

    ./src/term/term_linux.c

    ./src/audio/audio.c
    ./src/audio/audio_mixer.c

    ./src/audio/source/audio_file.c

    ./src/audio/effect/audio_gain.c
    ./src/audio/effect/audio_pan.c
    ./src/audio/effect/audio_filter.c
)

target_link_libraries(aplayer PUBLIC m pthread)

pkg_check_modules(portaudio REQUIRED)
target_link_libraries(aplayer PRIVATE portaudio)

pkg_check_modules(LIBAV REQUIRED libavformat libavcodec libavutil libswresample)
target_include_directories(aplayer PRIVATE ${LIBAV_INCLUDE_DIRS})
target_link_directories(aplayer PRIVATE ${LIBAV_LIBRARY_DIRS})
target_link_libraries(aplayer PRIVATE ${LIBAV_LIBRARIES})

target_include_directories(aplayer PUBLIC src/include)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
)
