#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([bsdiff], [0.1])
AC_CONFIG_SRCDIR([bsdiff.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.9])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lbz2':
AC_CHECK_LIB([bz2], [BZ2_bzReadOpen])

AC_CHECK_HEADERS([fcntl.h limits.h stddef.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
