lib_LTLIBRARIES = librfid_tag_reader.la
librfid_tag_reader_la_SOURCES = rfid_tag_reader.c
librfid_tag_readerinclude_HEADERS = rfid_tag_reader.h
librfid_tag_readerincludedir=$(includedir)/had/plugins

LIBS = @glib_LIBS@ @gmodule_LIBS@ @EXTRA_LDFLAGS@
AM_CFLAGS = @CFLAGS@ @glib_CFLAGS@ @EXTRA_CFLAGS@ -I../../
