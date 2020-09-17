cmake_minimum_required(VERSION 3.13)
project (
    bsdiff
    VERSION 4.3.0
    LANGUAGES C)

# Sets global options.
set(CMAKE_C_STANDARD 99)
if (MSVC)
	string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	add_compile_definitions("_CRT_SECURE_NO_WARNINGS" "_CRT_NONSTDC_NO_WARNINGS" "stat64=_stat64" "fstat64=_fstat64")
else()
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og")
    add_compile_options("-Wall" "-Wpedantic" "-Wextra" "-Werror" "-Wno-unused-parameter")
    add_compile_definitions("_LARGEFILE64_SOURCE")
endif()

# Includes bzip2 library.
find_package(BZip2)

# Builds bsdiff libraries.
add_library (static_bsdiff bsdiff.c bspatch.c)
set_target_properties(static_bsdiff PROPERTIES OUTPUT_NAME bsdiff)
if (!MSVC)
    add_library (shared_bsdiff SHARED bsdiff.c bspatch.c)
    set_target_properties(shared_bsdiff PROPERTIES OUTPUT_NAME bsdiff)
endif()

if (BZIP2_FOUND)
    # Builds bsdiff.
    add_executable(bsdiff bsdiff.c)
    target_compile_definitions(bsdiff PRIVATE "BSDIFF_EXECUTABLE")
    target_include_directories(bsdiff PRIVATE ${BZIP2_INCLUDE_DIR})
    target_link_libraries(bsdiff ${BZIP2_LIBRARIES})

    #Builds bspatch.
    add_executable(bspatch bspatch.c)
    target_compile_definitions(bspatch PRIVATE "BSPATCH_EXECUTABLE")
    target_include_directories(bspatch PRIVATE ${BZIP2_INCLUDE_DIR})
    target_link_libraries(bspatch ${BZIP2_LIBRARIES})
endif()
