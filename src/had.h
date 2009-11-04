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
 * \file	had.h
 * \brief	had header file. some constant and struct definitions
 * \author	Bjoern Biesenbach <bjoern at bjoern-b dot de>
*/

#ifndef __HAD_H__
#define __HAD_H__

#include <pthread.h>
#include <inttypes.h>
#include <glib.h>

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

#define SYSTEM_MOUNT_MPD "mount /mnt/usbstick > /dev/null 2>&1; sleep 1; mpd > /dev/null 2>&1"
#define SYSTEM_KILL_MPD "mpd --kill > /dev/null 2>&1;sleep 3; umount /mnt/usbstick > /dev/null 2>&1; sleep 1; sdparm -q -C stop /dev/discs/disc0/generic"
extern void updateGlcd();

extern pthread_t threads[5];
extern pthread_mutex_t mutexLedmatrix;
extern pthread_mutex_t mutexLedmatrixToggle;
extern GMainLoop *had_mainloop;

extern char *theTime(void);

extern int16_t lastTemperature[9][9][2];
extern int16_t lastVoltage[9];

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
	int kill_and_unmount;
};

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
 * struct for transmitting the current artist/title to glcd
 */
struct mpdPacket
{
	struct headPacket headP;
	char currentSong[31];
}mpdP;

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
}glcdP;

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

/**
 * struct for transmitting the setting for the relais port
 */
struct _relaisPacket
{
	struct headPacket headP; /**< header */
	unsigned char port; /**< port setting */
}relaisP;


#endif
