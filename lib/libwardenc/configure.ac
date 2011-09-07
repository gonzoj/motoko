#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([libwardenc], [0.1.0], [gonzoj@lavabit.com])
AC_CONFIG_SRCDIR([src/wardenc.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config/m4])

AM_INIT_AUTOMAKE([-Wall])

AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC

AC_PROG_CC_C_O
AM_PROG_CC_C_O

# Checks for libraries.
AC_CHECK_LIB(ssl, MD5_Init, [], [echo "libssl is missing." exit -1])
AC_CHECK_LIB(z, inflate, [], [echo "libz is missing." exit -1])
AC_CHECK_LIB(dl, dlopen, [], [echo "libdl is missing.", exit -1])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strchr strerror strrchr strstr])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	wardendump/Makefile
])

AC_OUTPUT
