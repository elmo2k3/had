/*
 * Copyright (C) 2007-2010 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
 * \file    had.h
 * \brief   had header file. some constant and struct definitions
 * \author  Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#ifndef __HAD_H__
#define __HAD_H__

#include <glib.h>
#include <inttypes.h>
#include "misc.h"

#define HAD_CONFIG_FILE "/etc/had.conf"

#define ADC_RES 1024

#define MAX_USB_SENSORS 10

/* Experimental discovered values */
#define ADC_MODUL_1 ADC_RES*1.22
#define ADC_MODUL_3 ADC_RES*1.3
#define ADC_MODUL_DEFAULT ADC_RES*1.25

#define GP_PACKET 0
#define GRAPH_PACKET 1
#define GRAPH_PACKET2 2
#define MPD_PACKET 3
#define RGB_PACKET 4
#define RELAIS_PACKET 5

#define MPD_PLAYING 1
#define MPD_RANDOM 2

#define GLCD_ADDRESS 0x30
#define TEMP1_ADDRESS 3
#define BASE_ADDRESS 10

#define SERIAL_CMD_TEMP_INSERT 1
#define SERIAL_CMD_ 1


//extern pthread_t threads[5];

extern int16_t lastTemperature[9][9][2];
extern int16_t lastVoltage[9];
extern GThread *ledMatrixThread;
extern time_t time_had_started;

struct _remote_control_keys
{
    int mpd_play_pause;
    int mpd_random;
    int mpd_prev;
    int mpd_next;
    int hifi_on_off;
    int light_on;
    int light_off[2];
    int red;
    int green;
    int blue;
    int brightlight;
    int music_on_hifi_on;
    int everything_off;
    int red_single[3];
    int green_single[3];
    int blue_single[3];
    int light_single_off[3];
    int ledmatrix_toggle;
    int open_door;
    int dockstar_on;
    int dockstar_off;
};

/** Struct holding all config vars
 * 
 */
struct _config
{
    char password[128];
    int serial_activated; /**< communication to base station activated? */
    char database_server[50]; /**< mysql server, can be hostname or ip */
    char database_user[20]; /**< mysql user */
    char database_password[30]; /**< mysql password */
    char database_database[20]; /**< database name */
    char database_database_ws2000[20]; /**< database name for old style weather station*/
    int database_port; /**< database port. default 3306 */
    
    int mpd_activated; /**< mpd activated */
    char mpd_server[50]; /**< mpd server, hostname or ip */
    char mpd_password[30]; /**< mpd password */
    int mpd_port; /**< mpd port, default 6600 */
    char mpd_fifo_file[1024];
    int mpd_fifo_activated;

    char scrobbler_user[20]; /**< audioscrobbler user */
    char scrobbler_pass[34]; /**< audioscrobbler password hash, see audioscrobbler docu */
    char scrobbler_tmpfile[100]; /**< tempfile. ugly. wont be needed in future versions */
    
    char pid_file[100]; /**< had pid file */
    char logfile[100]; /**< had logfile */
    char tty[255]; /**< serial device */
    int verbosity; /**< verbosity. currently 0 and 9 supported */
    int daemonize; /**< detach from tty, 0 or 1 */

    char led_matrix_ip[50]; /**< ip address of led-matrix-display */
    int led_matrix_port; /**< port of led-matrix-display */
    int led_matrix_activated; /**< led-matrix-display activated, 0 or 1 */
    int scrobbler_activated; /**< audioscrobbler activated, 0 or 1 */
    int led_shift_speed; /**< Shift speed for texts on the led matrix */

    int sms_activated; /**< send sms at several events? */
    char sipgate_user[100]; /**< sipgate.de user for sending sms*/
    char sipgate_pass[100]; /**< sipgate.de password */
    char cellphone[100]; /**< cellphone number for sms (format +4912378877) */

    int hr20_activated; /**< communication with hr20 thermostat activated? */
    char hr20_port[255]; /**< serial port for hr20 communication */
    int hr20_database_activated;
    int hr20_database_number;

    char statefile[100]; /**< had statefile */

    int usbtemp_activated;
    char usbtemp_device_id[MAX_USB_SENSORS][14];
    int usbtemp_device_modul[MAX_USB_SENSORS];
    int usbtemp_device_sensor[MAX_USB_SENSORS];
    int usbtemp_num_devices;

    int digital_input_module;
    int door_sensor_id;
    int window_sensor_id;

    char rfid_port[255];
    int rfid_activated;

    int switch_off_with_security;
    int sms_on_main_door;
    int security_time_to_active;
    int security_time_before_alarm;
    int beep_on_window_open;
    int remote_activated;

    int voltageboard_activated;
    char voltageboard_tty[255];

    struct _remote_control_keys rkeys;
}config;

/**
 * struct holding the params of one light module
 */ 
struct _rgb
{
    uint8_t red; /**< red */
    uint8_t green; /**< green */
    uint8_t blue; /**< blue */
    uint8_t smoothness; /**< smoothness (time for fading from one color to the other */
};

/**
 * struct for getting the current state of had
 */ 
struct _hadState
{
    struct _rgb rgbModuleValues[3]; /**< array holding current values of each light module */
    struct _rgb rgbModuleValuesTemp[3]; /**< array holding temporary values of each light module */
    uint8_t relais_state; /**< state of the relais */
    uint8_t input_state; /**< state of the input port */
    uint16_t last_voltage[3]; /**< last voltage values of rf modules */
    uint8_t scrobbler_user_activated; /**< scrobbler activated? */
    uint8_t ledmatrix_user_activated; /**< ledmatrix activated? */
    uint8_t beep_on_window_left_open; 
    uint8_t beep_on_door_opened;
}hadState;


/**
 * header for packages transmitted to the base station
 *
 * every packet send to the base station has to include it
 */ 
struct headPacket
{
    unsigned char address; /**< to address */
    unsigned char count; /**< how many bytes? payload + 1 byte for command! */
    unsigned char command; /**< what to do with the payload? */
};


/**
 * struct for trasmitting a full graph (y-points) to the glcd module
 */
struct graphPacket
{
    struct headPacket headP; /**< header */
    unsigned char numberOfPoints; /**< how many points? */
    signed char max[2]; /**< max temperature in this interval */
    signed char min[2]; /**< min temperature */
    signed char temperature_history[115]; /**< the y points */
}graphP;


struct __attribute__((packed)) glcdMainPacket
{
    struct headPacket headP;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
    unsigned char day;
    unsigned char month;
    unsigned char year;
    unsigned char weekday;
    int16_t temperature[4];
    unsigned char backlight;
    unsigned char wecker;
    uint8_t hr20_celsius_is;
    uint8_t hr20_decicelsius_is;
    uint8_t hr20_celsius_set;
    uint8_t hr20_decicelsius_set;
    uint8_t hr20_valve;
    uint8_t hr20_mode;
    uint8_t hr20_auto_t[4];
    uint8_t hr20_auto_t_deci[4];
}glcdP;

struct __attribute__((packed)) glcdMpdPacket
{
    struct headPacket headP;
    char title[20];
    char artist[20];
    char album[20];
    uint16_t length;
    uint16_t pos;
    uint8_t status;
}mpdP;

/**
 *
 * struct for trasmitting the light settings to a module
 */ 
struct _rgbPacket
{
    struct headPacket headP; /**< header */
    unsigned char red; /**< red color */
    unsigned char green; /**< green color */
    unsigned char blue; /**< blue color */
    unsigned char smoothness; /**< time for overblending */
};


#endif
