#STAGING_DIR = /home/bjoern/OpenWrt-SDK-Linux-i686-1/staging_dir_mipsel
#CC = ${STAGING_DIR}/bin/mipsel-linux-gcc
#LD = ${STAGING_DIR}/mipsel-linux-uclibc/bin/ld
#INCLUDE_DIR = ${STAGING_DIR}/usr/include
#CFLAGS = -I ${INCLUDE_DIR}
#LDFLAGS = -Xlinker -rpath-link -Xlinker ${STAGING_DIR}/usr/lib -L${STAGING_DIR}/usr/lib/ -L${STAGING_DIR}/lib -L${STAGING_DIR}/usr/lib/mysql
LDFLAGS += -lmysqlclient -lstdc++ -lm
CFLAGS += -Os

main: main.o serial.o libmpdclient.o

clean: 
	rm *.o main
