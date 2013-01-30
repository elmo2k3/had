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
static void can_send(char *data);

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

static gboolean can_mpd_status_periodical(gpointer data) // every 1s
{
    char send_string[255];
    char led_status;
    struct CanNode *node;

    node = can_get_node(19); // tv and music

    if(!config.can_activated)
        return TRUE;

    led_status = 0;
    
    if(mpdGetState() == MPD_PLAYER_PLAY)
        led_status |= 1;

    if(mpdGetRandom())
        led_status |= 2;

    if(node->relais_state & 0x01) // music is on
        led_status |= 8;
    
    snprintf(send_string, sizeof(send_string),"00104%02x%02x%02x%02x",
       MSG_COMMAND_STATUS, MPD_ADDRESS, MSG_STATUS_RELAIS, led_status);
    can_send(send_string);
    
    return TRUE;
}

static gboolean can_periodical(gpointer data) // every 60s
{
    struct CanNode *node;
    float temperature;
    int i;

    // insert hr20 stuff into database
    if(config.database_insert &&
        config.can_activated)
    {
        for(i=0;i<255;i++)
        {
            node = can_get_node(i);
            if(node->time_last_active > time(NULL)-10) // node is active
            {
                if(node->hr20_state.data_valid && node->hr20_state.data_timestamp != 255)
                {
                    temperature = node->hr20_state.tempis / 100.0;
                    databasePgInsertTemperature(config.can_db_temp_is,i,&temperature,time(NULL));
                    temperature = node->hr20_state.tempset / 100.0;
                    databasePgInsertTemperature(config.can_db_temp_set,i,&temperature,time(NULL));
                    temperature = node->hr20_state.valve;
                    databasePgInsertTemperature(config.can_db_valve,i,&temperature,time(NULL));
                    temperature = node->hr20_state.voltage / 1000.0;
                    databasePgInsertTemperature(config.can_db_battery,i,&temperature,time(NULL));
                }
            }
            if(i == 18) // insert uptime from address 18 (blubb!)
            {
                temperature = node->uptime;
                temperature = temperature*0.025; // sekunden zwischen 2 blubbs
                temperature = 60 / temperature; // blubbs pro minute // DAMNIT! NEVER DEVIDE BY ZERO
                //databasePgInsertTemperature(18,18,&temperature,time(NULL));
            }
        }
    }

    return TRUE; // keep periodic
}

