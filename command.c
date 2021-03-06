/*
 * Copyright (C) 2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
 * Copyright (C) 2003-2010 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "command.h"
#include "database.h"
#include "client.h"
#include "had.h"
#include "base_station.h"
#include "led_routines.h"
#include "mpd.h"
#include "tokenizer.h"
#include "string.h"
#include "configfile.h"
#include "voltageboard.h"
#include "can.h"
#include "can_configfile.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "command"

/*
 * The most we ever use is for search/find, and that limits it to the
 * number of tags we can have.  Add one for the command, and one extra
 * to catch errors clients may send us
 */
#define COMMAND_ARGV_MAX    (2+(100*2))

/* if min: -1 don't check args *
 * if max: -1 no max args      */
struct command {
    const char *cmd;
    unsigned permission;
    int min;
    int max;
    enum command_return (*handler)(struct client *client, int argc, char **argv);
};

/* this should really be "need a non-negative integer": */
static const char need_positive[] = "need a positive integer"; /* no-op */
static const char need_range[] = "need a range";

/* FIXME: redundant error messages */
static const char check_integer[] = "\"%s\" is not a integer";
static const char need_integer[] = "need an integer";

static const char *current_command;
static int command_list_num;

static enum command_return
action_commands(struct client *client,
        G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[]);
static bool G_GNUC_PRINTF(4, 5)
check_int(struct client *client, int *value_r,
      const char *s, const char *fmt, ...);
static bool
check_bool(struct client *client, bool *value_r, const char *s);

static enum command_return action_rgb_blink(struct client *client, int argc, char **argv)
{
    base_station_rgb_blink_all(3);
    return COMMAND_RETURN_OK;
}

static enum command_return action_led_matrix_toggle(struct client *client, int argc, char **argv)
{
    ledMatrixToggle();
    return COMMAND_RETURN_OK;
}

static enum command_return action_disconnect(struct client *client, int argc, char **argv)
{
//    network_client_disconnect(client);
    return COMMAND_RETURN_CLOSE;
}

static enum command_return
action_get_temperature(struct client *client, int argc, char **argv)
{
    int sensor, modul;

    if (!check_int(client, &sensor, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &modul, argv[2], need_positive))
        return COMMAND_RETURN_ERROR;

    client_printf(client,"temperature: %d %d %d.%d\r\n", modul, sensor,
        lastTemperature[modul][sensor]/10, lastTemperature[modul][sensor]%10);
    return COMMAND_RETURN_OK;
}

static enum command_return action_led_display_text(struct client *client, int argc, char **argv)
{
    int count;
    bool shift;
    
    if (!check_bool(client, &shift, argv[2]))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &count, argv[3], need_positive))
        return COMMAND_RETURN_ERROR;
    ledFifoInsert(argv[1],shift,count);

    return COMMAND_RETURN_OK;
}

static enum command_return action_set_rgb_all
(struct client *client, int argc, char **argv)
{
    int red,green,blue,smoothness;

    if (!check_int(client, &red, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &green, argv[2], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &blue, argv[3], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &smoothness, argv[4], need_positive))
        return COMMAND_RETURN_ERROR;
    for(int i= 0;i < 3; i++)
    {
        hadState.rgbModuleValues[i].red = (unsigned char)red;
        hadState.rgbModuleValues[i].green = (unsigned char)green;
        hadState.rgbModuleValues[i].blue = (unsigned char)blue;
        hadState.rgbModuleValues[i].smoothness = (unsigned char)smoothness;
    }
    setCurrentRgbValues();
    return COMMAND_RETURN_OK;
}

static enum command_return action_set_rgb(struct client *client, int argc, char **argv)
{
    struct _rgbPacket rgbPacket;
    int red,green,blue,smoothness,address;

    if (!check_int(client, &address, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &red, argv[2], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &green, argv[3], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &blue, argv[4], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &smoothness, argv[5], need_positive))
        return COMMAND_RETURN_ERROR;

    rgbPacket.headP.address = (unsigned char)address;
    rgbPacket.red = (unsigned char)red;
    rgbPacket.green = (unsigned char)green;
    rgbPacket.blue = (unsigned char)blue;
    rgbPacket.smoothness = (unsigned char)smoothness;
    sendPacket(&rgbPacket, RGB_PACKET);

