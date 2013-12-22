plugin_LTLIBRARIES = d2gs_pes_plugin.la
d2gs_pes_plugin_la_SOURCES = d2gs_pes.c
d2gs_pes_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAG_FVISIBILITY_HIDDEN)
d2gs_pes_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src
d2gs_pes_plugin_la_LDFLAGS = -module
d2gs_pes_plugin_la_LIBADD = -lpthread $(LADD_LIBRT) -lm
