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
libwardenc_la_CFLAGS = $(AM_CFLAGS) $(CFLAG_FVISIBILITY_HIDDEN)
libwardenc_la_CPPFLAGS = $(CPPFLAG_DEXPORT)
libwardenc_la_LDFLAGS = --version-info 0:0:0
libwardenc_la_LIBADD = -lcrypto -lz