    if(rgbPacket.headP.address >= 16 && rgbPacket.headP.address < 19)
    {
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].red = rgbPacket.red;
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].green = rgbPacket.green;
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].blue = rgbPacket.blue;
        hadState.rgbModuleValues[rgbPacket.headP.address-0x10].smoothness = rgbPacket.smoothness;
    }
    return COMMAND_RETURN_OK;
}

static enum command_return action_toggle_base_lcd_backlight(struct client *client, int argc, char **argv)
{
    bool on;
    if (!check_bool(client, &on, argv[1]))
        return COMMAND_RETURN_ERROR;
    if(on)
        setBaseLcdOn();
    else
        setBaseLcdOff();
    return COMMAND_RETURN_OK;
}

static enum command_return action_hifi_on_music_on
(struct client *client, int argc, char **argv)
{
    base_station_music_on_hifi_on();
    return COMMAND_RETURN_OK;
}

static enum command_return action_led_matrix_on_off
(struct client *client, int argc, char *argv[])
{
    bool on;
    if (!check_bool(client, &on, argv[1]))
        return COMMAND_RETURN_ERROR;
    if(on)
    {
        ledMatrixStart();
    }
    else
    {
       ledMatrixStop();
    }
    return COMMAND_RETURN_OK;
}

static enum command_return action_set_hifi
(struct client *client, int argc, char *argv[])
{
    bool on;
    if (!check_bool(client, &on, argv[1]))
        return COMMAND_RETURN_ERROR;
    if(on)
        base_station_hifi_on();
    else
        base_station_hifi_off();
    return COMMAND_RETURN_OK;
}

void command_success(struct client *client)
{
    client_puts(client, "OK\n");
}

static bool
command_available(G_GNUC_UNUSED const struct command *cmd)
{
    return TRUE;
}

static void command_error_v(struct client *client, enum ack error,
                const char *fmt, va_list args)
{
    assert(client != NULL);
    assert(current_command != NULL);

    client_printf(client, "ACK [%i@%i] {%s} ",
              (int)error, command_list_num, current_command);
    client_vprintf(client, fmt, args);
    client_puts(client, "\n");

    current_command = NULL;
}

G_GNUC_PRINTF(3, 4) static void command_error(struct client *client, enum ack error,
                       const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    command_error_v(client, error, fmt, args);
    va_end(args);
}

static bool G_GNUC_PRINTF(4, 5)
check_int(struct client *client, int *value_r,
      const char *s, const char *fmt, ...)
{
    char *test;
    long value;

    value = strtol(s, &test, 10);
    if (*test != '\0') {
        va_list args;
        va_start(args, fmt);
        command_error_v(client, ACK_ERROR_ARG, fmt, args);
        va_end(args);
        return false;
    }

#if G_MAXLONG > G_MAXINT
    if (value < G_MININT || value > G_MAXINT) {
        command_error(client, ACK_ERROR_ARG,
                  "Number too large: %s", s);
        return false;
    }
#endif

    *value_r = (int)value;
    return true;
}

G_GNUC_UNUSED static bool G_GNUC_PRINTF(5, 6)
check_range(struct client *client, unsigned *value_r1, unsigned *value_r2,
        const char *s, const char *fmt, ...)
{
    char *test, *test2;
    long value;

    value = strtol(s, &test, 10);
    if (*test != '\0' && *test != ':') {
        va_list args;
        va_start(args, fmt);
        command_error_v(client, ACK_ERROR_ARG, fmt, args);
        va_end(args);
        return false;
    }

    if (value == -1 && *test == 0) {
        /* compatibility with older MPD versions: specifying
           "-1" makes MPD display the whole list */
        *value_r1 = 0;
        *value_r2 = G_MAXUINT;
        return true;
    }

    if (value < 0) {
        command_error(client, ACK_ERROR_ARG,
                  "Number is negative: %s", s);
        return false;
    }

#if G_MAXLONG > G_MAXUINT
    if (value > G_MAXUINT) {
        command_error(client, ACK_ERROR_ARG,
                  "Number too large: %s", s);
        return false;
    }
#endif

    *value_r1 = (unsigned)value;

    if (*test == ':') {
        value = strtol(++test, &test2, 10);
        if (*test2 != '\0') {
            va_list args;
            va_start(args, fmt);
            command_error_v(client, ACK_ERROR_ARG, fmt, args);
            va_end(args);
            return false;
        }

        if (test == test2)
            value = G_MAXUINT;

        if (value < 0) {
            command_error(client, ACK_ERROR_ARG,
                      "Number is negative: %s", s);
            return false;
        }

#if G_MAXLONG > G_MAXUINT
        if (value > G_MAXUINT) {
            command_error(client, ACK_ERROR_ARG,
                      "Number too large: %s", s);
            return false;
        }
#endif
        *value_r2 = (unsigned)value;
    } else {
        *value_r2 = (unsigned)value + 1;
    }

    return true;
}

