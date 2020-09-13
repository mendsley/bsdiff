bin_PROGRAMS    = bsdiff bspatch

bsdiff_SOURCES  = bsdiff.c
bsdiff_CFLAGS   = -DBSDIFF_EXECUTABLE

bspatch_SOURCES = bspatch.c
bspatch_CFLAGS  = -DBSPATCH_EXECUTABLE

lib_LTLIBRARIES      = libbsdiff.la
libbsdiff_la_SOURCES = bsdiff.c bspatch.c
