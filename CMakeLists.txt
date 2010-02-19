project(had)

set(APPLICATION_NAME "had")
set(APPLICATION_VERSION "0.1")
set(SRCS had.c base_station.c database.c mpd.c network.c scrobbler.c config.c led_routines.c statefile.c sms.c hr20.c rfid_tag_reader.c commands.c misc.c)

cmake_minimum_required(VERSION 2.6)

include_directories(libmpd)
link_directories(libmpd)
add_executable(had ${SRCS})
target_link_libraries(had mysqlclient pthread mpd crypto curl ssl z glib-2.0 intl iconv gthread-2.0)
