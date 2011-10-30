CFLAGS=`pkg-config --cflags glib-2.0 gthread-2.0 libmpd libcurl fftw3 libcrypto` -Wall -g -std=gnu99
LDFLAGS=`pkg-config --libs glib-2.0 gthread-2.0 libmpd libcurl fftw3 libcrypto ` -lmysqlclient

.PHONY: clean

had: had.o \
	rfid_tag_reader.o \
	config.o \
	database.o \
	hr20.o \
	led_routines.o \
	mpd.o \
	scrobbler.o \
	sms.o \
	statefile.o \
	base_station.o \
	misc.o \
	command.o \
	client.o \
	client_expire.o \
	client_idle.o \
	client_new.o \
	client_read.o \
	fd_util.o \
	idle.o \
	socket_util.o \
	client_event.o \
	client_global.o \
	client_list.o \
	client_process.o \
	client_write.o \
	fifo_buffer.o \
	listen.o \
	command.o \
	tokenizer.o \
	voltageboard.o \
	led_mpd_fifo.o \
	security.o

*.o:	had.h

clean:
	$(RM) *.o had
