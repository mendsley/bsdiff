# Initializes autotools.
AC_INIT([bsdiff], [4.3.1])
AC_CONFIG_SRCDIR([bsdiff.c])
AM_INIT_AUTOMAKE([foreign])
LT_INIT

# Checks C compiler, headers and dependencies.
AC_PROG_CC
AC_CHECK_HEADERS([limits.h stddef.h stdint.h stdlib.h string.h])
AC_CHECK_LIB([bz2], [BZ2_bzReadOpen])

# Checks functions and types.
AC_FUNC_MALLOC
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T

# Creates Makefile.
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
