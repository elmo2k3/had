/*
 * Copyright (C) 2007-2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
 * \file	serial.c
 * \brief	functions to communicate with serial had base module
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h>

#include "serial.h"
#include "had.h"

static GIOChannel *channel = NULL;
static char serial_buf[2048];
static int buf_position;

static gboolean serialReceive
(GIOChannel *channel, GIOCondition *condition, gpointer data)
{
    char buf[2048];
    gsize bytes_read;
    gint i;
    while(g_io_channel_read_chars(channel, buf, sizeof(buf), &bytes_read, NULL)
		== G_IO_STATUS_AGAIN);
    buf[bytes_read] = '\0';
	g_debug("volume reader read %s",buf);

    for(i=0; i < bytes_read; i++)
    {
		if(buf[i] == '\n')
		{
			buf[buf_position+1] = '\0';
                        g_debug("buf_position = %d", buf_position);
			g_debug("string to work on: %s",serial_buf);
			char *divider;
			divider = strtok(serial_buf, ";");
			if(divider)
			{
				g_debug("part 1 %s", divider);
//				last_overall = atoi (divider);
			}
			divider = strtok(NULL, ";");
			if(divider)
			{
				g_debug("part 2 %s", divider);
//				beer_volume_reader->last_barrel = atoi (divider);
			}
		}
		else if(buf[i] == 10)
		{
		}
		else
		{
			serial_buf[buf_position++] = buf[i];
		}
    }
    return TRUE;
}

int initSerial(char *serial_device)
{
    int fd;
	struct termios newtio;
	/* open the device */
	//fd = open(serial_device, O_RDONLY | O_NOCTTY | O_NDELAY );
	//fd = open(serial_device, O_RDWR |O_NOCTTY | O_NDELAY );
	fd = open(serial_device, O_RDWR |O_NOCTTY | O_NDELAY | O_NONBLOCK);
	if (fd <0) 
    {
		return 0;
	}
	memset(&newtio, 0, sizeof(newtio)); /* clear struct for new port settings */
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

	buf_position = 0;
    channel = g_io_channel_unix_new(fd);
   	g_io_channel_set_buffered(channel, FALSE);
    g_io_add_watch(channel, G_IO_IN, 
		(GIOFunc)serialReceive, NULL);
	g_io_add_watch(channel, G_IO_ERR, (GIOFunc)exit, NULL);
    g_io_channel_unref(channel);

	return 1;
}

/*!
 *******************************************************************************
 * send a packet to the base module via serial port
 *
 * \param *packet pointer to a certain struct
 * \param type type of the struct
 *******************************************************************************/
void sendPacket(void *packet, int type)
{
//	struct headPacket *headP = (struct headPacket*)packet;
//	
//	if(config.serial_activated)
//	{
//		if(type == GP_PACKET)
//		{
//			headP->address = GLCD_ADDRESS;
//			headP->command = GP_PACKET;
//			headP->count = 18;
//			write(fd,packet,sizeof(glcdP));
//		}
//		else if(type == MPD_PACKET)
//		{
//			headP->address = GLCD_ADDRESS;
//			headP->command = MPD_PACKET;
//			headP->count = 32;
//			write(fd,packet,sizeof(mpdP));
//		}
//		else if(type == GRAPH_PACKET)
//		{
//			/* Very dirty! Zweistufiges Senden wegen Pufferueberlauf */
//			headP->address = GLCD_ADDRESS;
//			headP->count = 61;
//			headP->command = GRAPH_PACKET;
//			write(fd,packet,63);
//			usleep(1000000);
//
//			headP->command = GRAPH_PACKET2;
//			char *ptr = (char*)packet;
//			write(fd,ptr,3);
//			write(fd,ptr+62,60);
//		}
//		else if(type == RGB_PACKET)
//		{
//			struct _rgbPacket rgbP;
//			headP->count = 5;
//			write(fd,packet,sizeof(rgbP));
//		}		
//		else if(type == RELAIS_PACKET)
//		{
//			headP->address = 0x02;
//			headP->count = 2;
//			headP->command = 0;
//			write(fd,packet,sizeof(relaisP));
//		}
//	}
//	usleep(50000); // warten bis Packet wirklich abgeschickt wurde
//	usleep(30000); // warten bis Packet wirklich abgeschickt wurde
}

void sendRgbPacket(unsigned char address, unsigned char red, unsigned char green, unsigned char blue, unsigned char smoothness)
{
	struct _rgbPacket rgbPacket;

	memset(&rgbPacket, 0, sizeof(rgbPacket));

	rgbPacket.headP.address = address;
	rgbPacket.red = red;
	rgbPacket.green = green;
	rgbPacket.blue = blue;
	rgbPacket.smoothness = smoothness;

	sendPacket(&rgbPacket,RGB_PACKET);
}

void setCurrentRgbValues()
{
	int i;
	for(i=0;i<3;i++)
	{
		sendRgbPacket(0x10+i, hadState.rgbModuleValues[i].red,
			hadState.rgbModuleValues[i].green,
			hadState.rgbModuleValues[i].blue, 0);
	}
}

int readSerial(char *buf)
{
	int res;
//	res = read(fd,buf,255);
	buf[res] = 0;
	return res;
}

void setBeepOn()
{
	struct headPacket headP;
	headP.address = 0x02;
	headP.count = 1;
	headP.command = 3;
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
}

void setBeepOff()
{
	struct headPacket headP;
	headP.address = 0x02;
	headP.count = 1;
	headP.command = 4;
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
}

void setBaseLcdOn()
{
	struct headPacket headP;
	headP.address = 0x02;
	headP.count = 1;
	headP.command = 1;
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
}

void setBaseLcdOff()
{
	struct headPacket headP;
	headP.address = 0x02;
	headP.count = 1;
	headP.command = 2;
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
}

void open_door()
{
	verbose_printf(0,"Opening Door\n");
	relaisP.port |= 16;
	sendPacket(&relaisP, RELAIS_PACKET);
	if(hadState.beep_on_door_opened)
	{
		setBeepOn();
		sleep(1);
		setBeepOff();
	}
	else
		sleep(1);
	sleep(9);
	relaisP.port &= ~(16);
	sendPacket(&relaisP, RELAIS_PACKET);
}

void sendBaseLcdText(char *text)
{
	struct headPacket headP;
	struct _lcd_text
	{
		struct headPacket headP;
		char text[33];
	}lcd_text;

	lcd_text.headP.address = 0x02;
	lcd_text.headP.count = 34;
	lcd_text.headP.command = 5;
	strncpy(lcd_text.text,text,32);
	lcd_text.text[32] = '\0';
//	if(config.serial_activated)
//		write(fd,&lcd_text,sizeof(lcd_text));
}


