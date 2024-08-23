cmake_minimum_required(VERSION 3.10)
project(audio_player)

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DLOGGER_BASEPATH_LENGTH=${SOURCE_PATH_SIZE}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/src/audio/plugins/build_plugins.cmake)

include(FindPkgConfig)
find_package(PkgConfig REQUIRED)
# string(APPEND CMAKE_C_FLAGS "-Wall -Werror -pg")

add_executable(aplayer
    ./src/main.c
    ./src/fs_linux.c
    ./src/logger.c
    ./src/exception_linux.c

    ./src/struct/array.c
    ./src/struct/dict.c
    ./src/struct/queue.c
    ./src/struct/ring_buf.c
    ./src/struct/ds.c

    ./src/term/term_linux.c
    ./src/term/term_draw.c

    ./src/audio/audio.c
    ./src/audio/audio_source.c
    ./src/audio/audio_mixer.c
    ./src/audio/audio_effect.c
    ./src/audio/plugin.c

    ./src/audio/source/audio_file.c

    ./src/audio/effect/audio_gain.c
    ./src/audio/effect/audio_pan.c
    ./src/audio/effect/audio_filter.c

    ./src/audio/analyzer/audio_rms.c
    ./src/audio/analyzer/audio_fft.c
)

target_link_libraries(aplayer PUBLIC m pthread)

pkg_check_modules(ncurses REQUIRED)
target_link_libraries(aplayer PRIVATE ncurses)

pkg_check_modules(fftw3 REQUIRED)
target_link_libraries(aplayer PRIVATE fftw3)

pkg_check_modules(portaudio REQUIRED)
target_link_libraries(aplayer PRIVATE portaudio)

pkg_check_modules(LIBAV REQUIRED libavformat libavcodec libavutil libswresample)
target_include_directories(aplayer PRIVATE ${LIBAV_INCLUDE_DIRS})
target_link_directories(aplayer PRIVATE ${LIBAV_LIBRARY_DIRS})
target_link_libraries(aplayer PRIVATE ${LIBAV_LIBRARIES})

target_include_directories(aplayer PRIVATE ./src/include)
target_include_directories(aplayer PRIVATE ./thirdparty/include/)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
)
