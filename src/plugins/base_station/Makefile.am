lib_LTLIBRARIES = libbase_station.la
libbase_station_la_SOURCES = serial.c
libbase_stationinclude_HEADERS = serial.h
libbase_stationincludedir=$(includedir)/had/plugins

LIBS = @glib_LIBS@ @gmodule_LIBS@ @EXTRA_LDFLAGS@
AM_CFLAGS = @CFLAGS@ @glib_CFLAGS@ @EXTRA_CFLAGS@ -I../../
