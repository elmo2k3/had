LDFLAGS = -lmysqlclient
CFLAGS = -Os
main: main.o
clean: 
	rm *.o main
