#STAGING_DIR = /home/bjoern/OpenWrt-SDK-Linux-i686-1/staging_dir_mipsel
#CC = ${STAGING_DIR}/bin/mipsel-linux-gcc
#LD = ${STAGING_DIR}/mipsel-linux-uclibc/bin/ld
#INCLUDE_DIR = ${STAGING_DIR}/usr/include
#CFLAGS = -I ${INCLUDE_DIR}
#LDFLAGS = -Xlinker -rpath-link -Xlinker ${STAGING_DIR}/usr/lib -L${STAGING_DIR}/usr/lib/ -L${STAGING_DIR}/lib -L${STAGING_DIR}/usr/lib/mysql
LDFLAGS += -lmysqlclient -lpthread -lmpd -Llibmpd-0.14.0/src
CFLAGS += -Os -Wall -I libmpd-0.14.0/src

main: main.o serial.o database.o mpd.o network.o

clean: 
	rm *.o main
