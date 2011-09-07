libdir = @libdir@/@PACKAGE@/plugins
lib_LTLIBRARIES = libmcp_game_plugin.la
libmcp_game_plugin_la_SOURCES = mcp_game.c
libmcp_game_plugin_la_CFLAGS = -g -fvisibility=hidden -m32 -Wall
libmcp_game_plugin_la_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/lib/util/include
libmcp_game_plugin_la_LDFLAGS = -release 0.1.0
LDADD = -lrt -lpthread
