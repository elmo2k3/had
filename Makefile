# Bjoern Biesenbach <bjoern@bjoern-b.de>
# 17.8.2008

LDFLAGS += -lmysqlclient -lpthread -lmpd -Llibmpd
CFLAGS += -Os -Wall -I libmpd

OBJECTS = had.o serial.o database.o mpd.o network.o scrobbler.o config.o led_routines.o

had: $(OBJECTS)

had.o: had.c had.h serial.h database.h mpd.h network.h scrobbler.h config.h led_routines.h

serial.o: serial.c serial.h had.h

database.o: database.c database.h had.h

mpd.o: mpd.c mpd.h had.h

scrobbler.o: scrobbler.c scrobbler.h had.h

config.o: config.c config.h had.h

led_routines.o: led_routines.c led_routines.h had.h

clean: 
	rm *.o had
