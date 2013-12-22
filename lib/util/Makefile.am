utildir = $(pkgincludedir)/util
util_HEADERS = \
	include/util/compat.h \
	include/util/config.h \
	include/util/curses.h \
	include/util/error.h \
	include/util/file.h \
	include/util/list.h \
	include/util/net.h \
	include/util/string.h \
	include/util/system.h \
	include/util/types.h

noinst_LTLIBRARIES = libutil.la
libutil_la_SOURCES = \
	compat.c \
	include/util/compat.h \
	config.c \
	include/util/config.h \
	curses.c \
	include/util/curses.h \
	error.c \
	include/util/error.h \
	file.c \
	include/util/file.h \
	list.c \
	include/util/list.h \
	net.c \
	include/util/net.h \
	string.c \
	include/util/string.h \
	system.c \
	include/util/system.h \
	include/util/types.h
libutil_la_LDFLAGS = -static
libutil_la_LIBADD = -lopts -lncurses $(LADD_LIBRT)
