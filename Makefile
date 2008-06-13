LDFLAGS = -lmysqlclient
CFLAGS = -Os
main: main.o serial.o
clean: 
	rm *.o main
