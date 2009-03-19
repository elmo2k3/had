/*
 * Copyright (C) 2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*!
* \file	hr20.c
* \brief	functions to communicate with openhr20
* \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#include "hr20.h"
#include "had.h"
#include "config.h"

static int fd;

static void hr20GetStatusLine(char *line);
static int hr20InitSerial(char *device);
static int hr20SerialCommand(char *command, char *buffer);

static int hr20InitSerial(char *device) 
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
	tcflush(fd,TCIOFLUSH);
	tcsetattr(fd,TCSANOW,&newtio);
	return 1;
}

static int hr20SerialCommand(char *command, char *buffer)
{
	int cmd_length = strlen(command);
	int res;


	write(fd,command,cmd_length);
	
	if(buffer)
	{
		res = read(fd,buffer,255);
		buffer[res] = '\0';
		return res;
	}
	return 0;
}

/*!
 ********************************************************************************
 * hr20SetTemperature
 *
 * set the wanted temperature
 *
 * \param temperature the wanted temperature multiplied with 10. only steps
 *  	of 5 are allowed
 * \returns returns 1 on success, 0 on failure
 *******************************************************************************/
int hr20SetTemperature(int temperature)
{
	char buffer[255];
	char response[255];

	if(temperature % 5) // temperature may only be XX.5°C
		return 0;

	hr20InitSerial(config.hr20_port);

	sprintf(buffer,"A%x\r", temperature/5);

	hr20SerialCommand(buffer,response);

	close(fd);
	return 1;
}

/*!
 ********************************************************************************
 * hr20SetDateAndTime
 *
 * transfer current date and time to the openhr20 device
 *
 * \returns returns 1 on success
 *******************************************************************************/
int hr20SetDateAndTime()
{
	time_t rawtime;
	struct tm *ptm;

	char send_string[255];
	
	hr20InitSerial(config.hr20_port);

	time(&rawtime);
	ptm = localtime(&rawtime);

	sprintf(send_string,"Y%02x%02x%02x\r",ptm->tm_year-100, ptm->tm_mon+1, ptm->tm_mday);
	hr20SerialCommand(send_string, 0);
	
	sprintf(send_string,"H%02x%02x%02x\r",ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	hr20SerialCommand(send_string, 0);

	close(fd);
	return 1;
}

/*!
 ********************************************************************************
 * hr20SetModeManu
 *
 * set the mode to manual control
 *******************************************************************************/
void hr20SetModeManu()
{
	hr20InitSerial(config.hr20_port);
	hr20SerialCommand("M00\r", 0);
	close(fd);
}

/*!
 ********************************************************************************
 * hr20SetModeAuto
 *
 * set the mode to automatic control
 *******************************************************************************/
void hr20SetModeAuto()
{
	hr20InitSerial(config.hr20_port);
	hr20SerialCommand("M01\r", 0);
	close(fd);
}

/*!
 ********************************************************************************
 * hr20GetStatusLine
 *
 * gets the statusline from openhr20. still can't get a better solution than
 * trying to read the line several times
 *
 * \param *line will return the statusline
 *******************************************************************************/
static void hr20GetStatusLine(char *line)
{
	int i;
	for(i=0;i<10;i++)
	{
		hr20SerialCommand("\rD\r", line);
		if(line[0] == 'D' )
			break;
		usleep(100000*i);
	}
}

int hr20GetStatus(struct _hr20info *hr20info)
{
	/* example line as we receive it:
	  D: d6 10.01.09 22:19:14 M V: 54 I: 1975 S: 2000 B: 3171 Is: 00b9 X
	 */
	
	char line[2048];
	int length;
	hr20InitSerial(config.hr20_port);
	hr20GetStatusLine(line);

	hr20SerialCommand("\r\r\r\r",0);

	length = strlen(line);
	
	if(!line)
	{
		close(fd);
		return 0;
	}
	if(length < 60 || length > 70)
	{
		verbose_printf(0,"hr20.c: length of line = %d\n",length);
		verbose_printf(0,"hr20.c: read line was %s\n",line);
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

static int hr20checkPlausibility(struct _hr20info *hr20info)
{
	if(hr20info->mode < 1 || hr20info->mode > 2)
		return 0;
	if(hr20info->tempis < 500 || hr20info->tempis > 4000)
		return 0;
	if(hr20info->tempset < 500 || hr20info->tempset > 3000)
		return 0;
	if(hr20info->valve < 0 || hr20info->valve > 100)
		return 0;
	if(hr20info->voltage < 2000 || hr20info->voltage > 4000)
		return 0;
	return 1;
}


void hr20thread()
{
	struct tm *ptm;
	struct tm time_copy;
	time_t rawtime;
	struct _hr20info hr20info;
	int decicelsius;
	int celsius;

	while(1)
	{
		hr20GetStatus(&hr20info);
		if(hr20checkPlausibility(&hr20info) && config.hr20_database_number != 0)
		{
			time(&rawtime);
			ptm = gmtime(&rawtime);
			memcpy(&time_copy, gmtime(&rawtime), sizeof(struct tm));
			celsius = hr20info.tempis / 100;
			decicelsius = (hr20info.tempis - (celsius*100))*100;
			databaseInsertTemperature(config.hr20_database_number,0, celsius, decicelsius, &time_copy);
			celsius = hr20info.tempset / 100;
			decicelsius = (hr20info.tempset - (celsius*100))*100;
			databaseInsertTemperature(config.hr20_database_number,1, celsius, decicelsius, &time_copy);
			databaseInsertTemperature(config.hr20_database_number,2, hr20info.valve, 0, &time_copy);
			celsius = hr20info.voltage / 1000;
			decicelsius = (hr20info.voltage - (celsius*1000))*10;
			databaseInsertTemperature(config.hr20_database_number,3, celsius, decicelsius, &time_copy);
			verbose_printf(10,"hr20 read successfull\n");
			sleep(300);
		}
		else
		{
			verbose_printf(10,"hr20 read unsuccessfull\n");
			sleep(1);
		}
	}
}

