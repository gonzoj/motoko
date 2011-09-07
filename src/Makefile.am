SUBDIRS = ../lib/libwardenc

includedir = @includedir@/@PACKAGE@
nobase_include_HEADERS = \
	bncs.h \
	clientman.h \
	d2gs.h \
	gui.h \
	hash.h \
	internal.h \
	mcp.h \
	module.h \
	moduleman.h \
	packet.h \
	settings.h \
	data/item_codes.h \
	data/monster_fields.h \
	data/monster_names.h \
	data/skills.h \
	data/super_uniques.h
	
bin_PROGRAMS = motoko
motoko_SOURCES = \
	bncs.c \
	bncs.h \
	clientman.c \
	clientman.h \
	d2gs.c \
	d2gs.h \
	gui.c \
	gui.h \
	hash.c \
	hash.h \
	internal.c \
	internal.h \
	main.c \
	mcp.c \
	mcp.h \
	module.h \
	moduleman.c \
	moduleman.h \
	packet.c \
	packet.h \
	settings.h \
	data/item_codes.h \
	data/monster_fields.h \
	data/monster_names.h \
	data/skills.h \
	data/super_uniques.h
motoko_CFLAGS = -g -fvisibility=hidden -m32 -Wall
motoko_CPPFLAGS = -DBINDIR_TO_LIBDIR=\"@bin_to_lib@\" -DBINDIR_TO_DATADIR=\"@bin_to_data@\" -I$(top_srcdir)/src -I$(top_srcdir)/lib/libwardenc/src -I$(top_srcdir)/lib/util/include -D_export="__attribute__((visibility(\"default\")))"
motoko_LDFLAGS = -rdynamic 
LDADD = $(top_srcdir)/lib/libwardenc/src/libwardenc.la $(top_srcdir)/lib/util/libutil.la
