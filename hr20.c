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

/*!
* \file hr20.c
* \brief    functions to communicate with openhr20
* \author   Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#include <termios.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h>

#include "hr20.h"
#include "had.h"
#include "config.h"
#include "database.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "hr20"

static void hr20GetStatusLine(char *line);
static int hr20InitSerial(char *device);
static int16_t hexCharToInt(char c);
static int16_t hr20GetAutoTemperature(int slot);
static gboolean serialReceive (GIOChannel *channel, GIOCondition condition, gpointer data);
static int fd;

static int hr20_is_initiated = 0; 

/** struct holding complete status information from the hr20 device */
struct _hr20status
{
    int16_t tempis; /**< current temperature */
    int16_t tempset; /**< user set temperature */
    int8_t valve; /**< how open is the valve? in percent */
    int16_t voltage; /**< voltage of the batteries */
    int8_t mode; /**< mode, 1 for manual, 2 for automatic control */
    int16_t auto_temperature[4];
    
    time_t time_last;
    time_t time_last_db_update;

    char cmd[1024];
    int cmd_position;
    GIOChannel *channel;
}hr20status;

static gboolean hr20TryInit(gpointer data)
{
    struct termios newtio;

    if(!config.hr20_activated) //try again until it gets activated by user
        return TRUE;
    if(hr20_is_initiated)
        return FALSE;

    memset(&hr20status, 0, sizeof(hr20status));
    
    /* open the device */
    fd = open(config.hr20_port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd <0) 
    {
        return TRUE;
    }
    
    memset(&newtio,0, sizeof(newtio)); /* clear struct for new port settings */
    newtio.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = (ICANON);
    newtio.c_oflag = 0;
    tcflush(fd, TCIFLUSH);
    if(tcsetattr(fd,TCSANOW,&newtio) < 0)
    {
        return TRUE;
    }

    GIOChannel *serial_device_chan = g_io_channel_unix_new(fd);
    g_io_add_watch(serial_device_chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
        (GIOFunc)serialReceive, NULL);
    g_io_channel_unref(serial_device_chan);
    hr20status.channel = serial_device_chan;

    hr20_is_initiated = 1;
    
    g_warning("connected");
    
    return FALSE;
}

void hr20Init()
{
    if(!hr20_is_initiated)
    {
        g_timeout_add_seconds(1, hr20TryInit, NULL);
        g_timeout_add_seconds(300, hr20update, NULL);
    }
}

static int hr20checkPlausibility(struct _hr20status *hr20status)
{
    if(hr20status->mode < 1 || hr20status->mode > 2)
        return 0;
    if(hr20status->tempis < 500 || hr20status->tempis > 4000)
        return 0;
    if(hr20status->tempset < 500 || hr20status->tempset > 3000)
        return 0;
    if(hr20status->valve < 0 || hr20status->valve > 100)
        return 0;
    if(hr20status->voltage < 2000 || hr20status->voltage > 4000)
        return 0;
//    for(i=0;i<4;i++)
//    {
//        if(hr20status->auto_temperature[i] < 500 || hr20status->auto_temperature[i] > 3000)
//            return 0;
//    }
    return 1;
}

void parse_hr20()
{
    /* example line as we receive it:
      D: d6 10.01.09 22:19:14 M V: 54 I: 1975 S: 2000 B: 3171 Is: 00b9 X
     */
    char *trenner;
    char *line = hr20status.cmd;
    struct _hr20status hr20status_temp;

    if(strlen(line) < 4)
    {
        return;
    }

    trenner = (char*)strtok(line," ");
    if(!trenner)
    {
        return;
    }

    if(trenner[0] != 'D')
    {
        return;
    }
    
    trenner = (char*)strtok(NULL," ");
    trenner = (char*)strtok(NULL," ");
    trenner = (char*)strtok(NULL," ");
    trenner = (char*)strtok(NULL," ");
    if(trenner[0] == 'M')
        hr20status_temp.mode = 1;
    else if(trenner[0] == 'A')
        hr20status_temp.mode = 2;
    trenner = (char*)strtok(NULL," ");
    trenner = (char*)strtok(NULL," ");
    if(trenner)
        hr20status_temp.valve = atoi(trenner);

    trenner = (char*)strtok(NULL," ");
    trenner = (char*)strtok(NULL," ");
    if(trenner)
        hr20status_temp.tempis = atoi(trenner);

    trenner = (char*)strtok(NULL," ");
    trenner = (char*)strtok(NULL," ");
    if(trenner)
        hr20status_temp.tempset = atoi(trenner);

    trenner = (char*)strtok(NULL," ");
    trenner = (char*)strtok(NULL," ");
    if(trenner)
        hr20status_temp.voltage = atoi(trenner);
//    for(i=0;i<4;i++)
//    {
//        hr20status.auto_temperature[i] = hr20GetAutoTemperature(i);
//    }
    if(hr20checkPlausibility(&hr20status_temp))
    {
        hr20status.mode = hr20status_temp.mode;
        hr20status.valve = hr20status_temp.valve;
        hr20status.tempis = hr20status_temp.tempis;
        hr20status.tempset = hr20status_temp.tempset;
        hr20status.voltage = hr20status_temp.voltage;
        hr20status.time_last = time(NULL);
        g_debug("received sane data: %d %d %d %d %d",
            hr20status.mode,
            hr20status.valve,
            hr20status.tempis,
            hr20status.tempset,
            hr20status.voltage);
    }
    else
    {
        g_debug("received not sane data: %d %d %d %d %d",
            hr20status_temp.mode,
            hr20status_temp.valve,
            hr20status_temp.tempis,
            hr20status_temp.tempset,
            hr20status_temp.voltage);
    }
}

