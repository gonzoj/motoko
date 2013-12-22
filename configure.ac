#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([motoko], [0.1.0], [gonzoj@lavabit.com])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config/m4])

AM_INIT_AUTOMAKE([-Wall subdir-objects])

AC_USE_SYSTEM_EXTENSIONS

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_PROG_LIBTOOL

AC_ARG_WITH([libwardenc], AS_HELP_STRING([--without-libwardenc], [do not build and link libwardenc]))
AM_CONDITIONAL([WITH_LIBWARDENC], [test "x$with_libwardenc" != "xno"])
if test "x$with_libwardenc" != "xno"; then
	LADD_LIBWARDENC="\$(top_srcdir)/lib/libwardenc/src/libwardenc.la"
	LINK_WARDENC="yes"
	AC_DEFINE(USE_LIBWARDENC, 1, Define to 1 if you build and link libwardenc.)
else
	LINK_WARDENC="no"
fi

AC_SUBST(LADD_LIBWARDENC)

LT_SYS_MODULE_EXT

# Checks for programs.
AC_PROG_CC

AM_PROG_CC_C_O

if test "x$GCC" = "xyes"; then
	CFLAG_M32="-m32"
	CFLAG_FVISIBILITY_HIDDEN="-fvisibility=hidden"
	CPPFLAG_DEXPORT="-D_export=\"__attribute__((visibility(\\\"default\\\")))\""
	LDFLAG_RDYNAMIC="-rdynamic"
fi

if test "x$with_libwardenc" != "xno"; then
	AM_CFLAGS="$CFLAG_M32"
	STRING_AM_CFLAGS="[[$AM_CFLAGS]] "
fi
AM_CPPFLAGS="-I\$(top_srcdir)/lib/util/include"
AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(CFLAG_FVISIBILITY_HIDDEN)
AC_SUBST(CPPFLAG_DEXPORT)
AC_SUBST(LDFLAG_RDYNAMIC)

_CFLAGS="$CFLAGS"
CFLAGS="$AM_CFLAGS $CFLAGS"

# Checks for libraries.
AC_CHECK_LIB(dl, dlopen, [AC_DEFINE(HAVE_LIBDL, 1, Define to 1 if you have the `dl' library (-ldl).)], [echo "error: libdl missing."; exit 1])
AC_CHECK_LIB(pthread, pthread_create, [AC_DEFINE(HAVE_LIBPTHREAD, 1, Define to 1 if you have the `pthread' library (-lpthread).)], [echo "error: libpthread missing."; exit 1])
AC_CHECK_LIB(rt, clock_gettime, [AC_DEFINE(HAVE_LIBRT, 1, Define to 1 if you have the `rt' library (-lrt).) LINK_RT="yes" LADD_LIBRT="-lrt"], [LINK_RT="no"
	echo "warning: librt missing. using compatibility header."])
AC_CHECK_LIB(opts, optionGetValue, [AC_DEFINE(HAVE_LIBOPTS, 1, Define to 1 if you have the `opts' library (-lopts).)], [echo "error: libopts missing."; exit 1])
AC_CHECK_LIB(ncurses, initscr, [AC_DEFINE(HAVE_LIBNCURSES, 1, Define to 1 if you have the `ncurses' library (-lncurses).)], [echo "error: libncurses missing."; exit 1])
AC_CHECK_LIB(m, sqrt, [AC_DEFINE(HAVE_LIBM, 1, Define to 1 if you have the `m' library (-lm).)], [echo "warning: libm missing. some plugins will not build."])

CFLAGS="$_CFLAGS"

AC_SUBST(LADD_LIBRT)

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T

AC_CHECK_HEADERS([time.h], [AC_CHECK_TYPES(clockid_t, [], [], [#include <time.h>])])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([clock_gettime getcwd gethostbyname gettimeofday memset mkdir select socket sqrt strchr strdup strerror strrchr strstr setsockopt])

configdir="\$(pkgdatadir)"
plugindir="\$(pkglibdir)/plugins"
AC_SUBST(configdir)
AC_SUBST(plugindir)

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	data/Makefile
	lib/Makefile
	lib/util/Makefile
	lib/plugins/Makefile
	lib/plugins/bncs_login/Makefile
	lib/plugins/mcp_login/Makefile
	lib/plugins/mcp_game/Makefile
	lib/plugins/d2gs_chicken/Makefile
	lib/plugins/d2gs_pickit/Makefile
	lib/plugins/d2gs_pes/Makefile
	lib/plugins/d2gs_dclone/Makefile
	lib/plugins/d2gs_meph/Makefile
	lib/plugins/d2gs_pathing/Makefile
	lib/plugins/d2gs_record/Makefile
])

AC_CONFIG_SUBDIRS([lib/libwardenc])

AC_OUTPUT

echo \
"------------------------------------------------------------------------------
configuration (motoko):

compiler:		${CC}
compiler flags:		${STRING_AM_CFLAGS}${CFLAGS}
preprocessor flags:	${CPPFLAGS}
linker flags:		${LDFLAGS}
link libwardenc:	${LINK_WARDENC}
link librt:		${LINK_RT}
host system:		${host}
module extension:	`grep define\ LT_MODULE_EXT config.h | sed -e 's/^.*\("\..*"\)$/\1/'`
install path:		${prefix}

consult config.h for further configuration information."
