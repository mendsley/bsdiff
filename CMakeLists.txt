cmake_minimum_required(VERSION 3.13)
project (
    bsdiff
    VERSION 4.3.0
    LANGUAGES C)

set(CMAKE_C_STANDARD 99)
add_compile_options("-Wall" "-Wpedantic" "-Wextra" "-g" )

find_package(BZip2)
include_directories(${BZIP2_INCLUDE_DIR})

add_library (libbsdiff bsdiff.c bspatch.c)
set_target_properties(libbsdiff PROPERTIES PREFIX "")

add_executable(bsdiff bsdiff.c)
target_compile_definitions(bsdiff PRIVATE BSDIFF_EXECUTABLE)
target_link_libraries(bsdiff ${BZIP2_LIBRARIES})

add_executable(bspatch bspatch.c)
target_compile_definitions(bspatch PRIVATE BSPATCH_EXECUTABLE)
target_link_libraries(bspatch ${BZIP2_LIBRARIES})


