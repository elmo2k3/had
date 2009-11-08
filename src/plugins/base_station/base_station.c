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
#include <gmodule.h>

#include <had.h>
#include <config.h>
#include "base_station.h"

static gboolean serialReceive
(GIOChannel *channel, GIOCondition *condition, struct _BaseStation *base_station);

struct _BaseStation *base_station_new(char *serial_device)
{
    int fd;
	struct termios newtio;
	/* open the device */
	fd = open(serial_device, O_RDWR | O_NOCTTY );
	if (fd <0) 
	{
		return NULL;
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

	struct _BaseStation *base_station_to_return = g_new0(struct _BaseStation, 1);

    GIOChannel *serial_device_chan = g_io_channel_unix_new(fd);
    guint serial_watch = g_io_add_watch(serial_device_chan, G_IO_IN, 
		(GIOFunc)serialReceive, base_station_to_return);
	g_io_add_watch(serial_device_chan, G_IO_ERR, (GIOFunc)exit, NULL);
    g_io_channel_unref(serial_device_chan);
	base_station_to_return->serial_port_watcher = serial_watch;

	return base_station_to_return;
}
void base_station_set_callback(struct _BaseStation *tag_reader, void *callback,void *user_data)
{
	tag_reader->callback = callback;
	tag_reader->user_data = user_data;
}

gchar *base_station_last_tag(struct _BaseStation *base_station)
{
	return base_station->last_command;
}

static gboolean serialReceive
(GIOChannel *channel, GIOCondition *condition, struct _BaseStation *base_station)
{
    gchar buf[2048];
    gsize bytes_read;
    gint i;
    g_io_channel_read_chars(channel, buf, sizeof(buf), &bytes_read, NULL);
    buf[bytes_read] = '\0';

    for(i=0; i < bytes_read; i++)
    {
        if(buf[i] > 47 || buf[i] == '\n')
        {
            base_station->last_command[base_station->cmd_position++] = buf[i];
            if(base_station->last_command[base_station->cmd_position] == '\n')
            {
                base_station->last_command[base_station->cmd_position] = '\0';
                base_station->cmd_position = 0;

				if(base_station->callback)
				{
					base_station->callback(base_station, base_station->user_data);
				}
            }
        }
    }
    return TRUE;
}

/*!
 *******************************************************************************
 * send a packet to the base module via serial port
 *
 * \param *packet pointer to a certain struct
 * \param type type of the struct
 *******************************************************************************/
//void sendPacket(void *packet, int type)
//{
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
////	usleep(50000); // warten bis Packet wirklich abgeschickt wurde
////	usleep(30000); // warten bis Packet wirklich abgeschickt wurde
//}

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

//int readSerial(char *buf)
//{
//	int res;
//	res = read(fd,buf,255);
//	buf[res] = 0;
//	return res;
//}

//void setBeepOn()
//{
//	struct headPacket headP;
//	headP.address = 0x02;
//	headP.count = 1;
//	headP.command = 3;
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
//}

//void setBeepOff()
//{
//	struct headPacket headP;
//	headP.address = 0x02;
//	headP.count = 1;
//	headP.command = 4;
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
//}
//
//void setBaseLcdOn()
//{
//	struct headPacket headP;
//	headP.address = 0x02;
//	headP.count = 1;
//	headP.command = 1;
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
//}
//
//void setBaseLcdOff()
//{
//	struct headPacket headP;
//	headP.address = 0x02;
//	headP.count = 1;
//	headP.command = 2;
//	if(config.serial_activated)
//		write(fd,&headP,sizeof(headP));
//}

void open_door()
{
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

//void sendBaseLcdText(char *text)
//{
//	struct headPacket headP;
//	struct _lcd_text
//	{
//		struct headPacket headP;
//		char text[33];
//	}lcd_text;
//
//	lcd_text.headP.address = 0x02;
//	lcd_text.headP.count = 34;
//	lcd_text.headP.command = 5;
//	strncpy(lcd_text.text,text,32);
//	lcd_text.text[32] = '\0';
//	if(config.serial_activated)
//		write(fd,&lcd_text,sizeof(lcd_text));
//}

void command_received(struct _BaseStation *BaseStation, void *user_data)
{
	g_debug("command received: %s", BaseStation->last_command);
}

gchar *g_module_check_init(void)
{
	struct _BaseStation *BaseStation;

	BaseStation = base_station_new(hadConfigGetString("mod_base_station","port","/dev/ttyUSB0"));
	if(BaseStation)
		base_station_set_callback(BaseStation, command_received, NULL);
	else
	{
		g_debug("could not open serial device");
	}

	g_debug("module base_station loaded");
	return NULL;
}


