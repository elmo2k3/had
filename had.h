/*
 * Copyright (C) 2007-2008 Bjoern Biesenbach <bjoern@bjoern-b.de>
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
 * \file	had.h
 * \brief	had header file. some constant and struct definitions
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#ifndef __HAD_H__
#define __HAD_H__

#include <pthread.h>
#include <inttypes.h>

#define HAD_CONFIG_FILE "/etc/had.conf"

#define ADC_RES 1024

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

#define GLCD_ADDRESS 0x30
#define TEMP1_ADDRESS 3
#define BASE_ADDRESS 10

#define SERIAL_CMD_TEMP_INSERT 1
#define SERIAL_CMD_ 1

#define verbose_printf(X,args...) \
	if(X <= config.verbosity) \
        {\
                printf("%s    ",theTime()); \
                printf(args); \
	}

extern void updateGlcd();

extern pthread_t threads[3];

extern char *theTime(void);

extern int16_t lastTemperature[9][9][2];
extern int16_t lastVoltage[9];

/** Struct holding all config vars
 * 
 */
struct _config
{
	int serial_activated;
	char database_server[50]; /**< mysql server, can be hostname or ip */
	char database_user[20]; /**< mysql user */
	char database_password[30]; /**< mysql password */
	char database_database[20]; /**< database name */
	int database_port; /**< database port. default 3306 */
	
	char mpd_server[50]; /**< mpd server, hostname or ip */
	char mpd_password[30]; /**< mpd password */
	int mpd_port; /**< mpd port, default 6600 */

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

	int sms_activated;
	char sipgate_user[100];
	char sipgate_pass[100];
	char cellphone[100];

	int hr20_activated;
	char hr20_port[255];

	char statefile[100]; /**< had statefile */
}config;

struct _rgb
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t smoothness;
};

struct _hadState
{
	struct _rgb rgbModuleValues[3];
	uint8_t relais_state;
	uint8_t input_state;
	uint16_t last_voltage[3];
	uint8_t scrobbler_user_activated;
	uint8_t ledmatrix_user_activated;
}hadState;


struct headPacket
{
	unsigned char address;
	unsigned char count;
	unsigned char command;
};


/* 23 Bytes */
struct mpdPacket
{
	struct headPacket headP;
	char currentSong[31];
}mpdP;

/* 123 Bytes */
struct graphPacket
{
	struct headPacket headP;
	unsigned char numberOfPoints;
	signed char max[2];
	signed char min[2];
	signed char temperature_history[115];
}graphP;

/* 19 Byte */
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
}glcdP;

/* 6 Byte */
struct _rgbPacket
{
	struct headPacket headP;
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	unsigned char smoothness;
};

struct _relaisPacket
{
	struct headPacket headP;
	unsigned char port;
}relaisP;


#endif
