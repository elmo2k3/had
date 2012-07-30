/*
 * Copyright (C) 2009-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
#include <time.h>
#include <unistd.h>
#include <stdint.h>

#include "can_protocol.h"
#include "had.h"
#include "database.h"
#include "can.h"
#include "mpd.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "can"

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR (SECONDS_PER_MINUTE*60)
#define SECONDS_PER_DAY (24*SECONDS_PER_HOUR)

static int can_is_initiated = 0;
static int fd;

static gboolean can_try_init(gpointer data);

static struct CanNode can_nodes[255];

static struct CanTTY
{
    gchar cmd[1024];
    guint cmd_position;
    guint serial_port_watcher; /**< glib watcher */
    gchar error_string[1024]; /**< last error should be stored here (not in use yet) */
    GIOChannel *channel;
}can_tty;


struct CanNode *can_get_node(int address)
{
    if(address >= 0 && address < 256)
        return &can_nodes[address];
    else
        return NULL;
}

void process_can_tty(gchar **strings, int argc)
{
    int command;
    int rawtime;
    
    g_debug("Processing can_tty packet");
    
    rawtime = time(NULL);
}

static int hexToInt(char c)
{
    if (c >= '0' && c <= '9') return      c - '0';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    return -1;
}

static void process_command(struct CanTTY *can_tty)
{
    GError *error = NULL;
    int can_cmd;
    int can_device;
    int can_length;
    int can_data[6];
    int i;
    gchar *cl;
    
    //g_debug("received string %s",can_tty->cmd);
   
    if(strlen(can_tty->cmd) < 6)
    {
        g_debug("length too short %d", strlen(can_tty->cmd));
        return;
    }
    cl = can_tty->cmd;
    can_length = strlen(cl)/2;
    can_cmd = hexToInt(cl[0])*16 + hexToInt(cl[1]);
    can_device = hexToInt(cl[2])*16 + hexToInt(cl[3]);
    //sscanf(can_tty->cmd,"%3X%2X%2Xx%2X", &can_id, &can_cmd, &can_device, &can_length);

    //g_debug("can_device = %d",can_device);
    
    for(i=0;i<can_length-2;i++)
    {
        can_data[i] = hexToInt(cl[i*2+4])*16 + hexToInt(cl[i*2+5]);
        //g_debug("data[%d] = %d",i,can_data[i]);
    }

    int uptime;
    
    switch(can_cmd)
    {
        case MSG_COMMAND_STATUS:
            switch(can_data[0]) // STATUS TYPE
            {
                case MSG_STATUS_UPTIME:
                    uptime = can_data[1]*255*255*255 +
                                can_data[2]*255*255 +
                                can_data[3]*255 +
                                can_data[4];
                    can_nodes[can_device].uptime = uptime;
                    break;
                case MSG_STATUS_RELAIS:
                    can_nodes[can_device].relais_state = can_data[1];
                default:
                    break;
            }
            break;
        case MSG_COMMAND_MPD:
            g_debug("received mpd command");
            switch(can_data[0])
            {
                case MSG_MPD_PREV:
                    mpdPrev();
                    break;
                case MSG_MPD_NEXT:
                    mpdNext();
                    break;
                case MSG_MPD_RANDOM:
                    mpdToggleRandom();
                    break;
                case MSG_MPD_PLAY:
                    mpdPlay();
                    break;
                case MSG_MPD_PAUSE:
                    mpdPause();
                    break;
            }
            break;
    }

    can_nodes[can_device].time_last_active = time(NULL);

    g_io_channel_flush(can_tty->channel, &error);
    if(error)
        g_error_free(error);
}

static gboolean canSerialReceive
(GIOChannel *channel, GIOCondition condition, struct CanTTY *can_tty)
{
    gchar buf[2048];
    gsize bytes_read;
    GError *error = NULL;
    GIOStatus status;
    gint i;
    
    status = g_io_channel_read_chars(channel, buf, sizeof(buf), &bytes_read, &error);
    if( status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN)
    {
        g_warning("removed");
        can_is_initiated = 0;
        g_io_channel_shutdown(channel, 0, NULL);
        close(fd);
        g_timeout_add_seconds(1, can_try_init, NULL);
        return FALSE;
    }
    buf[bytes_read] = '\0';

    for(i=0; i < bytes_read; i++)
    {
        if(buf[i] == '\r' || can_tty->cmd_position == 1023)
        {
    //      g_debug("%lld %s",time(NULL), can_tty->cmd);
            can_tty->cmd[can_tty->cmd_position] = '\0';
            can_tty->cmd_position = 0;
            process_command(can_tty);
        }
        else if(buf[i] == '\n')
        {
        }
        else if(buf[i])
        {
            can_tty->cmd[can_tty->cmd_position++] = buf[i];
        }
    }
    return TRUE;
}

void can_init()
{
    memset(can_nodes,0, sizeof(can_nodes));
    g_debug("init");
    g_timeout_add_seconds(1, can_try_init, NULL);
}

void can_set_relais(int address, int relais, int state)
{
    char send_string[255];

    snprintf(send_string, sizeof(send_string),"001%02x%02x04%02x%02x",
        MSG_COMMAND_RELAIS, address, relais, state);
    can_send(send_string);
}

void can_toggle_relais(int address, int relais)
{
    if(can_nodes[address].relais_state & relais)
        can_set_relais(address, relais, 0);
    else
        can_set_relais(address, relais, 1);
}

void can_send(char *data)
{
    //char data[20];
    char esc = 27;
    gsize bytes_written;
    GError *error=NULL;

    g_io_channel_write_chars(can_tty.channel, &esc, 1,
        &bytes_written, &error);
    g_io_channel_write_chars(can_tty.channel, data, strlen(data),
        &bytes_written, &error);
    g_io_channel_flush(can_tty.channel, NULL);
}

static gboolean can_try_init(gpointer data)
{
    struct termios newtio;
    GError *error = NULL;
    
    if(can_is_initiated)
        return FALSE;

    if(!config.can_activated) //try again until it gets activated
        return TRUE;
    /* open the device */
    fd = open(config.can_tty, O_RDWR | O_NOCTTY /*| O_NDELAY*/ | O_NONBLOCK );
    if (fd <0) 
    {
        return TRUE;
    }

    memset(&newtio, 0, sizeof(newtio)); /* clear struct for new port settings */
    newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = (ICANON);
    newtio.c_oflag = 0;
    tcflush(fd, TCIFLUSH);
    if(tcsetattr(fd,TCSANOW,&newtio) < 0)
    {
        return TRUE;
    }
    
    GIOChannel *serial_device_chan = g_io_channel_unix_new(fd);
    guint serial_watch = g_io_add_watch(serial_device_chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
        (GIOFunc)canSerialReceive, &can_tty);
    g_io_channel_set_encoding(serial_device_chan, NULL, &error);
    g_io_channel_unref(serial_device_chan);
    can_tty.channel = serial_device_chan;
    can_tty.serial_port_watcher = serial_watch;

    can_is_initiated = 1;
    
    g_warning("connected");
    
    return FALSE;
}


