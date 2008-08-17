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

#ifndef __MAIN_H__
#define __MAIN_H__

#include <pthread.h>

#define ADC_RES 1024
#define ADC_MODUL_1 ADC_RES*1.22
#define ADC_MODUL_3 ADC_RES*1.3
#define ADC_MODUL_DEFAULT ADC_RES*1.25

#define GP_PACKET 0
#define GRAPH_PACKET 1
#define GRAPH_PACKET2 2
#define MPD_PACKET 3
#define RGB_PACKET 4
#define RELAIS_PACKET 5

#define GLCD_ADDRESS 7

extern pthread_t threads[2];
extern void hadSIGINT(void);

extern signed char lastTemperature[9][9][2];

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
struct glcdMainPacket
{
	struct headPacket headP;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned char day;
	unsigned char month;
	unsigned char year;
	unsigned char weekday;
	unsigned char temperature[8];
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
}rgbP;

struct _relaisPacket
{
	struct headPacket headP;
	unsigned char port;
}relaisP;


#endif
