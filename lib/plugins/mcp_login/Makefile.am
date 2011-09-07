libdir = @libdir@/@PACKAGE@/plugins
lib_LTLIBRARIES = libmcp_login_plugin.la
libmcp_login_plugin_la_SOURCES = mcp_login.c
libmcp_login_plugin_la_CFLAGS = -g -fvisibility=hidden -m32 -Wall
libmcp_login_plugin_la_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/lib/util/include
libmcp_login_plugin_la_LDFLAGS = -release 0.1.0
