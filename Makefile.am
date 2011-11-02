bin_PROGRAMS = had

had_SOURCES = had.c\
	rfid_tag_reader.c \
	configfile.c \
	database.c \
	hr20.c \
	led_routines.c \
	mpd.c \
	scrobbler.c \
	sms.c \
	statefile.c \
	base_station.c \
	misc.c \
	client.c \
	client_expire.c \
	client_idle.c \
	client_new.c \
	client_read.c \
	fd_util.c \
	idle.c \
	socket_util.c \
	client_event.c \
	client_global.c \
	client_list.c \
	client_process.c \
	client_write.c \
	fifo_buffer.c \
	listen.c \
	command.c \
	tokenizer.c \
	voltageboard.c \
	led_mpd_fifo.c \
	led_text_fifo.c \
	fonts/8x8.c \
	fonts/8x12.c \
	security.c

LIBS = @glib_LIBS@ \
	@libcrypto_LIBS@ \
	@gthread_LIBS@ \
	@libmpd_LIBS@ \
	@fftw3_LIBS@ \
	@libcurl_LIBS@ \
	@EXTRA_LDFLAGS@ \
	-lmysqlclient
AM_CFLAGS = @glib_CFLAGS@ \
	@libcrypto_CFLAGS@ \
	@gthread_CFLAGS@ \
	@libmpd_CFLAGS@ \
	@fftw3_CFLAGS@ \
	@libcurl_CFLAGS@ \
	@EXTRA_CFLAGS@ \
	-std=c99

