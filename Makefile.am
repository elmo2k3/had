bin_PROGRAMS = had

had_SOURCES = had.c\
	configfile.c \
	database_mysql.c \
	database_pgsql.c \
	led_routines.c \
	mpd.c \
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
	led_mpd_fifo.c \
	led_text_fifo.c \
	fonts/8x8.c \
	fonts/8x12.c \
	voltageboard.c \
	can.c \
	can_configfile.c

LIBS = @glib_LIBS@ \
	@gthread_LIBS@ \
	@libmpd_LIBS@ \
	@fftw3_LIBS@ \
	@libcurl_LIBS@ \
	@MYSQL_LIBS@ \
	@POSTGRESQL_LDFLAGS@ \
	@EXTRA_LDFLAGS@ \
	-lm
AM_CFLAGS = @glib_CFLAGS@ \
	@gthread_CFLAGS@ \
	@libmpd_CFLAGS@ \
	@fftw3_CFLAGS@ \
	@libcurl_CFLAGS@ \
	@MYSQL_CFLAGS@ \
	@POSTGRESQL_CFLAGS@ \
	@EXTRA_CFLAGS@ \
	-std=gnu99
AM_LDFLAGS = \
    @EXTRA_LDFLAGS@\
    @LDFLAGS@
