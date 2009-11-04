lib_LTLIBRARIES = libconfig.la
libconfig_la_SOURCES = config.c
libconfiginclude_HEADERS = config.h
libconfigincludedir=$(includedir)/had/plugins

LIBS = @glib_LIBS@ @gmodule_LIBS@ @EXTRA_LDFLAGS@
AM_CFLAGS = @CFLAGS@ @glib_CFLAGS@ @EXTRA_CFLAGS@ -I../../
