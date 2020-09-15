cmake_minimum_required(VERSION 3.13)
project (
    bsdiff
    VERSION 4.3.0
    LANGUAGES C)

# Sets global options.
set(CMAKE_C_STANDARD 99)
add_compile_options("-Wall" "-Wpedantic" "-Wextra" "-Werror")

# Includes bzip2 library.
find_package(BZip2 REQUIRED)
include_directories(${BZIP2_INCLUDE_DIR})

# Builds bsdiff libraries.
add_library (static_bsdiff bsdiff.c bspatch.c)
set_target_properties(static_bsdiff PROPERTIES OUTPUT_NAME bsdiff)
add_library (shared_bsdiff SHARED bsdiff.c bspatch.c)
set_target_properties(shared_bsdiff PROPERTIES OUTPUT_NAME bsdiff)

# Builds bsdiff.
add_executable(bsdiff bsdiff.c)
target_compile_definitions(bsdiff PRIVATE BSDIFF_EXECUTABLE)
target_link_libraries(bsdiff ${BZIP2_LIBRARIES})

#Builds bspatch.
add_executable(bspatch bspatch.c)
target_compile_definitions(bspatch PRIVATE BSPATCH_EXECUTABLE)
target_link_libraries(bspatch ${BZIP2_LIBRARIES})