static gboolean serialReceive
(GIOChannel *channel, GIOCondition condition, gpointer data)
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
        hr20_is_initiated = 0;
        g_io_channel_shutdown(channel, 0, NULL);
        close(fd);
        hr20Init();
        return FALSE;
    }
    if(bytes_read > 2048)
    {
        g_warning("read too many bytes?");
        return FALSE;
    }
    buf[bytes_read] = '\0';

    for(i=0; i < bytes_read; i++)
    {
        if(buf[i] == '\n' || hr20status.cmd_position == 1023)
        {
            parse_hr20();
            hr20status.cmd[hr20status.cmd_position] = '\0';
            hr20status.cmd_position = 0;
        }
        else if(buf[i])
        {
            hr20status.cmd[hr20status.cmd_position++] = buf[i];
        }
    }
    return TRUE;
}

static int hr20SerialCommand(char *buffer)
{
    if(buffer)
    {
        g_io_channel_write_chars(hr20status.channel, buffer, strlen(buffer),
          NULL, NULL);
        g_io_channel_flush(hr20status.channel, NULL);
        return 0;
    }
    return 1;
}

/*!
 ********************************************************************************
 * hr20SetTemperature
 *
 * set the wanted temperature
 *
 * \param temperature the wanted temperature multiplied with 10. only steps
 *      of 5 are allowed
 * \returns returns 1 on success, 0 on failure
 *******************************************************************************/
int hr20SetTemperature(int temperature)
{
    char buffer[255];

    if(!hr20_is_initiated)
        return -1;
    if(temperature % 5) // temperature may only be XX.5°C
        return 1;
    sprintf(buffer,"A%x\r", temperature/5);
    hr20SerialCommand(buffer);
    g_debug("setting temperature %d",temperature);
    return 0;
}

int hr20SetAutoTemperature(int slot, int temperature)
{
    char buffer[255];

    if(!hr20_is_initiated)
        return -1;

    if(temperature % 5) // temperature may only be XX.5°C
        return 0;

    sprintf(buffer,"S%02x%x\r",slot+1, temperature/5);
    hr20SerialCommand(buffer);
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
    
    if(!hr20_is_initiated)
        return -1;
    time(&rawtime);
    ptm = localtime(&rawtime);

    sprintf(send_string,"Y%02x%02x%02x\r",ptm->tm_year-100, ptm->tm_mon+1, ptm->tm_mday);
    hr20SerialCommand(send_string);
    
    sprintf(send_string,"H%02x%02x%02x\r",ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    hr20SerialCommand(send_string);

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
    if(!hr20_is_initiated)
        return;
    hr20SerialCommand("M00\r");
}

/*!
 ********************************************************************************
 * hr20SetModeAuto
 *
 * set the mode to automatic control
 *******************************************************************************/
void hr20SetModeAuto()
{
    if(!hr20_is_initiated)
        return;
    hr20SerialCommand("M01\r");
}

//static int16_t hr20GetAutoTemperature(int slot)
//{
//    int i;
//    char buffer[255];
//    char response[255];
//    char *result;
//    
//    sprintf(buffer,"\rG%02x\r",slot+1);
//    
//    for(i=0;i<10;i++)
//    {
//        hr20SerialCommand(buffer, response);
//        if(response[0] == 'G' )
//            break;
//        usleep(1000);
//    }
//    if(response[0] == 'G' && response[5] == '=')
//    {
//        result = strtok(response,"=");
//        result = strtok(NULL,"=");
//    
//        return (hexCharToInt(result[0])*16 + hexCharToInt(result[1]))*50;
//    }
//    else
//        return 0;
//}

static int16_t hexCharToInt(char c)
{
    if(c <= 57)
        return c - 48;
    return c - 87;
}

gboolean hr20update()
{
    float temperature_is;
    float temperature_set;
    float valve;
    float voltage;
    time_t rawtime;

    if(!hr20_is_initiated)
        return TRUE;

    if((config.hr20_database_number != 0) && (hr20status.time_last != 0) &&
        (hr20status.time_last != hr20status.time_last_db_update))
    {
        rawtime = hr20status.time_last;
        temperature_is = hr20status.tempis / 100.0;
        temperature_set = hr20status.tempset / 100.0; 
        valve = (float)hr20status.valve;
        voltage = hr20status.voltage / 1000.0;

        databaseInsertTemperature(config.hr20_database_number,0, &temperature_is, rawtime);
        databaseInsertTemperature(config.hr20_database_number,1, &temperature_set, rawtime);
        databaseInsertTemperature(config.hr20_database_number,2, &valve, rawtime);
        databaseInsertTemperature(config.hr20_database_number,3, &voltage, rawtime);

        hr20status.time_last_db_update = hr20status.time_last;
        g_debug("values submitted to database");
    }
    return TRUE;
}

float hr20GetTemperatureIs()
{
    return hr20status.tempis / 100.0;
}

float hr20GetTemperatureSet()
{
    return hr20status.tempset / 100.0;
}

float hr20GetVoltage()
{
    return hr20status.voltage / 1000.0;
}

int   hr20GetValve()
{
    return hr20status.valve;
}

int   hr20GetMode()
{
    return hr20status.mode;
}


