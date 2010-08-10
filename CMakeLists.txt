project(had)

set(APPLICATION_NAME "had")
set(APPLICATION_VERSION "0.1")
set(SRCS had.c rfid_tag_reader.c config.c database.c hr20.c led_routines.c mpd.c scrobbler.c sms.c statefile.c base_station.c misc.c client.c client_expire.c client_idle.c client_new.c client_read.c fd_util.c idle.c socket_util.c client_event.c client_global.c client_list.c client_process.c client_write.c fifo_buffer.c listen.c command.c tokenizer.c security.c)

cmake_minimum_required(VERSION 2.6)

include_directories(libmpd)
link_directories(libmpd)
add_executable(had ${SRCS})
target_link_libraries(had mysqlclient pthread mpd crypto curl ssl z glib-2.0 intl iconv gthread-2.0 fftw3)
