cmake_minimum_required(VERSION 3.10)
project(audio_player)

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DLOGGER_BASEPATH_LENGTH=${SOURCE_PATH_SIZE}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FindPkgConfig)
find_package(PkgConfig REQUIRED)
string(APPEND CMAKE_C_FLAGS " -Wall -Werror -pg -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-function")

add_executable(aplayer
    ./src/main.c
    ./src/fs_linux.c
    ./src/logger.c
    ./src/exception_linux.c
    ./src/app.c
    ./src/utils.c
    ./src/playlist.c
    ./src/clock.c
    ./src/imgconv.c
    ./src/image.c
    ./src/session.c

    ./src/struct/array.c
    ./src/struct/dict.c
    ./src/struct/queue.c
    ./src/struct/ring_buf.c
    ./src/struct/ds.c
    ./src/struct/arena_allocator.c
    ./src/struct/pathlib.c

    ./src/term/term_linux.c
    ./src/term/term_draw.c

    ./src/audio/audio.c
    ./src/audio/audio_source.c
    ./src/audio/audio_mixer.c
    ./src/audio/audio_effect.c

    ./src/audio/source/audio_file.c

    ./src/audio/effect/audio_gain.c
    ./src/audio/effect/audio_pan.c
    ./src/audio/effect/audio_filter.c
    ./src/audio/effect/audio_autogain.c

    ./src/audio/analyzer/audio_rms.c
    ./src/audio/analyzer/audio_fft.c

    ./src/ui/color.c
    ./src/ui/ui.c
    ./src/ui/layout.c
    ./src/ui/image_renderer.c

    ./src/ui/widgets/list.c
    ./src/ui/widgets/hprogress.c
    ./src/ui/widgets/debug.c
    ./src/ui/widgets/rect.c
    ./src/ui/widgets/timestamp.c
    ./src/ui/widgets/volume.c
    ./src/ui/widgets/media_control.c
    ./src/ui/widgets/vu_meter.c
    ./src/ui/widgets/statusline.c
    ./src/ui/widgets/tabs.c
    ./src/ui/widgets/art.c

    ./thirdparty/wcwidth.c
    ./thirdparty/cJSON.c
)

target_link_libraries(aplayer PRIVATE m pthread fftw3 portaudio)

if (NOT WIN32)
  target_link_libraries(aplayer PRIVATE ncurses)
endif()

pkg_check_modules(LIBAV REQUIRED libavformat libavcodec libavutil libswresample libswscale libavfilter libsixel libebur128)
target_include_directories(aplayer PRIVATE ${LIBAV_INCLUDE_DIRS})
target_link_directories(aplayer PRIVATE ${LIBAV_LIBRARY_DIRS})
target_link_libraries(aplayer PRIVATE ${LIBAV_LIBRARIES})

target_include_directories(aplayer PRIVATE
  ./src/include
  ./thirdparty/include
)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
)
