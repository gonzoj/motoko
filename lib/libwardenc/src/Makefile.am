include_HEADERS = wardenc.h

lib_LTLIBRARIES = libwardenc.la
libwardenc_la_SOURCES = \
	wardenc.c \
	wardenc.h \
	wcrypt.c \
	wcrypt.h \
	winapi.c \
	winapi.h \
	wmodule.c \
	wmodule.h \
	wutil.c \
	wutil.h \
	wtypes.h
libwardenc_la_CFLAGS = -g -fvisibility=hidden -m32 -Wall
libwardenc_la_CPPFLAGS = -I$(top_srcdir)/src -D_export="__attribute__((visibility(\"default\")))"
libwardenc_la_LDFLAGS = --version-info 0:0:0