G_GNUC_UNUSED static bool
check_unsigned(struct client *client, unsigned *value_r, const char *s)
{
    unsigned long value;
    char *endptr;

    value = strtoul(s, &endptr, 10);
    if (*endptr != 0) {
        command_error(client, ACK_ERROR_ARG,
                  "Integer expected: %s", s);
        return false;
    }

    if (value > G_MAXUINT) {
        command_error(client, ACK_ERROR_ARG,
                  "Number too large: %s", s);
        return false;
    }

    *value_r = (unsigned)value;
    return true;
}

static bool
check_bool(struct client *client, bool *value_r, const char *s)
{
    long value;
    char *endptr;

    value = strtol(s, &endptr, 10);
    if (*endptr != 0 || (value != 0 && value != 1)) {
        command_error(client, ACK_ERROR_ARG,
                  "Boolean (0/1) expected: %s", s);
        return false;
    }

    *value_r = !!value;
    return true;
}

static enum command_return
action_beep(struct client *client,
        int argc, char *argv[])
{
    int count, time, pause;
    if (argc == 1) {
        base_station_beep(1,1000,0);
    } else if (argc == 4) {
        if (!check_int(client, &count, argv[1], need_positive))
            return COMMAND_RETURN_ERROR;
        if (!check_int(client, &time, argv[2], need_positive))
            return COMMAND_RETURN_ERROR;
        if (!check_int(client, &pause, argv[3], need_positive))
            return COMMAND_RETURN_ERROR;
        base_station_beep(count,time,pause);
    }
    else
        return COMMAND_RETURN_ERROR;
    return COMMAND_RETURN_OK;
}

static enum command_return
action_set_sleep_light(struct client *client,
        int argc, char *argv[])
{
    bool on;
    if (!check_bool(client, &on, argv[1]))
        return COMMAND_RETURN_ERROR;
    if(on)
        base_station_sleep_light_on();
    else
        base_station_sleep_light_off();
    return COMMAND_RETURN_OK;
}


static enum command_return
action_set_printer(struct client *client,
        int argc, char *argv[])
{
    bool on;
    if (!check_bool(client, &on, argv[1]))
        return COMMAND_RETURN_ERROR;
    if(on)
        base_station_printer_on();
    else
        base_station_printer_off();
    return COMMAND_RETURN_OK;
}

static enum command_return
action_get_hifi(struct client *client,
        int argc, char *argv[])
{
    client_printf(client, "hifi: %s\n",
        base_station_hifi_is_on() ? "on":"off");
    return COMMAND_RETURN_OK;
}

static enum command_return
action_get_sleep_light(struct client *client,
        int argc, char *argv[])
{
    client_printf(client, "sleep_light: %s\n",
        base_station_sleep_light_is_on() ? "on":"off");
    return COMMAND_RETURN_OK;
}

