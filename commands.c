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

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include "network.h"
#include "commands.h"
#include "misc.h"
#include "had.h"
#include "base_station.h"
#include "led_routines.h"

struct command
{
    gchar *cmd;
    gint min_params;
    gint max_params;
    enum NetworkClientPermission permission; 
    enum commands_status (*action)(struct client *client, int argc, char **argv);
};

static enum commands_status action_commands(struct client *client, int argc, char **argv);
static enum commands_status action_disconnect(struct client *client, int argc, char **argv);
static enum commands_status action_auth(struct client *client, int argc, char **argv);
static enum commands_status action_get_auth_string(struct client *client, int argc, char **argv);
static enum commands_status action_rgb_blink(struct client *client, int argc, char **argv);
static enum commands_status action_get_temperature(struct client *client, int argc, char **argv);
static enum commands_status action_get_voltage(struct client *client, int argc, char **argv);
static enum commands_status action_get_relais(struct client *client, int argc, char **argv);
static enum commands_status action_open_door(struct client *client, int argc, char **argv);
static enum commands_status action_led_display_text(struct client *client, int argc, char **argv);
static enum commands_status action_set_rgb(struct client *client, int argc, char **argv);
static enum commands_status action_toggle_base_lcd_backlight(struct client *client, int argc, char **argv);
static enum commands_status action_led_matrix_on_off(struct client *client, int argc, char **argv);
static enum commands_status action_led_matrix_toggle(struct client *client, int argc, char **argv);

#define NUM_COMMANDS 14
static struct command commands[] = {
    {"commands", 0, 0,      NETWORK_CLIENT_PERMISSION_NONE, action_commands},
    {"quit", 0, 0,          NETWORK_CLIENT_PERMISSION_NONE, action_disconnect},
    {"auth",2,2,            NETWORK_CLIENT_PERMISSION_NONE, action_auth},
    {"get_auth_string",0,0, NETWORK_CLIENT_PERMISSION_NONE, action_get_auth_string},
    {"blink",0,0, NETWORK_CLIENT_PERMISSION_ADMIN, action_rgb_blink},
    {"get_temperature",2,2, NETWORK_CLIENT_PERMISSION_ADMIN, action_get_temperature},
    {"get_voltage",1,1, NETWORK_CLIENT_PERMISSION_ADMIN, action_get_voltage},
    {"get_relais",0,0, NETWORK_CLIENT_PERMISSION_ADMIN, action_get_relais},
    {"led_display_text",1,2, NETWORK_CLIENT_PERMISSION_ADMIN, action_led_display_text},
    {"base_lcd_backlight",1,1, NETWORK_CLIENT_PERMISSION_ADMIN, action_toggle_base_lcd_backlight},
    {"set_rgb",5,5, NETWORK_CLIENT_PERMISSION_ADMIN, action_set_rgb},
    {"open_door",0,1, NETWORK_CLIENT_PERMISSION_ADMIN, action_open_door},
    {"led_matrix",1,1, NETWORK_CLIENT_PERMISSION_ADMIN, action_led_matrix_on_off},
    {"toggle_lm",0,0, NETWORK_CLIENT_PERMISSION_ADMIN, action_led_matrix_toggle}
    };

static enum commands_status action_rgb_blink(struct client *client, int argc, char **argv)
{
    base_station_rgb_blink_all(3);
    return COMMANDS_OK;
}

static enum commands_status action_led_matrix_toggle(struct client *client, int argc, char **argv)
{
    ledMatrixToggle();
    return COMMANDS_OK;
}

static enum commands_status action_get_auth_string(struct client *client, int argc, char **argv)
{
    network_client_printf(client,"auth_string: %s\r\n",client->random_number);
    return COMMANDS_OK;
}

static enum commands_status action_auth(struct client *client, int argc, char **argv)
{
    client->permission = NETWORK_CLIENT_PERMISSION_ADMIN;
    switch(client->permission)
    {
        case NETWORK_CLIENT_PERMISSION_NONE: 
                    network_client_printf(client,"permission: NONE\r\n"); break;
        case NETWORK_CLIENT_PERMISSION_READ: 
                    network_client_printf(client,"permission: READ\r\n"); break;
        case NETWORK_CLIENT_PERMISSION_ADMIN: 
                    network_client_printf(client,"permission: ADMIN\r\n"); break;
    }
    return COMMANDS_OK;
}

