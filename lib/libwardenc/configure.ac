#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([libwardenc], [0.1.0], [gonzoj@lavabit.com])
AC_CONFIG_SRCDIR([src/wardenc.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config/m4])

AM_INIT_AUTOMAKE([-Wall subdir-objects])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_PROG_LIBTOOL

if test "x$with_libwardenc" != "xno"; then
	ON_ERROR="exit 1"
fi

# Checks for programs.
AC_PROG_CC

AC_PROG_CC_C_O
AM_PROG_CC_C_O

if test "x$GCC" = "xyes"; then
	CFLAG_M32="-m32"
	CFLAG_FVISIBILITY_HIDDEN="-fvisibility=hidden"
	CPPFLAG_DEXPORT="-D_export=\"__attribute__((visibility(\\\"default\\\")))\""
fi

AM_CFLAGS="$CFLAG_M32"
AC_SUBST(AM_CFLAGS)
AC_SUBST(CFLAG_FVISIBILITY_HIDDEN)
AC_SUBST(CPPFLAG_DEXPORT)

_CFLAGS="$CFLAGS"
CFLAGS="$AM_CFLAGS $CFLAGS"

# Checks for libraries.
AC_CHECK_LIB(crypto, MD5_Init, [AC_DEFINE(HAVE_LIBCRYPTO, 1, Define to 1 if you have the `crytpo' library (-lcrypto).)], [echo "error: libcrypto is missing."; $ON_ERROR])
AC_CHECK_LIB(z, inflate, [AC_DEFINE(HAVE_LIBZ, 1, Define to 1 if you have the `z' library (-lz).)], [echo "error: libz is missing."; $ON_ERROR])
AC_CHECK_LIB(dl, dlopen, [AC_DEFINE(HAVE_LIBDL, 1, Define to 1 if you have the `dl' library (-ldl).)], [echo "warning: libdl is missing. wardendump will not build."])

CFLAGS="$_CFLAGS"

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([getcwd memset mkdir strchr strerror strrchr strstr])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	wardendump/Makefile
])

AC_OUTPUT

if test "x$with_libwardenc" != "xno"; then
	echo \
"------------------------------------------------------------------------------
configuration (libwardenc):

compiler:		${CC}
compiler flags:		[[$AM_CFLAGS]] ${CFLAGS}
preprocessor flags:	${CPPFLAGS}
linker flags:		${LDFLAGS}
host system:		${host}
install path:		${prefix}

consult config.h for further configuration information."
fi
