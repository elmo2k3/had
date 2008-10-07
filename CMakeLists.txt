project(had)

set(APPLICATION_NAME "had")
set(APPLICATION_VERSION "0.1")
set(SRCS had.c serial.c database.c mpd.c network.c scrobbler.c config.c led_routines.c)

cmake_minimum_required(VERSION 2.6)

include_directories(libmpd)
link_directories(libmpd)
add_executable(had ${SRCS})
set(CMAKE_C_FLAGS -Wall)
target_link_libraries(had mysqlclient pthread mpd)
