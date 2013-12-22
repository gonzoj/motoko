plugin_LTLIBRARIES = mcp_game_plugin.la
mcp_game_plugin_la_SOURCES = mcp_game.c
mcp_game_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAG_FVISIBILITY_HIDDEN)
mcp_game_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src
mcp_game_plugin_la_LDFLAGS = -module
mcp_game_plugin_la_LIBADD = $(LADD_LIBRT) -lpthread
