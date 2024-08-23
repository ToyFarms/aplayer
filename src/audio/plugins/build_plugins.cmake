cmake_minimum_required(VERSION 3.10)

set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/audio/plugins")
file(GLOB SOURCES "${SOURCE_DIR}/*.c")

if(WIN32)
    set(OUTPUT_DIR "~/AppData/Local/aplayer/plugins")
else()
    set(OUTPUT_DIR "~/.local/share/aplayer/plugins")
endif()

foreach(SOURCE_FILE ${SOURCES})
    get_filename_component(LIB_NAME ${SOURCE_FILE} NAME_WE)
    add_library(${LIB_NAME} SHARED ${SOURCE_FILE})

    target_include_directories(${LIB_NAME} PRIVATE src/include)
    target_sources(${LIB_NAME} PRIVATE src/term/term_draw.c src/struct/ds.c src/logger.c)
    set_target_properties(${LIB_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}
    )
endforeach()