static enum commands_status action_disconnect(struct client *client, int argc, char **argv)
{
    network_client_disconnect(client);
    return COMMANDS_DISCONNECT;
}

static enum commands_status action_commands(struct client *client, int argc, char **argv)
{
    int i;
    network_client_printf(client,"Available commands:\r\n");
    for(i=1; i< NUM_COMMANDS; i++)
    {
        network_client_printf(client,"%s\r\n",commands[i].cmd);
    }
    return COMMANDS_OK;
}

static enum commands_status
action_get_temperature(struct client *client, int argc, char **argv)
{
    int sensor, modul;

    modul = atoi(argv[1]);
    sensor = atoi(argv[2]);

    network_client_printf(client,"temperature: %d %d %d.%d\r\n", modul, sensor,
        lastTemperature[modul][sensor][0], lastTemperature[modul][sensor][1]);
    return COMMANDS_OK;
}

static enum commands_status
action_get_relais(struct client *client, int argc, char **argv)
{
    network_client_printf(client, "relais: %x\r\n",relaisP.port);
    return COMMANDS_OK;
}

static enum commands_status
action_get_voltage(struct client *client, int argc, char **argv)
{
    int modul;

    modul = atoi(argv[1]);

    network_client_printf(client,"voltage: %d %d\r\n", modul,
        lastVoltage[modul]);
    return COMMANDS_OK;
}

static enum commands_status
action_open_door(struct client *client, int argc, char **argv)
{
    if(argc == 1)
    {
        verbose_printf(0,"Opening door for %s\n",argv[1]);
    }
    else
    {
        verbose_printf(0,"Opening door\n");
    }
    open_door();
    return COMMANDS_OK;
}

enum commands_status commands_process(struct client *client, gchar *cmdline)
{
    int i;
    gchar *pos;
    gint ret;
    char *argv[1024] = { NULL };
    int argc;
    
    ret = COMMANDS_FAIL;
    for(i = 0;i < NUM_COMMANDS; i++)
    {
        pos = g_strstr_len(cmdline, -1, commands[i].cmd);
        if(pos == cmdline) // command must be at the beginning of the line
        {
            network_client_printf(client,"command: %s\r\n",commands[i].cmd);
            argc = buffer2array(cmdline, argv, 1024) -1 ;
            if(argc < commands[i].min_params || argc > commands[i].max_params)
                return COMMANDS_FAIL;
            if(client->permission >= commands[i].permission)
                ret = commands[i].action(client, argc, argv);
            else
                ret = COMMANDS_DENIED;
        }
    }
    return ret;
}

static enum commands_status action_led_display_text(struct client *client, int argc, char **argv)
{
    ledPushToStack(argv[1],atoi(argv[2]),argv[3]);

    return COMMANDS_OK;
}

static enum commands_status action_set_rgb(struct client *client, int argc, char **argv)
{
   struct _rgbPacket rgbPacket;

   rgbPacket.headP.address = atoi(argv[1]);
   rgbPacket.red = atoi(argv[2]);
   rgbPacket.green = atoi(argv[3]);
   rgbPacket.blue = atoi(argv[4]);
   rgbPacket.smoothness = atoi(argv[5]);
   sendPacket(&rgbPacket, RGB_PACKET);
    
    if(rgbPacket.headP.address >= 16 && rgbPacket.headP.address < 19)
    {
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].red = rgbPacket.red;
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].green = rgbPacket.green;
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].blue = rgbPacket.blue;
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].smoothness = rgbPacket.smoothness;
    }
    return COMMANDS_OK;
}

static enum commands_status action_toggle_base_lcd_backlight(struct client *client, int argc, char **argv)
{
    if(atoi(argv[1]))
        setBaseLcdOn();
    else
        setBaseLcdOff();
    return COMMANDS_OK;
}

static enum commands_status action_led_matrix_on_off
(struct client *client, int argc, char **argv)
{
    if(atoi(argv[1]))
        ledMatrixStart();
    else
       ledMatrixStop();
    return COMMANDS_OK;
}