static enum command_return
action_uptime(struct client *client,
        int argc, char *argv[])
{
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR (SECONDS_PER_MINUTE*60)
#define SECONDS_PER_DAY (24*SECONDS_PER_HOUR)
    time_t diff_time = time(NULL) - time_had_started;
    time_t days, hours, minutes, seconds;

    days = diff_time / SECONDS_PER_DAY;
    diff_time -= days*SECONDS_PER_DAY;
    hours = diff_time / SECONDS_PER_HOUR;
    diff_time -= hours*SECONDS_PER_HOUR;
    minutes = diff_time / SECONDS_PER_MINUTE;
    diff_time -= minutes*SECONDS_PER_MINUTE;
    seconds = diff_time;

    client_printf(client, "uptime: %d days, %02d:%02d:%02d\n",
        (int)days,(int)hours,(int)minutes,(int)seconds);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_get_printer(struct client *client,
        int argc, char *argv[])
{
    client_printf(client, "printer: %s\n",
        base_station_printer_is_on() ? "on":"off");
    return COMMAND_RETURN_OK;
}

static enum command_return
action_all_off(struct client *client,
        int argc, char *argv[])
{
    base_station_everything_off();
    return COMMAND_RETURN_OK;
}

static enum command_return
action_sent_graph(struct client *client,
        int argc, char *argv[])
{
    getDailyGraph(3,1,&graphP);
    sendPacket(&graphP, GRAPH_PACKET);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_led_matrix_select_screen(struct client *client,
        int argc, char *argv[])
{
    int screen_num;
    if (!check_int(client, &screen_num, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    ledMatrixSelectScreen(screen_num);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_config_load(struct client *client,
        int argc, char *argv[])
{
    readConfig();
    return COMMAND_RETURN_OK;
}

static enum command_return
action_config_save(struct client *client,
        int argc, char *argv[])
{
    writeConfig();
    return COMMAND_RETURN_OK;
}

static enum command_return
action_config_set(struct client *client,
        int argc, char *argv[])
{
    if(setConfigValueByName(argv[1],argv[2]) != 0)
        return COMMAND_RETURN_ERROR;
    return COMMAND_RETURN_OK;
}

static enum command_return
action_config_get(struct client *client,
        int argc, char *argv[])
{
    printConfig((void*)client_printf,(void*)client);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_get_log(struct client *client,
        int argc, char *argv[])
{
    FILE *logfile = fopen(config.logfile,"r");
    char linebuf[1024];

    if(!logfile)
        return COMMAND_RETURN_ERROR;
    while(fgets(linebuf, sizeof(linebuf), logfile))
    {
        client_printf(client,"%s",linebuf);
    }
    return COMMAND_RETURN_OK;
}

static enum command_return
action_led_matrix_get_screen(struct client *client,
        int argc, char *argv[])
{
    client_printf(client, "current screen: %d\r\n",
        ledMatrixCurrentScreen());
    return COMMAND_RETURN_OK;
}

static enum command_return
action_dockstar_off(struct client *client,
        int argc, char *argv[])
{
    voltageboard_dockstar_off();
    return COMMAND_RETURN_OK;
}

static enum command_return
action_dockstar_on(struct client *client,
        int argc, char *argv[])
{
    voltageboard_dockstar_on();
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_get_node(struct client *client,
        int argc, char *argv[])
{
    int address;
    struct CanNode *node;
    int days, hours, minutes, seconds, uptime;
    if (!check_int(client, &address, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    node = can_get_node(address);
    uptime = node->uptime;
    days = uptime/(SECONDS_PER_HOUR*24); uptime -= days*SECONDS_PER_HOUR*24;
    hours = uptime/SECONDS_PER_HOUR; uptime -= hours*SECONDS_PER_HOUR;
    minutes = uptime/SECONDS_PER_MINUTE; uptime -= minutes*SECONDS_PER_MINUTE;
    seconds = uptime;
    client_printf(client, "address: %d\r\n",address);
    client_printf(client, "uptime: %d %02d:%02d:%02d\r\n",
        days, hours, minutes, seconds);
    client_printf(client, "relais: %d\r\n",node->relais_state);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_get_nodes(struct client *client,
        int argc, char *argv[])
{
    int address;
    int i;
    struct CanNode *node;
    int days, hours, minutes, seconds, uptime;

    for(i=0;i<255;i++)
    {
        node = can_get_node(i);
        if(node->time_last_active > time(NULL)-10)
        {
            uptime = node->uptime;
            days = uptime/(SECONDS_PER_HOUR*24); uptime -= days*SECONDS_PER_HOUR*24;
            hours = uptime/SECONDS_PER_HOUR; uptime -= hours*SECONDS_PER_HOUR;
            minutes = uptime/SECONDS_PER_MINUTE; uptime -= minutes*SECONDS_PER_MINUTE;
            seconds = uptime;
            client_printf(client, "address: %d\r\n",i);
            client_printf(client, "name: %s\r\n",can_config_get_node_name(i));
            client_printf(client, "uptime: %d %02d:%02d:%02d\r\n",
                days, hours, minutes, seconds);
            client_printf(client, "relais: %d\r\n",node->relais_state);
            client_printf(client, "relais1: %d %s\r\n",
                node->relais_state & 0x01 ? 1:0, can_config_get_relais_name(i,1));
            client_printf(client, "relais2: %d %s\r\n",
                node->relais_state & 0x02 ? 1:0, can_config_get_relais_name(i,2));
            client_printf(client, "relais3: %d %s\r\n",
                node->relais_state & 0x04 ? 1:0, can_config_get_relais_name(i,3));
            client_printf(client, "relais4: %d %s\r\n",
                node->relais_state & 0x08 ? 1:0, can_config_get_relais_name(i,4));
            client_printf(client, "hr20_data_valid: %d\r\n",node->hr20_state.data_valid);
            client_printf(client, "hr20_mode: %d\r\n",node->hr20_state.mode);
            client_printf(client, "hr20_window: %d\r\n",node->hr20_state.window_open);
            client_printf(client, "hr20_tempis: %d\r\n",node->hr20_state.tempis);
            client_printf(client, "hr20_tempset: %d\r\n",node->hr20_state.tempset);
            client_printf(client, "hr20_valve: %d\r\n",node->hr20_state.valve);
            client_printf(client, "hr20_voltage: %d\r\n",node->hr20_state.voltage);
            client_printf(client, "hr20_error_code: %d\r\n",node->hr20_state.error_code);
            client_printf(client, "hr20_data_age: %d\r\n",node->hr20_state.data_timestamp);
            client_printf(client, "version: %d\r\n",node->version);
            client_printf(client, "voltage: %d\r\n",node->voltage);
        }
    }
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_get_nodes_json(struct client *client,
        int argc, char *argv[])
{
    /*
     * {
     *    "name": "Schlafzimmer",
     *    "uptime": "25 15:01:23",
     *    "relais":[{"state":0, "name":""},{ "state":1, "name":"Lampe"},{ "state":0, "name":""},{ "state":0, "name":""}]
     * }
     * */
    int address;
    int i;
    struct CanNode *node;
    int first = 1;
    
    client_printf(client, "{\r\n");

    for(i=0;i<255;i++)
    {
        node = can_get_node(i);
        if(node->time_last_active > time(NULL)-10)
        {
            if(!first)
            {
                client_printf(client, ",\r\n");
            }
            first = 0;
            client_printf(client, "\"%d\":{\r\n",i);
            client_printf(client, "\"name\": \"%s\",\r\n",can_config_get_node_name(i));
            client_printf(client, "\"uptime\": %d,\r\n",node->uptime);
            client_printf(client, "\"show_in_ui\": \"%d\",\r\n",can_config_get_show_in_ui(i));
            client_printf(client, "\"relais\":[");
            client_printf(client, "{\"state\": %d, \"name\": \"%s\"},",
                node->relais_state & 0x01 ? 1:0, can_config_get_relais_name(i,1));
            client_printf(client, "{\"state\": %d, \"name\": \"%s\"},",
                node->relais_state & 0x02 ? 1:0, can_config_get_relais_name(i,2));
            client_printf(client, "{\"state\": %d, \"name\": \"%s\"},",
                node->relais_state & 0x04 ? 1:0, can_config_get_relais_name(i,3));
            client_printf(client, "{\"state\": %d, \"name\": \"%s\"}]\r\n,",
                node->relais_state & 0x08 ? 1:0, can_config_get_relais_name(i,4));
            client_printf(client, "\"hr20_data_valid\": %d,\r\n",node->hr20_state.data_valid);
            client_printf(client, "\"hr20_mode\": %d,\r\n",node->hr20_state.mode);
            client_printf(client, "\"hr20_window\": %d,\r\n",node->hr20_state.window_open);
            client_printf(client, "\"hr20_tempis\": %d,\r\n",node->hr20_state.tempis);
            client_printf(client, "\"hr20_tempset\": %d,\r\n",node->hr20_state.tempset);
            client_printf(client, "\"hr20_valve\": %d,\r\n",node->hr20_state.valve);
            client_printf(client, "\"hr20_voltage\": %d,\r\n",node->hr20_state.voltage);
            client_printf(client, "\"hr20_error_code\": %d,\r\n",node->hr20_state.error_code);
            client_printf(client, "\"hr20_data_age\": %d,\r\n",node->hr20_state.data_timestamp);
            client_printf(client, "\"version\": %d,\r\n",node->version);
            client_printf(client, "\"voltage\": %d\r\n}\r\n",node->voltage);
        }
    }
    client_printf(client, "}\r\n");
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_set_relais(struct client *client,
        int argc, char *argv[])
{
    int address;
    int relais;
    bool relais_state;
    if (!check_int(client, &address, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &relais, argv[2], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_bool(client, &relais_state, argv[3]))
        return COMMAND_RETURN_ERROR;
    can_set_relais(address, relais, (int)relais_state);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_set_temperature(struct client *client,
        int argc, char *argv[])
{
    int address;
    int temperature;
    if (!check_int(client, &address, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &temperature, argv[2], need_positive))
        return COMMAND_RETURN_ERROR;
    can_set_temperature(address, temperature);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_set_date(struct client *client,
        int argc, char *argv[])
{
    int address;
    if (!check_int(client, &address, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    can_set_date(address);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_set_mode_manu(struct client *client,
        int argc, char *argv[])
{
    int address;
    if (!check_int(client, &address, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    can_set_mode_manu(address);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_set_mode_auto(struct client *client,
        int argc, char *argv[])
{
    int address;
    if (!check_int(client, &address, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    can_set_mode_auto(address);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_hands_on_device(struct client *client,
        int argc, char *argv[])
{
    canHandsOnDevice(); 
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_hands_off_device(struct client *client,
        int argc, char *argv[])
{
    canHandsOffDevice(); 
    return COMMAND_RETURN_OK;
}

static enum command_return
action_can_toggle_relais(struct client *client,
        int argc, char *argv[])
{
    int address;
    int relais;
    if (!check_int(client, &address, argv[1], need_positive))
        return COMMAND_RETURN_ERROR;
    if (!check_int(client, &relais, argv[2], need_positive))
        return COMMAND_RETURN_ERROR;
    can_toggle_relais(address, relais);
    return COMMAND_RETURN_OK;
}

static enum command_return
action_voltageboard_get(struct client *client,
        int argc, char *argv[])
{
    client_printf(client, "voltage:       %0.1fV\r\n", voltageboard_get_voltage_total()/100.0);
    client_printf(client, "current:       %0.1fA\r\n", voltageboard_get_amps_total()/1000.0);
    client_printf(client, "power:         %dW\r\n", voltageboard_get_power_total());
    client_printf(client, "voltage 5V 1:  %0.1fV\r\n", voltageboard_get_voltage_5v_1()/100.0);
    client_printf(client, "voltage 5V 2:  %0.1fV\r\n", voltageboard_get_voltage_5v_2()/100.0);
    client_printf(client, "5V 1 on:       %d\r\n", voltageboard_get_power_on_5v_1());
    client_printf(client, "5V 2 on:       %d\r\n", voltageboard_get_power_on_5v_2());
    client_printf(client, "Relais on:     %d\r\n", voltageboard_get_relais());
    client_printf(client, "Relais bat on: %d\r\n", voltageboard_get_relais_bat());
    return COMMAND_RETURN_OK;
}

/**
 * The command registry.
 *
 * This array must be sorted!
 */
static const struct command commands[] = {
    {"all_off",         PERMISSION_ADMIN, 0,0, action_all_off},
    {"beep",            PERMISSION_ADMIN, 0,3, action_beep},
    {"blink",           PERMISSION_ADMIN, 0,0, action_rgb_blink},
    {"can_get_json",   PERMISSION_ADMIN, 0,0, action_can_get_nodes_json},
    {"can_get_node",    PERMISSION_ADMIN, 1,1, action_can_get_node},
    {"can_get_nodes",   PERMISSION_ADMIN, 0,0, action_can_get_nodes},
    {"can_handsoffdevice",   PERMISSION_ADMIN, 0,0, action_can_hands_off_device},
    {"can_handsondevice",   PERMISSION_ADMIN, 0,0, action_can_hands_on_device},
    {"can_set_date",    PERMISSION_ADMIN, 1,1, action_can_set_date},
    {"can_set_mode_auto",PERMISSION_ADMIN, 1,1, action_can_set_mode_auto},
    {"can_set_mode_manu",PERMISSION_ADMIN, 1,1, action_can_set_mode_manu},
    {"can_set_relais",  PERMISSION_ADMIN, 3,3, action_can_set_relais},
    {"can_set_temperature",  PERMISSION_ADMIN, 2,2, action_can_set_temperature},
    {"can_toggle_relais",  PERMISSION_ADMIN, 2,2, action_can_toggle_relais},
    {"commands",        PERMISSION_ADMIN, 0,0, action_commands},
    {"config_get",      PERMISSION_ADMIN, 0,0, action_config_get},
    {"config_load",     PERMISSION_ADMIN, 0,0, action_config_load},
    {"config_save",     PERMISSION_ADMIN, 0,0, action_config_save},
    {"config_set",      PERMISSION_ADMIN, 2,2, action_config_set},
    {"dockstar_off",    PERMISSION_ADMIN, 0,0, action_dockstar_off},
    {"dockstar_on",     PERMISSION_ADMIN, 0,0, action_dockstar_on},
    {"get_hifi",        PERMISSION_ADMIN, 0,0, action_get_hifi},
    {"get_log",         PERMISSION_ADMIN, 0,0, action_get_log},
    {"get_printer",     PERMISSION_ADMIN, 0,0, action_get_printer},
    {"get_sleep_light", PERMISSION_ADMIN, 0,0, action_get_sleep_light},
    {"get_temperature", PERMISSION_ADMIN, 2,2, action_get_temperature},
    {"hifi_on",         PERMISSION_ADMIN, 0,0, action_hifi_on_music_on},
    {"lm",              PERMISSION_ADMIN, 1,1, action_led_matrix_on_off},
    {"lm_get_screen",   PERMISSION_ADMIN, 0,0, action_led_matrix_get_screen},
    {"lm_select_screen",PERMISSION_ADMIN, 1,1, action_led_matrix_select_screen},
    {"lm_text",         PERMISSION_ADMIN, 3,3, action_led_display_text},
    {"lm_toggle",       PERMISSION_ADMIN, 0,0, action_led_matrix_toggle},
    {"quit",            PERMISSION_ADMIN, 0,0, action_disconnect},
    {"sent_graph",      PERMISSION_ADMIN, 0,0, action_sent_graph},
    {"set_backlight",   PERMISSION_ADMIN, 1,1, action_toggle_base_lcd_backlight},
    {"set_hifi",        PERMISSION_ADMIN, 1,1, action_set_hifi},
    {"set_printer",     PERMISSION_ADMIN, 1,1, action_set_printer},
    {"set_rgb",         PERMISSION_ADMIN, 5,5, action_set_rgb},
    {"set_rgb_all",     PERMISSION_ADMIN, 4,4, action_set_rgb_all},
    {"set_sleep_light", PERMISSION_ADMIN, 1,1, action_set_sleep_light},
    {"uptime",          PERMISSION_ADMIN, 0,0, action_uptime},
    {"voltageboard_get",PERMISSION_ADMIN, 0,0, action_voltageboard_get}
};

static const unsigned num_commands = sizeof(commands) / sizeof(commands[0]);

/* don't be fooled, this is the command handler for "commands" command */
static enum command_return
action_commands(struct client *client,
        G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
    const unsigned permission = client_get_permission(client);
    const struct command *cmd;

    for (unsigned i = 0; i < num_commands; ++i) {
        cmd = &commands[i];

        if (cmd->permission == (permission & cmd->permission) &&
            command_available(cmd))
            client_printf(client, "command: %s\n", cmd->cmd);
    }

    return COMMAND_RETURN_OK;
}

void command_init(void)
{
#ifndef NDEBUG
    /* ensure that the command list is sorted */
    for (unsigned i = 0; i < num_commands - 1; ++i)
        assert(strcmp(commands[i].cmd, commands[i + 1].cmd) < 0);
#endif
}

void command_finish(void)
{
}

static const struct command *
command_lookup(const char *name)
{
    unsigned a = 0, b = num_commands, i;
    int cmp;

    /* binary search */
    do {
        i = (a + b) / 2;

        cmp = strcmp(name, commands[i].cmd);
        if (cmp == 0)
            return &commands[i];
        else if (cmp < 0)
            b = i;
        else if (cmp > 0)
            a = i + 1;
    } while (a < b);

    return NULL;
}

static bool
command_check_request(const struct command *cmd, struct client *client,
              unsigned permission, int argc, char *argv[])
{
    int min = cmd->min + 1;
    int max = cmd->max + 1;

    if (cmd->permission != (permission & cmd->permission)) {
        if (client != NULL)
            command_error(client, ACK_ERROR_PERMISSION,
                      "you don't have permission for \"%s\"",
                      cmd->cmd);
        return false;
    }

    if (min == 0)
        return true;

    if (min == max && max != argc) {
        if (client != NULL)
            command_error(client, ACK_ERROR_ARG,
                      "wrong number of arguments for \"%s\"",
                      argv[0]);
        return false;
    } else if (argc < min) {
        if (client != NULL)
            command_error(client, ACK_ERROR_ARG,
                      "too few arguments for \"%s\"", argv[0]);
        return false;
    } else if (argc > max && max /* != 0 */ ) {
        if (client != NULL)
            command_error(client, ACK_ERROR_ARG,
                      "too many arguments for \"%s\"", argv[0]);
        return false;
    } else
        return true;
}

static const struct command *
command_checked_lookup(struct client *client, unsigned permission,
               int argc, char *argv[])
{
    static char unknown[] = "";
    const struct command *cmd;

    current_command = unknown;

    if (argc == 0)
        return NULL;

    cmd = command_lookup(argv[0]);
    if (cmd == NULL) {
        if (client != NULL)
            command_error(client, ACK_ERROR_UNKNOWN,
                      "unknown command \"%s\"", argv[0]);
        return NULL;
    }

    current_command = cmd->cmd;

    if (!command_check_request(cmd, client, permission, argc, argv))
        return NULL;

    return cmd;
}

enum command_return
command_process(struct client *client, unsigned num, char *line)
{
    GError *error = NULL;
    int argc;
    char *argv[COMMAND_ARGV_MAX] = { NULL };
    const struct command *cmd;
    enum command_return ret = COMMAND_RETURN_ERROR;

    command_list_num = num;

    /* get the command name (first word on the line) */

    argv[0] = tokenizer_next_word(&line, &error);
    if (argv[0] == NULL) {
        current_command = "";
        if (*line == 0)
            command_error(client, ACK_ERROR_UNKNOWN,
                      "No command given");
        else {
            command_error(client, ACK_ERROR_UNKNOWN,
                      "%s", error->message);
            g_error_free(error);
        }
        current_command = NULL;

        return COMMAND_RETURN_ERROR;
    }

    argc = 1;

    /* now parse the arguments (quoted or unquoted) */

    while (argc < (int)G_N_ELEMENTS(argv) &&
           (argv[argc] =
        tokenizer_next_param(&line, &error)) != NULL)
        ++argc;

    /* some error checks; we have to set current_command because
       command_error() expects it to be set */

    current_command = argv[0];

    if (argc >= (int)G_N_ELEMENTS(argv)) {
        command_error(client, ACK_ERROR_ARG, "Too many arguments");
        current_command = NULL;
        return COMMAND_RETURN_ERROR;
    }

    if (*line != 0) {
        command_error(client, ACK_ERROR_ARG,
                  "%s", error->message);
        current_command = NULL;
        g_error_free(error);
        return COMMAND_RETURN_ERROR;
    }

    /* look up and invoke the command handler */

    cmd = command_checked_lookup(client, client_get_permission(client),
                     argc, argv);
    if (cmd)
        ret = cmd->handler(client, argc, argv);

    current_command = NULL;
    command_list_num = 0;

    return ret;
}
