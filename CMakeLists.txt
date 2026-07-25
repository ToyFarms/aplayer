cmake_minimum_required(VERSION 3.14)
project(audio_player C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (NOT WIN32)
    find_package(PkgConfig REQUIRED)
endif()

if (MSVC)
    add_compile_options(/utf-8)
endif()

include(FetchContent)

if (WIN32)
    FetchContent_Declare(
        pthread-win32
        GIT_REPOSITORY https://github.com/GerHobbelt/pthread-win32
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pthread-win32)

    FetchContent_Declare(
        ffmpeg_prebuilt
        URL https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip
    )
    FetchContent_MakeAvailable(ffmpeg_prebuilt)
else()
    pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
        libavformat
        libavcodec
        libavutil
        libswresample
        libswscale
        libavfilter
    )
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_BINDINGS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WITH_ZLIB ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    taglib
    GIT_REPOSITORY https://github.com/taglib/taglib.git
    GIT_TAG v2.0.2
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    libebur128
    GIT_REPOSITORY https://github.com/jiixyj/libebur128.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    fftw3
    URL https://www.fftw.org/pub/fftw/fftw-3.3.11.tar.gz
)

FetchContent_MakeAvailable(taglib libebur128 portaudio fftw3)

if (WIN32)
    set(PLATFORM_SUFFIX win)
else()
    set(PLATFORM_SUFFIX linux)
endif()

add_executable(aplayer
    ./src/main.c
    ./src/fs_${PLATFORM_SUFFIX}.c
    ./src/logger.c
    ./src/exception_${PLATFORM_SUFFIX}.c
    ./src/app.c
    ./src/utils.c
    ./src/playlist.c
    ./src/clock.c
    ./src/imgconv.c
    ./src/image.c
    ./src/session.c
    ./src/metagen.c
    ./src/metagen_lyrics.cpp
    ./src/ssixel.c

    ./src/struct/array.c
    ./src/struct/dict.c
    ./src/struct/queue.c
    ./src/struct/ring_buf.c
    ./src/struct/ds.c
    ./src/struct/arena_allocator.c
    ./src/struct/pathlib.c

    ./src/term/term_${PLATFORM_SUFFIX}.c
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
    ./src/ui/widgets/lyrics.c

    ./thirdparty/wcwidth.c
    ./thirdparty/cJSON.c
)

target_compile_definitions(aplayer PRIVATE TAGLIB_STATIC)

if (WIN32)
    target_link_libraries(aplayer PRIVATE pthreadVC3 avformat avcodec avutil swresample swscale avfilter)
    target_link_directories(aplayer PRIVATE "${ffmpeg_prebuilt_SOURCE_DIR}/lib")
else()
    pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
        libavformat
        libavcodec
        libavutil
        libswresample
        libswscale
        libavfilter
    )
    target_link_libraries(aplayer PRIVATE m pthread ncurses PkgConfig::LIBAV)
endif()

file(GLOB_RECURSE TAGLIB_HEADERS "${taglib_SOURCE_DIR}/taglib/*.h")
set(TAGLIB_INCLUDE_DIRS "")
foreach(_hdr ${TAGLIB_HEADERS})
    get_filename_component(_dir ${_hdr} DIRECTORY)
    list(APPEND TAGLIB_INCLUDE_DIRS ${_dir})
endforeach()
list(REMOVE_DUPLICATES TAGLIB_INCLUDE_DIRS)

target_include_directories(aplayer PRIVATE
    ./src/include
    ./thirdparty/include
    "${ffmpeg_prebuilt_SOURCE_DIR}/include"
    "${pthread-win32_SOURCE_DIR}"
    "${libebur128_SOURCE_DIR}/ebur128"
    "${taglib_SOURCE_DIR}/bindings/c"
    "${taglib_SOURCE_DIR}"
    "${taglib_BINARY_DIR}"
    ${TAGLIB_INCLUDE_DIRS}
    "${fftw3_SOURCE_DIR}/api"
)

target_link_libraries(aplayer PRIVATE fftw3 tag tag_c portaudio ebur128)