static void process_command(struct CanTTY *can_tty)
{
    GError *error = NULL;
    int can_cmd;
    int can_device;
    int can_length;
    int can_data[6];
    int i;
    static int blubb_times[30];
    static int blubb_pos = -1;
    int blubb_avg;
    gchar *cl;
    
    //g_debug("received string %s",can_tty->cmd);
   
    if(strlen(can_tty->cmd) < 6)
    {
        g_debug("length too short %d", strlen(can_tty->cmd));
        return;
    }
    cl = can_tty->cmd;
    can_length = strlen(cl)/2;
    can_cmd = hexToInt(cl[6])*16 + hexToInt(cl[7]);
    can_device = hexToInt(cl[8])*16 + hexToInt(cl[9]);
    can_length = hexToInt(cl[4])*16 + hexToInt(cl[5]);
    //sscanf(can_tty->cmd,"%3X%2X%2Xx%2X", &can_id, &can_cmd, &can_device, &can_length);

    //g_debug("can_device = %d",can_device);
    
    if(can_length > 8)
        return;
    for(i=0;i<can_length-2;i++)
    {
        can_data[i] = hexToInt(cl[i*2+10])*16 + hexToInt(cl[i*2+11]);
        //g_debug("data[%d] = %d",i,can_data[i]);
    }

    int uptime;
    
    switch(can_cmd)
    {
        case MSG_COMMAND_STATUS:
            switch(can_data[0]) // STATUS TYPE
            {
                case MSG_STATUS_UPTIME:
                    uptime = (can_data[1]&0x0F)*255*255*255 +
                                can_data[2]*255*255 +
                                can_data[3]*255 +
                                can_data[4];
                    if(can_device == 18) // very special: node 18 sends blubb times, not uptime
                    {
                        if(blubb_pos == -1) // in init, fill whole array with first value
                        {
                            for(i=0;i<30;i++)
                            {
                                blubb_times[i] = uptime;
                            }
                            blubb_pos = 0;
                        }

                        blubb_times[blubb_pos] = uptime;
                        if(++blubb_pos > 29)
                            blubb_pos = 0;
                        for(i=0;i<30;i++)
                        {
                            blubb_avg += blubb_times[i];
                        }
                        can_nodes[18].uptime = blubb_avg / 30;
                    }
                    else // all other nodes submit uptime
                    {
                        can_nodes[can_device].uptime = uptime;
                    }
                    can_nodes[can_device].version = (can_data[1] & 0xF0)>>4;
                    if(can_nodes[can_device].version > 0)
                        can_nodes[can_device].voltage = can_data[5];
                    else
                        can_nodes[can_device].voltage = 0;
                    break;
                case MSG_STATUS_RELAIS:
                    can_nodes[can_device].relais_state = can_data[1];
                    break;
                case MSG_STATUS_HR20_TEMPS:
                    can_nodes[can_device].hr20_state.data_valid = can_data[1] & 0x01;
                    can_nodes[can_device].hr20_state.mode = can_data[1] & 0x02;
                    can_nodes[can_device].hr20_state.window_open = can_data[1] & 0x04;
                    can_nodes[can_device].hr20_state.tempis = can_data[2] << 8;
                    can_nodes[can_device].hr20_state.tempis |= can_data[3];
                    can_nodes[can_device].hr20_state.tempset = can_data[4] << 8;
                    can_nodes[can_device].hr20_state.tempset |= can_data[5];
                    break;
                case MSG_STATUS_HR20_VALVE_VOLT:
                    can_nodes[can_device].hr20_state.data_timestamp = can_data[1];
                    can_nodes[can_device].hr20_state.valve = can_data[2];
                    can_nodes[can_device].hr20_state.voltage = can_data[3] << 8;
                    can_nodes[can_device].hr20_state.voltage |= can_data[4];
                    can_nodes[can_device].hr20_state.error_code = can_data[5];
                    break;
                default:
                    break;
            }
            break;
        case MSG_COMMAND_RELAIS:
            if(can_device == MPD_ADDRESS)
            {
                g_debug("received mpd command");
                switch(can_data[0])
                {
                    case 1:
                        mpdPause();
                        break;
                    case 2:
                        mpdToggleRandom();
                        break;
                    case 4:
                        mpdPlayNumber(1);
                        break;
                    case 8: // switch on switch and music. switch off music
                        if(can_data[1] == 1)
                        {
                            can_set_relais(18, 1, 1); // switch on
                            can_set_relais(19, 1, 1); // music on
                        }
                        else
                        {
                            can_set_relais(19, 1, 0); // music off
                        }
                        break;
                    case 16:
                        mpdNext();
                        g_debug("mpdNext()");
                        break;
                    case 32:
                        mpdPrev();
                        break;
                }
            }
            break;
    }

    can_nodes[can_device].time_last_active = time(NULL);

    g_io_channel_flush(can_tty->channel, &error);
    if(error)
        g_error_free(error);
}

void canHandsOffDevice()
{
    can_is_initiated = 0;
    g_io_channel_shutdown(can_tty.channel, 0, NULL);
    close(fd);
}

void canHandsOnDevice()
{
    g_timeout_add_seconds(1, can_try_init, NULL);
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
    g_timeout_add_seconds(60, can_periodical, NULL);
    g_timeout_add_seconds(1, can_mpd_status_periodical, NULL);
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

void can_set_temperature(int address, int temperature)
{
    char send_string[255];

    if(temperature % 5)
        return;

    if(temperature < 50 || temperature > 300)
        return;

    snprintf(send_string, sizeof(send_string),"00103%02x%02x%02x",
        MSG_COMMAND_HR20_SET_T, address, temperature/5);
    can_send(send_string);
}

void can_set_date(int address)
{
    time_t rawtime;
    struct tm *ptm;
    char send_string[255];

    time(&rawtime);
    ptm = localtime(&rawtime);

    snprintf(send_string, sizeof(send_string),"00105%02x%02x%02x%02x%02x",
        MSG_COMMAND_HR20_SET_TIME, address,ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    can_send(send_string);
    snprintf(send_string, sizeof(send_string),"00105%02x%02x%02x%02x%02x",
        MSG_COMMAND_HR20_SET_DATE, address, ptm->tm_year-100, ptm->tm_mon+1, ptm->tm_mday);
    can_send(send_string);
}

void can_set_mode_manu(int address)
{
    char send_string[255];

    snprintf(send_string, sizeof(send_string),"00102%02x%02x",
        MSG_COMMAND_HR20_SET_MODE_MANU, address);
    can_send(send_string);
}

void can_set_mode_auto(int address)
{
    char send_string[255];

    snprintf(send_string, sizeof(send_string),"00102%02x%02x",
        MSG_COMMAND_HR20_SET_MODE_AUTO, address);
    can_send(send_string);
}

void can_set_relais(int address, int relais, int state)
{
    char send_string[255];

    snprintf(send_string, sizeof(send_string),"00104%02x%02x%02x%02x",
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


