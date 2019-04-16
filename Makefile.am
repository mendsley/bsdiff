bin_PROGRAMS = bsdiff bspatch

bsdiff_SOURCES = bsdiff.c

bspatch_SOURCES = bspatch.c

bsdiff_CFLAGS = -DBSDIFF_EXECUTABLE
bspatch_CFLAGS = -DBSPATCH_EXECUTABLE

EXTRA_DIST = bsdiff.h bspatch.h

include_HEADERS=bsdiff.h bspatch.h
lib_LTLIBRARIES=libbsdiff.la
libbsdiff_la_SOURCES=bsdiff.c bspatch.c
