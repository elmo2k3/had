CFLAGS=`pkg-config --cflags glib-2.0 gthread-2.0 libmpd libcurl` -Wall -g
LDFLAGS=`pkg-config --libs glib-2.0 gthread-2.0 libmpd libcurl` -lmysqlclient

.PHONY: clean

had: had.o rfid_tag_reader.o config.o database.o hr20.o led_routines.o mpd.o network.o scrobbler.o sms.o statefile.o base_station.o misc.o commands.o

clean:
	$(RM) *.o had
