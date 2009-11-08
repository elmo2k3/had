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
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

#include "rfid_tag_reader.h"

static gboolean rfid_timeout
(struct RfidTagReader *rfid_tag_reader)
{
	rfid_tag_reader->last_tagid[0] = '\0';
	rfid_tag_reader->timeout_active = 0;
	return FALSE;
}

void rfid_tag_reader_set_callback(struct RfidTagReader *tag_reader, void *callback,void *user_data)
{
	tag_reader->callback = callback;
	tag_reader->user_data = user_data;
}

gchar *rfid_tag_reader_last_tag(struct RfidTagReader *tag_reader)
{
	return tag_reader->tagid;
}

static gboolean serialReceive
(GIOChannel *channel, GIOCondition *condition, struct RfidTagReader *rfid_tag_reader)
{
    gchar buf[2048];
    gsize bytes_read;
    gint i;
    g_io_channel_read_chars(channel, buf, sizeof(buf), &bytes_read, NULL);
    buf[bytes_read] = '\0';

    for(i=0; i < bytes_read; i++)
    {
        if(buf[i] > 47 && buf[i] < 91) // we want to parse hex data only
        {
            rfid_tag_reader->tagid[rfid_tag_reader->tagposition++] = buf[i];
            if(rfid_tag_reader->tagposition >= 10)
            {
                rfid_tag_reader->tagid[10] = '\0';
                rfid_tag_reader->tagposition = 0;

				/* check if old and new tagid are not equal */
				if(g_strcmp0(rfid_tag_reader->tagid, rfid_tag_reader->last_tagid))
				{
					if(rfid_tag_reader->callback)
					{
						rfid_tag_reader->callback(rfid_tag_reader, rfid_tag_reader->user_data);
					}
                	//fprintf(stdout,"tagid =  %s\n",rfid_tag_reader->tagid);
					g_strlcpy(rfid_tag_reader->last_tagid, 
						rfid_tag_reader->tagid,
						sizeof(rfid_tag_reader->last_tagid));
				}
				else // they were equal
				{
					if(!rfid_tag_reader->timeout_active)
					{
						rfid_tag_reader->timeout_active = 1;
						g_timeout_add_seconds(5, (GSourceFunc)
							rfid_timeout, rfid_tag_reader);
					}
				}
            }
        }
    }
    return TRUE;
}

struct RfidTagReader *rfid_tag_reader_new(char *serial_device)
{
    int fd;
	struct termios newtio;
	/* open the device */
	fd = open(serial_device, O_RDONLY | O_NOCTTY | O_NDELAY );
	if (fd <0) 
    {
		return NULL;
	}

	memset(&newtio, 0, sizeof(newtio)); /* clear struct for new port settings */
	newtio.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = (ICANON);
	tcflush(fd, TCIFLUSH);
	if(tcsetattr(fd,TCSANOW,&newtio) < 0)
	{
		return NULL;
	}
    
	struct RfidTagReader *rfid_tag_reader_to_return = g_new0(struct RfidTagReader, 1);

    GIOChannel *serial_device_chan = g_io_channel_unix_new(fd);
    guint serial_watch = g_io_add_watch(serial_device_chan, G_IO_IN, 
		(GIOFunc)serialReceive, rfid_tag_reader_to_return);
	g_io_add_watch(serial_device_chan, G_IO_ERR, (GIOFunc)exit, NULL);
    g_io_channel_unref(serial_device_chan);
	rfid_tag_reader_to_return->serial_port_watcher = serial_watch;

	return rfid_tag_reader_to_return;
}

void tag_read(struct RfidTagReader *tag_reader, void *user_data)
{
	g_debug("tag read: %s",rfid_tag_reader_last_tag(tag_reader));
}

gchar *g_module_check_init(void)
{
	g_debug("module rfid_tag_reader loaded");
	gchar *port = hadConfigGetString("mod_rfid_tag_reader","port","/dev/ttyUSB1");
	struct RfidTagReader *tag_reader = rfid_tag_reader_new(port);
	//g_free(port);
	if(!tag_reader)
	{
		g_debug("rfid_tag_reader.c: could not init tagreader");
	}
	else
		rfid_tag_reader_set_callback(tag_reader, tag_read, NULL);
	return NULL;
}

