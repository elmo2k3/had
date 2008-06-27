#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "serial.h"
#include "main.h"

int fd;

int initSerial(char *device) // returns fd
{
	struct termios newtio;
	

	/* open the device */
	fd = open(device, O_RDWR | O_NOCTTY );
	if (fd <0) 
	{
		return -1;
	}

	bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */
	/* 
	BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
	CRTSCTS : output hardware flow control (only used if the cable has
		all necessary lines. See sect. 7 of Serial-HOWTO)
	CS8     : 8n1 (8bit,no parity,1 stopbit)
	CLOCAL  : local connection, no modem contol
	CREAD   : enable receiving characters
	*/
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	/*
	IGNPAR  : ignore bytes with parity errors
	ICRNL   : map CR to NL (otherwise a CR input on the other computer
		will not terminate input)
	otherwise make device raw (no other input processing)
	*/
	newtio.c_iflag = IGNPAR | ICRNL;
	/*
	Raw output.
	*/
	newtio.c_oflag = 0;
	/*
	ICANON  : enable canonical input
	disable all echo functionality, and don't send signals to calling program
	*/
	newtio.c_lflag = ICANON;
	/* 
	initialize all control characters 
	default values can be found in /usr/include/termios.h, and are given
	in the comments, but we don't need them here
	*/
	newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */ 
	newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
	newtio.c_cc[VERASE]   = 0;     /* del */
	newtio.c_cc[VKILL]    = 0;     /* @ */
	newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
	newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
	newtio.c_cc[VSWTC]    = 0;     /* '\0' */
	newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */ 
	newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
	newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
	newtio.c_cc[VEOL]     = 0;     /* '\0' */
	newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
	newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
	newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
	newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
	newtio.c_cc[VEOL2]    = 0;     /* '\0' */

	/* 
	now clean the modem line and activate the settings for the port
	*/
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&newtio);
	return 0;
}

void sendPacket(void *packet, int type)
{
	struct headPacket *headP = (struct headPacket*)packet;
	

	if(type == GP_PACKET)
	{
		headP->address = GLCD_ADDRESS;
		headP->command = GP_PACKET;
		headP->count = 17;
		write(fd,packet,sizeof(glcdP));
	}
	else if(type == MPD_PACKET)
	{
		headP->address = GLCD_ADDRESS;
		headP->command = MPD_PACKET;
		headP->count = 32;
		write(fd,packet,sizeof(mpdP));
	}
	else if(type == GRAPH_PACKET)
	{
		/* Very dirty! Zweistufiges Senden wegen Pufferueberlauf */
		headP->address = GLCD_ADDRESS;
		headP->count = 61;
		headP->command = GRAPH_PACKET;
		write(fd,packet,63);
		usleep(1000000);

		headP->command = GRAPH_PACKET2;
		char *ptr = (char*)packet;
		write(fd,ptr,3);
		write(fd,ptr+62,60);
	}
	else if(type == RGB_PACKET)
	{
		write(fd,packet,sizeof(rgbP));
	}		
	usleep(100000); // warten bis Packet wirklich abgeschickt wurde
}

int readSerial(char *buf)
{
	int res;
	res = read(fd,buf,255);
	buf[res] = 0;
	return res;
}
