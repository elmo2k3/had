# Bjoern Biesenbach <bjoern@bjoern-b.de>
# 17.8.2008

#STAGING_DIR = /home/bjoern/OpenWrt-SDK-Linux-i686-1/staging_dir_mipsel
#CC = ${STAGING_DIR}/bin/mipsel-linux-gcc
#LD = ${STAGING_DIR}/mipsel-linux-uclibc/bin/ld
#INCLUDE_DIR = ${STAGING_DIR}/usr/include
#CFLAGS = -I ${INCLUDE_DIR}
#LDFLAGS = -Xlinker -rpath-link -Xlinker ${STAGING_DIR}/usr/lib -L${STAGING_DIR}/usr/lib/ -L${STAGING_DIR}/lib -L${STAGING_DIR}/usr/lib/mysql
LDFLAGS += -lmysqlclient -lpthread -lmpd -Llibmpd
CFLAGS += -Os -Wall -I libmpd -g

had: had.o serial.o database.o mpd.o network.o scrobbler.o config.o

clean: 
	rm *.o had
