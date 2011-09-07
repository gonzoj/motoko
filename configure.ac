#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([motoko], [0.1.0], [gonzoj@lavabit.com])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config/m4])

AM_INIT_AUTOMAKE([-Wall subdir-objects])

AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC

AM_PROG_CC_C_O

# Checks for libraries.
AC_CHECK_LIB(dl, dlopen, [], [echo "error: libdl missing." exit -1])
AC_CHECK_LIB(pthread, pthread_create, [], [echo "error: libpthread missing." exit -1])
AC_CHECK_LIB(rt, clock_gettime, [], [echo "error: librt missing." exit -1])
AC_CHECK_LIB(opts, optionGetValue, [], [echo "error: libopts missing." exit -1])
AC_CHECK_LIB(ncurses, initscr, [], [echo "error: libncurses missing." exit -1])
AC_CHECK_LIB(ssl, MD5_Init, [], [echo "error: libssl missing." exit -1])
AC_CHECK_LIB(z, inflate, [], [echo "error: libz missing." exit -1])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime getcwd gethostbyname memset mkdir select socket sqrt strchr strdup strerror strrchr strstr])

AX_COMPUTE_RELATIVE_PATHS([bindir:libdir:bl bindir:datadir:bd])
AC_SUBST(bin_to_lib, [$bl])
AC_SUBST(bin_to_data, [$bd])

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
])

AC_CONFIG_SUBDIRS([lib/libwardenc])

AC_OUTPUT
