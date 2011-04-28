/*
 * Copyright (C) 2011 Bjoern Biesenbach <bjoern@bjoern-b.de>
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

#include "had.h"
#include "hr20.h"
#include "voltageboard.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "voltageboard"

static int voltageboard_is_initiated = 0;
static int fd;

static int voltage_total;
static int amps_total;
static int power_total;
static int voltage_5v_1;
static int voltage_5v_2;

static int power_5v_1_on;
static int power_5v_2_on;
static int relais_on;
static int relais_bat_on;

static gboolean voltageboard_try_init(gpointer data);


static struct Voltageboard
{
    gchar cmd[1024];
    guint cmd_position;
    guint serial_port_watcher; /**< glib watcher */
    gchar error_string[1024]; /**< last error should be stored here (not in use yet) */
    GIOChannel *channel;
}voltageboard;

static void process_command(struct Voltageboard *voltageboard)
{
    gchar **strings;
    GError *error = NULL;
    int i=0;
    
    g_debug("received string %s",voltageboard->cmd);
    strings = g_strsplit( voltageboard->cmd, ";", 15);
    
    while(strings[i]){
        i++;
    }
    if(i == 9){        
        voltage_total = atoi(strings[0]);
        amps_total = atoi(strings[1]);
        voltage_5v_1 = atoi(strings[2]);
        voltage_5v_2 = atoi(strings[3]);
        power_total = atoi(strings[4]);

        relais_on = atoi(strings[5]);
        relais_bat_on = atoi(strings[6]);
        power_5v_1_on = atoi(strings[7]);
        power_5v_2_on = atoi(strings[8]);
    }

    g_io_channel_flush(voltageboard->channel, &error);
    if(error)
        g_error_free(error);
    g_strfreev(strings);
}

static gboolean serialReceive
(GIOChannel *channel, GIOCondition condition, struct Voltageboard *voltageboard)
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
        voltageboard_is_initiated = 0;
        g_io_channel_shutdown(channel, 0, NULL);
        close(fd);
        g_timeout_add_seconds(1, voltageboard_try_init, NULL);
        return FALSE;
    }
    buf[bytes_read] = '\0';

    for(i=0; i < bytes_read; i++)
    {
        if(buf[i] == '\r' || voltageboard->cmd_position == 1023)
        {
            voltageboard->cmd[voltageboard->cmd_position] = '\0';
            voltageboard->cmd_position = 0;
            process_command(voltageboard);
        }
        else if(buf[i] == '\n')
        {
        }
        else if(buf[i])
        {
            voltageboard->cmd[voltageboard->cmd_position++] = buf[i];
        }
    }
    return TRUE;
}

void voltageboard_init()
{
    g_timeout_add_seconds(1, voltageboard_try_init, NULL);
}

static gboolean voltageboard_try_init(gpointer data)
{
    struct termios newtio;
    GError *error = NULL;
    
    if(voltageboard_is_initiated)
        return FALSE;

    if(!config.voltageboard_activated) //try again until it gets activated
        return TRUE;
    /* open the device */
    
    fd = open(config.voltageboard_tty, O_RDWR | O_NOCTTY /*| O_NDELAY*/ | O_NONBLOCK );
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
        (GIOFunc)serialReceive, &voltageboard);
    g_io_channel_set_encoding(serial_device_chan, NULL, &error);
    g_io_channel_unref(serial_device_chan);
    voltageboard.channel = serial_device_chan;
    voltageboard.serial_port_watcher = serial_watch;

    voltageboard_is_initiated = 1;
    
    g_warning("connected");
    
    return FALSE;
}

void voltageboard_dockstar_on()
{
    char buf;
    gsize bytes_written;

    if(!config.voltageboard_activated)
        return;

    buf = 'c';
    g_io_channel_write_chars(voltageboard.channel, &buf, 1,
        &bytes_written, NULL);
    g_io_channel_flush(voltageboard.channel, NULL);
    g_usleep(1000000);

    buf = 'a';
    g_io_channel_write_chars(voltageboard.channel, &buf, 1,
        &bytes_written, NULL);
    g_io_channel_flush(voltageboard.channel, NULL);
}

static gboolean voltageboard_dockstar_post_shutdown(gpointer data)
{
    char buf;
    gsize bytes_written;

    buf = 'A';
    g_io_channel_write_chars(voltageboard.channel, &buf, 1,
        &bytes_written, NULL);
    g_io_channel_flush(voltageboard.channel, NULL);

    buf = 'C';
    g_io_channel_write_chars(voltageboard.channel, &buf, 1,
        &bytes_written, NULL);
    g_io_channel_flush(voltageboard.channel, NULL);

    return FALSE;
}

void voltageboard_dockstar_off()
{
    if(!config.voltageboard_activated)
        return;

    system("ssh dockstar-bjoern -lroot -i /etc/dropbear/id_rsa 'halt'");
    g_timeout_add_seconds(20, voltageboard_dockstar_post_shutdown, NULL);
}

int voltageboard_get_voltage_total(void){
    return voltage_total;
}

int voltageboard_get_amps_total(void){
    return amps_total;
}
int voltageboard_get_voltage_5v_1(void){
    return voltage_5v_1;
}
int voltageboard_get_voltage_5v_2(void){
    return voltage_5v_2;
}
int voltageboard_get_power_total(void){
    return power_total;
}

int voltageboard_get_relais(void){
    return relais_on;
}
int voltageboard_get_relais_bat(void){
    return relais_bat_on;
}
int voltageboard_get_power_on_5v_1(void){
    return power_5v_1_on;
}
int voltageboard_get_power_on_5v_2(void){
    return power_5v_2_on;
}


