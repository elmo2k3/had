#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>

#include "hr20.h"
#include "had.h"
#include "config.h"

static int fd;

static int initSerial(char *device) 
{
	struct termios newtio;
	

	/* open the device */
	fd = open(device, O_RDWR | O_NOCTTY );
	if (fd <0) 
	{
		return 0;
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
	newtio.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
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
	return 1;
}

static int serialCommand(char *command, char *buffer)
{
	int cmd_length = strlen(command);
	int res;

	tcflush(fd,TCIOFLUSH);

	write(fd,command,cmd_length);
	usleep(100000);	
	
	if(buffer)
	{
		res = read(fd,buffer,255);
		buffer[res] = '\0';
		return res;
	}
	return 0;
}


int hr20GetStatus(struct _hr20info *hr20info)
{
	/* example line as we receive it:
	  D: d6 10.01.09 22:19:14 M V: 54 I: 1975 S: 2000 B: 3171 Is: 00b9 X
	 */
	
	char line[255];

	initSerial(config.hr20_port);
	serialCommand("D\r", line);

	if(!line)
	{
		close(fd);
		return 0;
	}

	char *trenner = (char*)strtok(line," ");
	if(!trenner)
	{
		close(fd);
		return 0;
	}

	if(trenner[0] != 'D')
	{
		close(fd);
		return 0;
	}
	
	trenner = (char*)strtok(NULL," ");
	trenner = (char*)strtok(NULL," ");
	trenner = (char*)strtok(NULL," ");
	trenner = (char*)strtok(NULL," ");
	if(trenner[0] == 'M')
		hr20info->mode = 1;
	else if(trenner[0] == 'A')
		hr20info->mode = 2;
	trenner = (char*)strtok(NULL," ");
	trenner = (char*)strtok(NULL," ");
	if(trenner)
		hr20info->valve = atoi(trenner);

	trenner = (char*)strtok(NULL," ");
	trenner = (char*)strtok(NULL," ");
	if(trenner)
		hr20info->tempis = atoi(trenner);

	trenner = (char*)strtok(NULL," ");
	trenner = (char*)strtok(NULL," ");
	if(trenner)
		hr20info->tempset = atoi(trenner);

	trenner = (char*)strtok(NULL," ");
	trenner = (char*)strtok(NULL," ");
	if(trenner)
		hr20info->voltage = atoi(trenner);

	close(fd);

	return 1;
}

