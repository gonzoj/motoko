nobase_pkginclude_HEADERS = \
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
motoko_CFLAGS = $(AM_CFLAGS) $(CFLAG_FVISIBILITY_HIDDEN)
motoko_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/lib/libwardenc/src $(CPPFLAG_DEXPORT) -DPLUGINDIR=\"$(plugindir)\" -DCONFIGDIR=\"$(configdir)\"
motoko_LDFLAGS = $(LDFLAG_RDYNAMIC)
LDADD = -ldl -lpthread -lncurses $(top_srcdir)/lib/util/libutil.la $(LADD_LIBWARDENC)
