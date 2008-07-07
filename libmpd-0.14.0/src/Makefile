STAGING_DIR = /home/bjoern/OpenWrt-SDK-Linux-i686-1/staging_dir_mipsel
CC = ${STAGING_DIR}/bin/mipsel-linux-gcc
LD = ${STAGING_DIR}/mipsel-linux-uclibc/bin/ld
INCLUDE_DIR = ${STAGING_DIR}/usr/include 
CFLAGS = -I ${INCLUDE_DIR} -I ../ -I $(STAGING_DIR)/usr/include/glib-1.2/ -I$(STAGING_DIR)/usr/lib/glib/include/
LDFLAGS = -Xlinker -rpath-link -Xlinker ${STAGING_DIR}/usr/lib -L${STAGING_DIR}/usr/lib/ -L${STAGING_DIR}/lib -L${STAGING_DIR}/usr/lib/mysql

SRCS=debug_printf.o libmpd.o libmpdclient.o libmpd-database.o libmpd-player.o libmpd-playlist.o libmpd-status.o libmpd-strfsong.o

libmpd: $(SRCS)
	$(CC) -shared -o libmpd.so $?

clean: 
	rm *.o main
