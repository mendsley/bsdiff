cmake_minimum_required(VERSION 3.6)
project(bsdiff)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        bsdiff.c
        bsdiff.h
        bspatch.c
        bspatch.h
        config.h)

add_executable(bsdiff ${SOURCE_FILES})