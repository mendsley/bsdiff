cmake_minimum_required(VERSION 3.13)
project (
    bsdiff
    VERSION 4.3.0
    LANGUAGES C)

set(CMAKE_C_STANDARD 99)

add_library (libbsdiff bsdiff.c bspatch.c)
set(libbsdiff LIBRARY_OUTPUT_NAME libbsdiff.a)

add_executable(bsdiff bsdiff.c)
add_executable(bspatch bspatch.c)

find_package(BZip2)
include_directories(${BZIP2_INCLUDE_DIR})
target_link_libraries(bsdiff ${BZIP2_LIBRARIES})
target_link_libraries(bspatch ${BZIP2_LIBRARIES})

target_compile_definitions(bsdiff PRIVATE BSDIFF_EXECUTABLE)
target_compile_definitions(bspatch PRIVATE BSPATCH_EXECUTABLE)
